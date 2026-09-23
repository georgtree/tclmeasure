/*
 * tclmeasureInput.h --
 *
 *      Shared read-only input interface for lists and real RBC vectors.
 *      Storage is borrowed for one synchronous command; no Tcl evaluation or
 *      event processing is permitted after opening the inputs.
 */
#ifndef TCLMEASURE_INPUT_H
#define TCLMEASURE_INPUT_H

#include <tcl.h>

typedef struct {
    Tcl_Size length;
    Tcl_Obj **elements;
    const double *values;
    int square;
} MeasureInput;

int MeasureVectorSupportCmd(void *clientData, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[]);
int MeasureInitVectors(Tcl_Interp *interp);
int MeasureOpenInput(Tcl_Interp *interp, Tcl_Obj *obj, int vectors, MeasureInput *input);

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * MeasureGetDouble --
 *
 *      Reads a zero-based sample from borrowed list or vector storage. An optional squared view is used by RMS
 *      before interpolation, matching the existing Tcl implementation. Returns TCL_ERROR for invalid indices or
 *      non-numeric list elements, with the interpreter result set. Does not allocate a vector or evaluate Tcl.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
static inline int MeasureGetDouble(Tcl_Interp *interp, const MeasureInput *input, Tcl_Size index, double *value) {
    if (index < 0 || index >= input->length) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("measurement input index out of range", -1));
        return TCL_ERROR;
    }
    if (input->values != NULL) {
        *value = input->values[index];
    } else if (Tcl_GetDoubleFromObj(interp, input->elements[index], value) != TCL_OK) {
        return TCL_ERROR;
    }
    if (input->square) {
        *value *= *value;
    }
    return TCL_OK;
}

#endif /* TCLMEASURE_INPUT_H */
