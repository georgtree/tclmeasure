#include "tclmeasureInput.h"
#include <stdint.h>
#include <string.h>

#ifdef HAVE_RBC
#include <rbcVector.h>
#include <rbcDecls.h>
#ifndef Rbc_VectorCheckWritable
#error "RBC 0.8.1 or newer stubs headers are required"
#endif
/* RBC installs this portable stub client source alongside its public headers. */
#include <rbcStubLib.c>
#endif

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * MeasureInitVectors --
 *
 *      Initializes the optional RBC vector API in the current interpreter. Call once at command entry, before
 *      borrowing any input storage: package loading may evaluate Tcl. A process-wide stub pointer alone does not
 *      establish that RBC is loaded in this interpreter. List commands never call this function.
 *
 * Results:
 *      TCL_OK on success; TCL_ERROR with a package-loading or unsupported-build message otherwise.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
int MeasureInitVectors(Tcl_Interp *interp) {
#ifdef HAVE_RBC
    if (Rbc_VectorInitStubs(interp, "0.8.1", 0) == NULL) {
        if (Tcl_GetStringResult(interp)[0] == '\0') {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("RBC vector stubs are unavailable", -1));
        }
        return TCL_ERROR;
    }
    return TCL_OK;
#else
    Tcl_SetObjResult(interp, Tcl_NewStringObj("tclmeasure was built without RBC vector support", -1));
    return TCL_ERROR;
#endif
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * MeasureOpenInput --
 *
 *      Opens obj as a list, or as a vector name when vectors is nonzero. Vector callers must first initialize
 *      stubs with MeasureInitVectors. Returned storage is borrowed, read-only and valid only while the source
 *      remains unchanged. RBC read-only vectors are valid inputs: never require write permission or unlock them.
 *      Vector offsets do not affect sample positions. Complex vectors are rejected.
 *
 * Results:
 *      TCL_OK with input initialized, or TCL_ERROR with an explanatory interpreter result. No buffers or vectors
 *      are allocated, and no cleanup is required. Does not evaluate Tcl or dispatch notifications.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
int MeasureOpenInput(Tcl_Interp *interp, Tcl_Obj *obj, int vectors, MeasureInput *input) {
    input->length = 0;
    input->elements = NULL;
    input->values = NULL;
    input->square = 0;
    if (!vectors) {
        return Tcl_ListObjGetElements(interp, obj, &input->length, &input->elements);
    }
#ifdef HAVE_RBC
    Rbc_Vector *vector;
    if (Rbc_GetVector(interp, Tcl_GetString(obj), &vector) != TCL_OK) {
        return TCL_ERROR;
    }
    if (Rbc_VectorGetType(vector) != RBC_VECTOR_REAL) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("vector \"%s\" must contain real values", Tcl_GetString(obj)));
        return TCL_ERROR;
    }
    input->length = Rbc_VectorLength(vector);
    input->values = Rbc_VectorData(vector);
    return TCL_OK;
#else
    return MeasureInitVectors(interp);
#endif
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * MeasureVectorSupportCmd --
 *
 *      Checks and initializes vector support for the Tcl wrapper before it resolves input names or reads default
 *      endpoints. Takes no arguments. Returns TCL_OK with an empty result, or TCL_ERROR from MeasureInitVectors.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
int MeasureVectorSupportCmd(void *clientData, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[]) {
    if (objc != 1) {
        Tcl_WrongNumArgs(interp, 1, objv, "");
        return TCL_ERROR;
    }
    if (MeasureInitVectors(interp) != TCL_OK) {
        return TCL_ERROR;
    }
    Tcl_ResetResult(interp);
    return TCL_OK;
}

#ifdef HAVE_RBC
/* Numeric copies are complete before any destination command can evaluate Tcl. */
typedef struct {
    const char *key;
    Tcl_Size n;
    double *v;
    Tcl_Obj *name;
    Tcl_Obj *autoName;
    Tcl_Command created;
} Output;

typedef struct {
    Tcl_Interp *ip;
    Tcl_Obj *args;
    int replace;
} Context;

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * Error --
 *
 *      Sets a publication error message and returns TCL_ERROR.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
