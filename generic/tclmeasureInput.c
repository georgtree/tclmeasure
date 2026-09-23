#include "tclmeasureInput.h"

#ifdef HAVE_RBC
#include <rbcVector.h>
#include <rbcDecls.h>
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
    if (Rbc_VectorInitStubs(interp, "0.5.0", 0) == NULL) {
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
 *      remains unchanged. Vector offsets do not affect sample positions. Complex vectors are rejected.
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