static int Error(Tcl_Interp *ip, const char *message) {
    Tcl_SetObjResult(ip, Tcl_NewStringObj(message, -1));
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * Option --
 *
 *      Borrows one value from the validated output-options dictionary, or returns NULL when absent.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
static Tcl_Obj *Option(Context *c, const char *key) {
    Tcl_Obj *value = NULL, *k = Tcl_NewStringObj(key, -1);
    Tcl_IncrRefCount(k);
    Tcl_DictObjGet(c->ip, c->args, k, &value);
    Tcl_DecrRefCount(k);
    return value;
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * ResolveOutputNamespace --
 *
 *      Resolves an explicit destination namespace against the caller and creates missing parents.
 *      Returns a retained canonical name, or NULL with an error.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
static Tcl_Obj *ResolveOutputNamespace(Tcl_Interp *interp, Tcl_Obj *option) {
    const char *name = Tcl_GetString(option);
    if (*name == '\0') {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("-namespace must not be empty", -1));
        return NULL;
    }
    const char *current = Tcl_GetCurrentNamespace(interp)->fullName;
    Tcl_Obj *qualified = strncmp(name, "::", 2) == 0
                             ? option
                             : Tcl_ObjPrintf("%s%s%s", current, strcmp(current, "::") == 0 ? "" : "::", name);
    Tcl_IncrRefCount(qualified);
    Tcl_Namespace *ns = Tcl_FindNamespace(interp, Tcl_GetString(qualified), NULL, 0);
    if (ns == NULL) {
        ns = Tcl_CreateNamespace(interp, Tcl_GetString(qualified), NULL, NULL);
    }
    Tcl_Obj *result = NULL;
    if (ns != NULL) {
        result = Tcl_NewStringObj(ns->fullName, -1);
        Tcl_IncrRefCount(result);
    }
    Tcl_DecrRefCount(qualified);
    return result;
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * Qualified --
 *
 *      Returns a new fully qualified name, preserving absolute names and literal parentheses.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
static Tcl_Obj *Qualified(const char *ns, const char *s) {
    if (s[0] == ':' && s[1] == ':') {
        return Tcl_NewStringObj(s, -1);
    }
    return Tcl_ObjPrintf("%s%s%s", ns, strcmp(ns, "::") ? "::" : "", s);
}

/* Validate identity/type separately from write permission for post-publication checks. */
/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * CheckDestination --
 *
 *      Checks collision policy, vector command identity, real type and optional write permission.
 *      Unrelated commands, aliases and read-only replacement destinations are rejected.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
static int CheckDestination(Tcl_Interp *ip, Tcl_Obj *name, int replace, int writable) {
    const char *s = Tcl_GetString(name);
    Tcl_CmdInfo info;
    int command = Tcl_GetCommandInfo(ip, s, &info);
    if (Rbc_VectorExists2(ip, s)) {
        Rbc_Vector *v;
        if (!replace) {
            Tcl_SetObjResult(ip, Tcl_ObjPrintf("vector \"%s\" already exists", s));
            return TCL_ERROR;
        }
        if (Rbc_GetVector(ip, s, &v) != TCL_OK) {
            return TCL_ERROR;
        }
        if (!command || info.isNativeObjectProc != 2 || info.objClientData2 != (void *)v) {
            return Error(ip, "destination is not the vector's instance command");
        }
        if (Rbc_VectorGetType(v) != RBC_VECTOR_REAL) {
            return Error(ip, "destination vector must be real");
        }
        if (writable && Rbc_VectorCheckWritable(ip, v) != TCL_OK) {
            return TCL_ERROR;
        }
    } else if (command) {
        Tcl_SetObjResult(ip, Tcl_ObjPrintf("command \"%s\" already exists", s));
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * CreateDestination --
 *
 *      Creates an unmapped real vector using RBC literal names or its #auto allocator.
 *      Records the command token so a failed publication can remove only the command it created.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
static int CreateDestination(Tcl_Interp *ip, Output *o) {
    Tcl_Obj *args[7] = {Tcl_NewStringObj("::rbc::vector", -1),
                        Tcl_NewStringObj("create", -1),
                        o->name ? o->name : o->autoName,
                        Tcl_NewStringObj("-variable", -1),
                        Tcl_NewObj(),
                        Tcl_NewStringObj("-literal", -1),
                        Tcl_NewBooleanObj(o->name != NULL)};
    for (int j = 0; j < 7; j++) {
        Tcl_IncrRefCount(args[j]);
    }
    int code = Tcl_EvalObjv(ip, 7, args, TCL_EVAL_DIRECT);
    for (int j = 0; j < 7; j++) {
        Tcl_DecrRefCount(args[j]);
    }
    if (code != TCL_OK) {
        return code;
    }
    if (!o->name) {
        Tcl_Obj *first;
        if (Tcl_ListObjIndex(ip, Tcl_GetObjResult(ip), 0, &first) != TCL_OK || !first) {
            return Error(ip, "RBC did not return a created vector name");
        }
        o->name = Qualified(Tcl_GetCurrentNamespace(ip)->fullName, Tcl_GetString(first));
        Tcl_IncrRefCount(o->name);
    }
    o->created = Tcl_FindCommand(ip, Tcl_GetString(o->name), NULL, TCL_GLOBAL_ONLY);
    if (!o->created) {
        return Error(ip, "RBC did not create the destination command");
    }
    return TCL_OK;
}
#endif

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * MeasurePublishCmd --
 *
 *      Publishes a completed {x list y list} measurement as two caller-owned real RBC vectors. All samples are
 *      copied before creating or replacing destinations, permitting output/input aliasing without borrowed-storage
 *      invalidation. Preflights explicit names, reserves them before #auto, and removes newly created commands on
 *      failure. Existing replacements are not transactional against application notification callbacks.
 *
 * Results:
 *      TCL_OK with {x fullyQualifiedName y fullyQualifiedName}, or TCL_ERROR with an explanatory message.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
int MeasurePublishCmd(void *clientData, Tcl_Interp *ip, Tcl_Size objc, Tcl_Obj *const objv[]) {
    if (objc != 3) {
        Tcl_WrongNumArgs(ip, 1, objv, "result options");
        return TCL_ERROR;
    }
    if (MeasureInitVectors(ip) != TCL_OK) {
        return TCL_ERROR;
    }
#ifdef HAVE_RBC
    Tcl_Size size;
    if (Tcl_DictObjSize(ip, objv[1], &size) != TCL_OK || Tcl_DictObjSize(ip, objv[2], &size) != TCL_OK)
        return TCL_ERROR;
    Context context = {ip, objv[2], 0}, *c = &context;
    Tcl_Obj *policy = Option(c, "ifexists");
    if (policy && strcmp(Tcl_GetString(policy), "error") && strcmp(Tcl_GetString(policy), "replace")) {
        return Error(ip, "-ifexists must be error or replace");
    }
    c->replace = policy && !strcmp(Tcl_GetString(policy), "replace");
    Output out[2] = {{0}};
    int count = 2, code = TCL_ERROR;
    Tcl_Obj *nsName = NULL, *result = Tcl_NewDictObj();
    Tcl_IncrRefCount(result);
    for (int i = 0; i < count; i++) {
        out[i].key = i ? "y" : "x";
        Tcl_Obj *key = Tcl_NewStringObj(out[i].key, -1), *list = NULL, **elements;
        Tcl_IncrRefCount(key);
        int status = Tcl_DictObjGet(ip, objv[1], key, &list);
        Tcl_DecrRefCount(key);
        if (status != TCL_OK) {
            goto done;
        }
        if (!list) {
            Error(ip, "measurement result requires x and y fields");
            goto done;
        }
        if (Tcl_ListObjGetElements(ip, list, &out[i].n, &elements) != TCL_OK) {
            goto done;
        }
        if ((size_t)out[i].n > SIZE_MAX / sizeof(double)) {
            Error(ip, "measurement output is too large");
            goto done;
        }
        out[i].v = (double *)Tcl_Alloc((size_t)out[i].n * sizeof(double));
        for (Tcl_Size j = 0; j < out[i].n; j++) {
            if (Tcl_GetDoubleFromObj(ip, elements[j], &out[i].v[j]) != TCL_OK) {
                goto done;
            }
        }
    }
    Tcl_Obj *names = Option(c, "names"), *primary = Option(c, "name");
    Tcl_Obj *namespaceOption = Option(c, "namespace");
    nsName = namespaceOption ? ResolveOutputNamespace(ip, namespaceOption) : NULL;
    if (namespaceOption && !nsName) {
        goto done;
    }
    const char *ns = nsName ? Tcl_GetString(nsName) : Tcl_GetCurrentNamespace(ip)->fullName;
    int replace = c->replace;
    if (names) {
        Tcl_DictSearch search;
        Tcl_Obj *key, *value;
        int done;
        if (Tcl_DictObjFirst(ip, names, &search, &key, &value, &done) != TCL_OK) {
            goto done;
        }
        for (; !done; Tcl_DictObjNext(&search, &key, &value, &done)) {
            int found = 0;
            for (int i = 0; i < count; i++) {
                if (!strcmp(Tcl_GetString(key), out[i].key)) {
                    found = 1;
                }
            }
            if (!found) {
                Tcl_SetObjResult(ip, Tcl_ObjPrintf("unknown output field \"%s\"", Tcl_GetString(key)));
                Tcl_DictObjDone(&search);
                goto done;
            }
        }
        Tcl_DictObjDone(&search);
    }
    for (int i = 0; i < count; i++) {
        Tcl_Obj *name = NULL;
        if (names) {
            Tcl_Obj *key = Tcl_NewStringObj(out[i].key, -1);
            Tcl_IncrRefCount(key);
            Tcl_DictObjGet(ip, names, key, &name);
            Tcl_DecrRefCount(key);
        }
        if (!strcmp(out[i].key, "y") && primary) {
            if (name) {
                Error(ip, "use either -name or -names y, not both");
                goto done;
            }
            name = primary;
        }
        const char *text = name ? Tcl_GetString(name) : "#auto";
        size_t length = strlen(text);
        if (!strcmp(text, "#auto") || (length >= 7 && !strcmp(text + length - 7, "::#auto"))) {
            out[i].autoName = Qualified(ns, text);
            Tcl_IncrRefCount(out[i].autoName);
        } else {
            if (!Tcl_GetCharLength(name)) {
                Error(ip, "output name must not be empty");
                goto done;
            }
            out[i].name = Qualified(ns, Tcl_GetString(name));
            Tcl_IncrRefCount(out[i].name);
            for (int j = 0; j < i; j++) {
                if (out[j].name && !strcmp(Tcl_GetString(out[j].name), Tcl_GetString(out[i].name))) {
                    Error(ip, "output vector names must be distinct");
                    goto done;
                }
            }
            if (CheckDestination(ip, out[i].name, replace, 1) != TCL_OK) {
                goto done;
            }
        }
    }
    /* Reserve explicit names first so #auto cannot take one of them. */
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < count; i++) {
            if ((pass == 0 && !out[i].name) || (pass == 1 && out[i].name)) {
                continue;
            }
            if (out[i].name && CheckDestination(ip, out[i].name, replace, 1) != TCL_OK) {
                goto done;
            }
            if (!out[i].name || !Rbc_VectorExists2(ip, Tcl_GetString(out[i].name))) {
                if (CreateDestination(ip, &out[i]) != TCL_OK) {
                    goto done;
                }
            }
        }
    }
    /* Different spellings may resolve to the same command. */
    for (int i = 0; i < count; i++) {
        Tcl_Command token = Tcl_FindCommand(ip, Tcl_GetString(out[i].name), NULL, TCL_GLOBAL_ONLY);
        for (int j = 0; j < i; j++) {
            if (token && token == Tcl_FindCommand(ip, Tcl_GetString(out[j].name), NULL, TCL_GLOBAL_ONLY)) {
                Error(ip, "output vector names must be distinct");
                goto done;
            }
        }
    }
    for (int i = 0; i < count; i++) {
        Rbc_Vector *v;
        if (CheckDestination(ip, out[i].name, 1, 1) != TCL_OK ||
            Rbc_GetVector(ip, Tcl_GetString(out[i].name), &v) != TCL_OK) {
            goto done;
        }
        if (Rbc_ResetVector(v, out[i].v, out[i].n, out[i].n, TCL_VOLATILE) != TCL_OK) {
            goto done;
        }
    }
    for (int i = 0; i < count; i++) {
        if (!Rbc_VectorExists2(ip, Tcl_GetString(out[i].name)) || CheckDestination(ip, out[i].name, 1, 0) != TCL_OK) {
            Error(ip, "output vector removed or replaced during notification");
            goto done;
        }
    }
    for (int i = 0; i < count; i++) {
        Tcl_DictObjPut(ip, result, Tcl_NewStringObj(out[i].key, -1), out[i].name);
    }
    Tcl_SetObjResult(ip, result);
    code = TCL_OK;
done:
    if (code != TCL_OK) {
        Tcl_InterpState state = Tcl_SaveInterpState(ip, code);
        for (int i = 0; i < count; i++) {
            if (out[i].created &&
                Tcl_FindCommand(ip, Tcl_GetString(out[i].name), NULL, TCL_GLOBAL_ONLY) == out[i].created) {
                Tcl_DeleteCommandFromToken(ip, out[i].created);
            }
        }
        Tcl_RestoreInterpState(ip, state);
    }
    if (nsName) {
        Tcl_DecrRefCount(nsName);
    }
    for (int i = 0; i < count; i++) {
        if (out[i].name) {
            Tcl_DecrRefCount(out[i].name);
        }
        if (out[i].autoName) {
            Tcl_DecrRefCount(out[i].autoName);
        }
        if (out[i].v) {
            Tcl_Free((char *)out[i].v);
        }
    }
    Tcl_DecrRefCount(result);
    return code;
#else
    return TCL_ERROR;
#endif
}
