
#include "tclmeasure.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * Tclmeasure_Init --
 *
 *      Entry point for the `tclmeasure` Tcl extension. This function initializes the package, ensures that
 *      the target namespace exists, provides the package to the Tcl interpreter, and registers all associated
 *      commands implemented in C.
 *
 * Parameters:
 *      Tcl_Interp *interp        - input/output: Tcl interpreter where the extension is being loaded
 *
 * Results:
 *      TCL_OK on success; TCL_ERROR if:
 *          - Tcl stubs initialization fails
 *          - Namespace creation fails
 *          - Tcl_PkgProvideEx fails
 *
 * Side Effects:
 *      - Ensures the namespace `::tclmeasure` exists
 *      - Registers the following object-based commands:
 *          ::tclmeasure::TrigTarg
 *          ::tclmeasure::FindDerivWhen
 *          ::tclmeasure::FindAt
 *          ::tclmeasure::DerivAt
 *          ::tclmeasure::Integ
 *          ::tclmeasure::MinMaxPPMinAtMaxAt
 *      - Marks the extension as available via `package require tclmeasure`
 *
 * Notes:
 *      - Requires Tcl version 8.6 through 10.0 (inclusive), verified via `Tcl_InitStubs()`
 *      - Relies on `Tcl_CreateObjCommand2`, which requires Tcl 8.6+
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
extern DLLEXPORT int Tclmeasure_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.6-10.0", 0) == NULL) {
        return TCL_ERROR;
    }
    /* check the existence of the namespace */
    if (Tcl_Eval(interp, "namespace eval ::tclmeasure {}") != TCL_OK) {
        return TCL_ERROR;
    }
    /* Provide the current package */
    if (Tcl_PkgProvideEx(interp, PACKAGE_NAME, PACKAGE_VERSION, NULL) != TCL_OK) {
        return TCL_ERROR;
    }
    Tcl_CreateObjCommand2(interp, "::tclmeasure::TrigTarg", (Tcl_ObjCmdProc2 *)TrigTargCmdProc2, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tclmeasure::FindDerivWhen", (Tcl_ObjCmdProc2 *)FindDerivWhenCmdProc2, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tclmeasure::FindAt", (Tcl_ObjCmdProc2 *)FindAtCmdProc2, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tclmeasure::DerivAt", (Tcl_ObjCmdProc2 *)DerivAtCmdProc2, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tclmeasure::Integ", (Tcl_ObjCmdProc2 *)IntegCmdProc2, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tclmeasure::MinMaxPPMinAtMaxAt", (Tcl_ObjCmdProc2 *)MinMaxPPMinAtMaxAtCmdProc2,
                          NULL, NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * CalcXBetween --
 *
 *      Compute the X-coordinate of a point lying on the line segment between (x1, y1) and (x2, y2),
 *      given a specific Y-coordinate (yBetween). This is a linear interpolation along the line.
 *
 * Parameters:
 *      double x1         - input: X-coordinate of the first point
 *      double y1         - input: Y-coordinate of the first point
 *      double x2         - input: X-coordinate of the second point
 *      double y2         - input: Y-coordinate of the second point
 *      double yBetween   - input: Y value for which to compute the corresponding X on the line
 *
 * Results:
 *      Returns the interpolated X-coordinate such that (x, yBetween) lies on the line from (x1, y1) to (x2, y2)
 *
 * Side Effects:
 *      None
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
static inline double CalcXBetween(double x1, double y1, double x2, double y2, double yBetween) {
    return (yBetween - y1) * (x2 - x1) / (y2 - y1) + x1;
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * CalcYBetween --
 *
 *      Compute the Y-coordinate of a point lying on the line segment between (x1, y1) and (x2, y2),
 *      given a specific X-coordinate (xBetween). This is a linear interpolation along the line.
 *
 * Parameters:
 *      double x1         - input: X-coordinate of the first point
 *      double y1         - input: Y-coordinate of the first point
 *      double x2         - input: X-coordinate of the second point
 *      double y2         - input: Y-coordinate of the second point
 *      double xBetween   - input: X value for which to compute the corresponding Y on the line
 *
 * Results:
 *      Returns the interpolated Y-coordinate such that (xBetween, y) lies on the line from (x1, y1) to (x2, y2)
 *
 * Side Effects:
 *      None
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
static inline double CalcYBetween(double x1, double y1, double x2, double y2, double xBetween) {
    return (y2 - y1) / (x2 - x1) * (xBetween - x1) + y1;
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * CalcCrossPoint --
 *
 *      Compute the X-coordinate of the intersection point of two lines. Each line is defined by a pair of points:
 *      (x11, y11)-(x21, y21) for the first line, and (x12, y12)-(x22, y22) for the second line. The function assumes
 *      the lines are not parallel and that the denominators are non-zero.
 *
 * Parameters:
 *      double x11, y11   - input: coordinates of the first point on the first line
 *      double x21, y21   - input: coordinates of the second point on the first line
 *      double x12, y12   - input: coordinates of the first point on the second line
 *      double x22, y22   - input: coordinates of the second point on the second line
 *
 * Results:
 *      Returns the X-coordinate of the intersection point of the two lines.
 *
 * Side Effects:
 *      None
 *
 * Notes:
 *      The function does not perform error checking for division by zero. It is the caller's responsibility to ensure
 *      the lines are not parallel (i.e., they have different slopes).
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
static inline double CalcCrossPoint(double x11, double y11, double x21, double y21, double x12, double y12, double x22,
                                    double y22) {
    return (y12 - (y22 - y12) / (x22 - x12) * x12 - (y11 - (y21 - y11) / (x21 - x11) * x11)) /
           ((y21 - y11) / (x21 - x11) - (y22 - y12) / (x22 - x12));
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * DerivSelect --
 *
 *      Prepare a 3-point stencil for derivative or interpolation calculations based on a specified X-coordinate
 *      (`xwhen`) that lies between two adjacent sample points (xi and xip1). Depending on the location of `xwhen`
 *      relative to the segment and its position in the array, the function selects appropriate X and Y values from
 *      the input arrays `x` and `vec`, converts them to doubles, and writes them into the `out` buffer.
 *
 * Parameters:
 *      Tcl_Interp *interp   - input: target interpreter used for Tcl_GetDoubleFromObj conversions
 *      Tcl_WideInt i        - input: current segment index (base point index in x/vec arrays)
 *      double xi            - input: X value at index `i`
 *      double xwhen         - input: target X value (interpolation/evaluation point)
 *      double xip1          - input: X value at index `i + 1`
 *      Tcl_WideInt xlen     - input: total number of elements in the `x` array
 *      Tcl_Obj **x          - input: array of Tcl_Obj pointers holding X values (at least x[i-1] to x[i+2])
 *      Tcl_Obj **vec        - input: array of Tcl_Obj pointers holding corresponding Y values
 *      double ywhen         - input: Y value at the point `xwhen` (for use in interpolation output)
 *      double *out          - output: pointer to a 6-element array to store the selected X and Y values
 *                                out[0..2] = selected X values (or interpolated positions)
 *                                out[3..5] = selected Y values (with ywhen at the center)
 *      int *pos             - output: index of the middle point in `out`, used to identify where `xwhen` fits:
 *                                -1 = unknown/middle is ywhen (e.g. edge case),
 *                                 0 = ywhen is second point in sequence (common case),
 *                                 1 = ywhen is last (used for final segment)
 *
 * Results:
 *      Populates the `out` buffer with 3 X values and 3 corresponding Y values to form a stencil around `xwhen`.
 *      Uses Tcl_GetDoubleFromObj to safely convert the values. The result is intended for slope or interpolation use.
 *
 * Side Effects:
 *      May set error messages in the interpreter if Tcl_GetDoubleFromObj fails (typically not expected if inputs
 *      are known to be numeric).
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
static void DerivSelect(Tcl_Interp *interp, Tcl_WideInt i, double xi, double xwhen, double xip1, Tcl_WideInt xlen,
                        Tcl_Obj **x, Tcl_Obj **vec, double ywhen, double *out, int *pos) {
    if (i == 0) {
        if (xi == xwhen) {
            out[0] = xwhen;
            out[1] = xip1;
            Tcl_GetDoubleFromObj(interp, x[i + 2], &out[2]);
            out[3] = ywhen;
            Tcl_GetDoubleFromObj(interp, vec[i + 1], &out[4]);
            Tcl_GetDoubleFromObj(interp, vec[i + 2], &out[5]);
            *pos = -1;
        } else if (xip1 == xwhen) {
            out[0] = xi;
            out[1] = xwhen;
            Tcl_GetDoubleFromObj(interp, x[i + 2], &out[2]);
            Tcl_GetDoubleFromObj(interp, vec[i + 1], &out[3]);
            out[4] = ywhen;
            Tcl_GetDoubleFromObj(interp, vec[i + 2], &out[5]);
            *pos = 0;
        } else {
            out[0] = xi;
            out[1] = xwhen;
            out[2] = xip1;
            Tcl_GetDoubleFromObj(interp, vec[i], &out[3]);
            out[4] = ywhen;
            Tcl_GetDoubleFromObj(interp, vec[i + 1], &out[5]);
            *pos = -1;
        }
    } else if (i == (xlen - 2)) {
        if (xip1 == xwhen) {
            Tcl_GetDoubleFromObj(interp, x[i - 1], &out[0]);
            out[1] = xi;
            out[2] = xwhen;
            Tcl_GetDoubleFromObj(interp, vec[i - 1], &out[3]);
            Tcl_GetDoubleFromObj(interp, vec[i], &out[4]);
            out[5] = ywhen;
            *pos = 1;
        } else {
            out[0] = xi;
            out[1] = xwhen;
            out[2] = xip1;
            Tcl_GetDoubleFromObj(interp, vec[i], &out[3]);
            out[4] = ywhen;
            Tcl_GetDoubleFromObj(interp, vec[i + 1], &out[5]);
            *pos = 1;
        }
    } else {
        if (xi == xwhen) {
            Tcl_GetDoubleFromObj(interp, x[i - 1], &out[0]);
            out[1] = xwhen;
            out[2] = xip1;
            Tcl_GetDoubleFromObj(interp, vec[i - 1], &out[3]);
            out[4] = ywhen;
            Tcl_GetDoubleFromObj(interp, vec[i + 1], &out[5]);
            *pos = 0;
        } else if (xip1 == xwhen) {
            out[0] = xi;
            out[1] = xwhen;
            Tcl_GetDoubleFromObj(interp, x[i + 2], &out[2]);
            Tcl_GetDoubleFromObj(interp, vec[i], &out[3]);
            out[4] = ywhen;
            Tcl_GetDoubleFromObj(interp, vec[i + 2], &out[5]);
            *pos = 0;
        } else {
            out[0] = xi;
            out[1] = xwhen;
            out[2] = xip1;
            Tcl_GetDoubleFromObj(interp, vec[i], &out[3]);
            out[4] = ywhen;
            Tcl_GetDoubleFromObj(interp, vec[i + 1], &out[5]);
            *pos = 0;
        }
    }
    return;
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * Deriv --
 *
 *      Compute the numerical derivative at a point using a three-point stencil. The function supports three types of
 *      finite-difference approximations (centered, backward, and forward) depending on the `type` parameter.
 *      The input values represent three adjacent (x, y) points: (xim1, yim1), (xi, yi), and (xip1, yip1).
 *
 * Parameters:
 *      double xim1     - input: X value at i-1 (previous point)
 *      double xi       - input: X value at i (current point)
 *      double xip1     - input: X value at i+1 (next point)
 *      double yim1     - input: Y value at i-1
 *      double yi       - input: Y value at i
 *      double yip1     - input: Y value at i+1
 *      int type        - input: derivative type selector:
 *                           0  = centered difference (at xi)
 *                          -1  = backward-biased (at xi)
 *                           1  = forward-biased (at xi)
 *
 * Results:
 *      Returns the estimated derivative at xi using the appropriate finite-difference formula based on `type`.
 *
 * Side Effects:
 *      None
 *
 * Notes:
 *      This method handles non-uniform spacing between X values. It assumes that h1 = xi - xim1 and h2 = xip1 - xi
 *      are both non-zero. If `type` is not -1, 0, or 1, the function returns 0.0 by default.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
static double Deriv(double xim1, double xi, double xip1, double yim1, double yi, double yip1, int type) {
    double h1 = xi - xim1;
    double h2 = xip1 - xi;
    if (type == 0) {
        return -h2 / (h1 * (h1 + h2)) * yim1 - (h1 - h2) / (h1 * h2) * yi + h1 / (h2 * (h1 + h2)) * yip1;
    } else if (type == -1) {
        return -(2 * h1 + h2) / (h1 * (h1 + h2)) * yim1 + (h1 + h2) / (h1 * h2) * yi - h1 / (h2 * (h1 + h2)) * yip1;
    } else if (type == 1) {
        return h2 / (h1 * (h1 + h2)) * yim1 - (h1 + h2) / (h1 * h2) * yi + (h1 + 2 * h2) / (h2 * (h1 + h2)) * yip1;
    }
    return 0.0;
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * TrigTargCmdProc2 --
 *
 *      Tcl command implementation that analyzes two vector signals (trigVec and targVec) over a shared time base (x),
 *      detects specified edge transitions (rise, fall, or crossing) for each vector, and returns the time difference
 *      between the trigger and target events. The function supports both fixed occurrence counts and "last" mode to
 *      capture the final matching transition.
 *
 * Parameters:
 *      void *clientData              - input: optional client data (unused)
 *      Tcl_Interp *interp            - input/output: interpreter used for argument parsing and result/error reporting
 *      Tcl_Size objc                 - input: number of arguments passed to the command
 *      Tcl_Obj *const objv[]         - input: argument vector; expected format:
 *
 *              objv[1]  = x           - Tcl list of numeric X values (time base)
 *              objv[2]  = trigVec     - Tcl list of Y values for the trigger signal
 *              objv[3]  = val1        - trigger threshold value
 *              objv[4]  = targVec     - Tcl list of Y values for the target signal
 *              objv[5]  = val2        - target threshold value
 *              objv[6]  = trigCond    - trigger condition: "rise", "fall", or "cross"
 *              objv[7]  = trigCount   - trigger hit index to use (or "last")
 *              objv[8]  = targCond    - target condition: "rise", "fall", or "cross"
 *              objv[9]  = targCount   - target hit index to use (or "last")
 *              objv[10] = trigDelay   - minimum X value before trigger events are considered
 *              objv[11] = targDelay   - minimum X value before target events are considered
 *
 * Results:
 *      On success, returns a dictionary with the following keys:
 *          "xtrig"   => interpolated X value at which trigger condition was met
 *          "xtarg"   => interpolated X value at which target condition was met
 *          "xdelta"  => difference (xtarg - xtrig)
 *
 *      On failure, returns TCL_ERROR and sets an error message in the interpreter.
 *
 * Side Effects:
 *      Parses and converts lists to arrays, and individual values to doubles and integers.
 *      May allocate intermediate Tcl_Obj structures.
 *      Sets the interpreter result to either an error message or result dictionary.
 *
 * Notes:
 *      - Lists must be of equal length.
 *      - Events are detected using a two-point scan for each segment: (xi, xi+1), (vec[i], vec[i+1]).
 *      - Linear interpolation is used to estimate the exact X value where val1/val2 thresholds are crossed.
 *      - Condition counts are 1-based; use "last" to return the final matching transition.
 *      - If the requested condition is not found, a descriptive error is returned.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
static int TrigTargCmdProc2(void *clientData, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[]) {
    int trigVecCount = 0;
    int targVecCount = 0;
    int trigVecFoundFlag = 0;
    int targVecFoundFlag = 0;
    double lastTrigHit[4] = {0.0, 0.0, 0.0, 0.0};
    int lastTrigHitSet = 0;
    double lastTargHit[4] = {0.0, 0.0, 0.0, 0.0};
    int lastTargHitSet = 0;
    double xTrig, xTarg;
    if (objc != 12) {
        Tcl_WrongNumArgs(interp, 11, objv,
                         "x trigVec val1 targVec val2 trigVecCond trigVecCondCount targVecCond targVecCondCount "
                         "trigVecDelay targVecDelay");
        return TCL_ERROR;
    }
    Tcl_Obj *xVec = objv[1];
    Tcl_Obj *trigVec = objv[2];
    double val1;
    Tcl_GetDoubleFromObj(interp, objv[3], &val1);
    Tcl_Obj *targVec = objv[4];
    double val2;
    Tcl_GetDoubleFromObj(interp, objv[5], &val2);
    int trigVecCond;
    if (!strcmp(Tcl_GetString(objv[6]), "rise")) {
        trigVecCond = COND_RISE;
    } else if (!strcmp(Tcl_GetString(objv[6]), "fall")) {
        trigVecCond = COND_FALL;
    } else {
        trigVecCond = COND_CROSS;
    }
    Tcl_WideInt trigVecCondCount;
    if (!strcmp(Tcl_GetString(objv[7]), "last")) {
        trigVecCondCount = -1;
    } else {
        Tcl_GetWideIntFromObj(interp, objv[7], &trigVecCondCount);
    }
    int targVecCond;
    if (!strcmp(Tcl_GetString(objv[8]), "rise")) {
        targVecCond = COND_RISE;
    } else if (!strcmp(Tcl_GetString(objv[8]), "fall")) {
        targVecCond = COND_FALL;
    } else {
        targVecCond = COND_CROSS;
    }
    Tcl_WideInt targVecCondCount;
    if (!strcmp(Tcl_GetString(objv[9]), "last")) {
        targVecCondCount = -1;
    } else {
        Tcl_GetWideIntFromObj(interp, objv[9], &targVecCondCount);
    }
    double trigVecDelay;
    Tcl_GetDoubleFromObj(interp, objv[10], &trigVecDelay);
    double targVecDelay;
    Tcl_GetDoubleFromObj(interp, objv[11], &targVecDelay);

    Tcl_Size xLen, trigVecLen, targVecLen;
    Tcl_Obj **xVecElems, **trigVecElems, **targVecElems;
    if (Tcl_ListObjGetElements(interp, xVec, &xLen, &xVecElems) == TCL_ERROR) {
        return TCL_ERROR;
    }
    if (Tcl_ListObjGetElements(interp, trigVec, &trigVecLen, &trigVecElems) == TCL_ERROR) {
        return TCL_ERROR;
    }
    if (Tcl_ListObjGetElements(interp, targVec, &targVecLen, &targVecElems) == TCL_ERROR) {
        return TCL_ERROR;
    }
    if (xLen != trigVecLen) {
        Tcl_Obj *errorMsg =
            Tcl_ObjPrintf("Length of x '%ld' is not equal to length of trigVec '%ld'", xLen, trigVecLen);
        Tcl_SetObjResult(interp, errorMsg);
        return TCL_ERROR;
    } else if (trigVecLen != targVecLen) {
        Tcl_Obj *errorMsg =
            Tcl_ObjPrintf("Length of trigVec '%ld' is not equal to length of targVec '%ld'", trigVecLen, targVecLen);
        Tcl_SetObjResult(interp, errorMsg);
        return TCL_ERROR;
    }
    for (Tcl_Size i = 0; i < trigVecLen - 1; ++i) {
        double xi;
        Tcl_GetDoubleFromObj(interp, xVecElems[i], &xi);
        if ((xi < trigVecDelay) && (xi < targVecDelay)) {
            continue;
        }
        double xip1, trigVecI, trigVecIp1, targVecI, targVecIp1;
        Tcl_GetDoubleFromObj(interp, xVecElems[i + 1], &xip1);
        Tcl_GetDoubleFromObj(interp, trigVecElems[i], &trigVecI);
        Tcl_GetDoubleFromObj(interp, trigVecElems[i + 1], &trigVecIp1);
        Tcl_GetDoubleFromObj(interp, targVecElems[i], &targVecI);
        Tcl_GetDoubleFromObj(interp, targVecElems[i + 1], &targVecIp1);
        if (!trigVecFoundFlag && (xi >= trigVecDelay)) {
            int result;
            switch ((enum Conditions)trigVecCond) {
            case COND_RISE:
                if ((trigVecI <= val1) && (trigVecIp1 > val1)) {
                    result = 1;
                } else {
                    result = 0;
                }
                break;
            case COND_FALL:
                if ((trigVecI >= val1) && (trigVecIp1 < val1)) {
                    result = 1;
                } else {
                    result = 0;
                }
                break;
            case COND_CROSS:
                if (((trigVecI <= val1) && (trigVecIp1 > val1)) || ((trigVecI >= val1) && (trigVecIp1 < val1))) {
                    result = 1;
                } else {
                    result = 0;
                }
                break;
            };
            if (result) {
                trigVecCount++;
                if (trigVecCondCount == -1) {
                    lastTrigHit[0] = xi;
                    lastTrigHit[1] = trigVecI;
                    lastTrigHit[2] = xip1;
                    lastTrigHit[3] = trigVecIp1;
                    lastTrigHitSet = 1;
                }
            }
        }
        if (!targVecFoundFlag && (xi >= targVecDelay)) {
            int result;
            switch ((enum Conditions)targVecCond) {
            case COND_RISE:
                if ((targVecI <= val2) && (targVecIp1 > val2)) {
                    result = 1;
                } else {
                    result = 0;
                }
                break;
            case COND_FALL:
                if ((targVecI >= val2) && (targVecIp1 < val2)) {
                    result = 1;
                } else {
                    result = 0;
                }
                break;
            case COND_CROSS:
                if (((targVecI <= val2) && (targVecIp1 > val2)) || ((targVecI >= val2) && (targVecIp1 < val2))) {
                    result = 1;
                } else {
                    result = 0;
                }
                break;
            };
            if (result) {
                targVecCount++;
                if (targVecCondCount == -1) {
                    lastTargHit[0] = xi;
                    lastTargHit[1] = targVecI;
                    lastTargHit[2] = xip1;
                    lastTargHit[3] = targVecIp1;
                    lastTargHitSet = 1;
                }
            }
        }
        if (trigVecCondCount != -1) {
            if ((trigVecCount == trigVecCondCount) && !trigVecFoundFlag) {
                trigVecFoundFlag = 1;
                xTrig = CalcXBetween(xi, trigVecI, xip1, trigVecIp1, val1);
            }
        }
        if (targVecCondCount != -1) {
            if ((targVecCount == targVecCondCount) && !targVecFoundFlag) {
                targVecFoundFlag = 1;
                xTarg = CalcXBetween(xi, targVecI, xip1, targVecIp1, val2);
            }
        }
        if (trigVecFoundFlag && targVecFoundFlag) {
            break;
        }
    }
    if ((trigVecCondCount == -1) && lastTrigHitSet) {
        trigVecFoundFlag = 1;
        xTrig = CalcXBetween(lastTrigHit[0], lastTrigHit[1], lastTrigHit[2], lastTrigHit[3], val1);
    }
    if ((targVecCondCount == -1) && lastTargHitSet) {
        targVecFoundFlag = 1;
        xTarg = CalcXBetween(lastTargHit[0], lastTargHit[1], lastTargHit[2], lastTargHit[3], val2);
    }
    if (!trigVecFoundFlag) {
        const char *condition;
        const char *vecCondCount;
        switch ((enum Conditions)trigVecCond) {
        case COND_RISE:
            condition = "rise";
            break;
        case COND_FALL:
            condition = "fall";
            break;
        case COND_CROSS:
            condition = "cross";
            break;
        };
        if (trigVecCondCount == -1) {
            vecCondCount = "last";
        } else {
            vecCondCount = Tcl_GetString(Tcl_NewWideIntObj(trigVecCondCount));
        }
        Tcl_Obj *errorMsg = Tcl_ObjPrintf("Trig value '%f' with conditions '%s %s delay=%f' was not found", val1,
                                          condition, vecCondCount, trigVecDelay);
        Tcl_SetObjResult(interp, errorMsg);
        return TCL_ERROR;
    } else if (!targVecFoundFlag) {
        const char *condition;
        const char *vecCondCount;
        switch ((enum Conditions)targVecCond) {
        case COND_RISE:
            condition = "rise";
            break;
        case COND_FALL:
            condition = "fall";
            break;
        case COND_CROSS:
            condition = "cross";
            break;
        };
        if (targVecCondCount == -1) {
            vecCondCount = "last";
        } else {
            vecCondCount = Tcl_GetString(Tcl_NewWideIntObj(targVecCondCount));
        }
        Tcl_Obj *errorMsg = Tcl_ObjPrintf("Targ value '%f' with conditions '%s %s delay=%f' was not found", val2,
                                          condition, vecCondCount, targVecDelay);
        Tcl_SetObjResult(interp, errorMsg);
        return TCL_ERROR;
    }
    Tcl_Obj *result = Tcl_NewDictObj();
    Tcl_DictObjPut(interp, result, Tcl_NewStringObj("xtrig", -1), Tcl_NewDoubleObj(xTrig));
    Tcl_DictObjPut(interp, result, Tcl_NewStringObj("xtarg", -1), Tcl_NewDoubleObj(xTarg));
    Tcl_DictObjPut(interp, result, Tcl_NewStringObj("xdelta", -1), Tcl_NewDoubleObj(xTarg - xTrig));
    Tcl_SetObjResult(interp, result);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * FindDerivWhenCmdProc2 --
 *
 *      Implements a Tcl command to evaluate time-domain vector conditions (rise/fall/crossing) over one or more
 *      time-aligned signals. Depending on the selected mode, it returns either the time (`xWhen`) of a matching
 *      condition, the corresponding value from a secondary signal (`yFind`), or the derivative at the matched time.
 *      The function supports flexible condition targeting via explicit index, "last", or "all" modes.
 *
 * Parameters:
 *      void *clientData              - input: optional user data (unused)
 *      Tcl_Interp *interp            - input/output: interpreter for result and error reporting
 *      Tcl_Size objc                 - input: number of command arguments
 *      Tcl_Obj *const objv[]         - input: command arguments, expected as:
 *
 *          objv[1]  = x              - list of X (time) values
 *          objv[2]  = mode           - one of: when, wheneq, findwhen, findwheneq, derivwhen, derivwheneq
 *          objv[3]  = findVec        - list of values used for yFind or derivative computations
 *          objv[4]  = whenVecLS      - left-side comparison signal for condition detection
 *          objv[5]  = val            - scalar threshold for single-vector comparisons
 *          objv[6]  = whenVecRS      - right-side signal (used for equality/cross-vector mode)
 *          objv[7]  = whenVecCond    - condition: "rise", "fall", or "cross"
 *          objv[8]  = whenVecCondCount - index (1-based), "last", or "all" occurrence of condition to use
 *          objv[9]  = delay          - minimum X before any condition is considered
 *          objv[10] = from           - inclusive range start for evaluation
 *          objv[11] = to             - inclusive range end for evaluation
 *
 * Results:
 *      TCL_OK on success, with interpreter result set to:
 *        - list of xWhen values       (mode: "when", "wheneq")
 *        - list of interpolated yFind (mode: "findwhen", "findwheneq")
 *        - list of derivatives        (mode: "derivwhen", "derivwheneq")
 *
 *      TCL_ERROR on failure (e.g. invalid arguments, mismatched vector lengths, no match found).
 *
 * Side Effects:
 *      May allocate and populate Tcl lists (`xWhenObj`, `yFindObj`, `derYObj`) as return values.
 *      Sets interpreter result to descriptive error messages if validation or detection fails.
 *
 * Notes:
 *      - Matching is performed on (x, value) pairs from `whenVecLS` and optionally `whenVecRS`.
 *      - Linear interpolation is used to find exact crossing points (`CalcXBetween`, `CalcCrossPoint`).
 *      - Derivative estimation uses 3-point stencil via `DerivSelect()` and `Deriv()` with positional logic.
 *      - "last" uses the most recent matching event, "all" accumulates all matching times and values.
 *      - For `wheneq`, a cross-condition between `whenVecLS` and `whenVecRS` is evaluated.
 *      - Derivative results are aligned with crossing points and interpolated values.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
static int FindDerivWhenCmdProc2(void *clientData, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[]) {
    double lastWhenHit[4] = {0.0, 0.0, 0.0, 0.0};
    int lastWhenHitSet = 0;
    double lastWhenHitCross[8] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    int lastWhenHitCrossSet = 0;
    double lastFindWhenHit[4] = {0.0, 0.0, 0.0, 0.0};
    double lastDerYWhenHit[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    int lastDerYWhenHitPos;
    int xWhenSet = 0;
    Tcl_Obj *xWhenObj = Tcl_NewListObj(0, NULL);
    Tcl_Obj *yFindObj = Tcl_NewListObj(0, NULL);
    Tcl_Obj *derYObj = Tcl_NewListObj(0, NULL);

    if (objc != 12) {
        Tcl_WrongNumArgs(interp, 11, objv,
                         "x mode findVec whenVecLS val whenVecRS whenVecCond whenVecCondCount delay from to");
        return TCL_ERROR;
    }
    Tcl_Obj *xVec = objv[1];
    int mode;
    if (Tcl_GetIndexFromObj(NULL, objv[2], FindDerivWhenSwitches, "mode", 0, &mode) != TCL_OK) {
        return TCL_ERROR;
    }
    Tcl_Obj *findVec = objv[3];
    Tcl_Obj *whenVecLS = objv[4];
    double val;
    Tcl_GetDoubleFromObj(interp, objv[5], &val);
    Tcl_Obj *whenVecRS = objv[6];
    int whenVecCond;
    if (!strcmp(Tcl_GetString(objv[7]), "rise")) {
        whenVecCond = COND_RISE;
    } else if (!strcmp(Tcl_GetString(objv[7]), "fall")) {
        whenVecCond = COND_FALL;
    } else {
        whenVecCond = COND_CROSS;
    }
    Tcl_WideInt whenVecCondCount;
    if (!strcmp(Tcl_GetString(objv[8]), "last")) {
        whenVecCondCount = -1;
    } else if (!strcmp(Tcl_GetString(objv[8]), "all")) {
        whenVecCondCount = -2;
    } else {
        Tcl_GetWideIntFromObj(interp, objv[8], &whenVecCondCount);
    }
    double delay;
    Tcl_GetDoubleFromObj(interp, objv[9], &delay);
    double from;
    Tcl_GetDoubleFromObj(interp, objv[10], &from);
    double to;
    Tcl_GetDoubleFromObj(interp, objv[11], &to);

    Tcl_Size xLen, findVecLen, whenVecLSLen, whenVecRSLen;
    Tcl_Obj **xVecElems, **findVecElems, **whenVecLSElems, **whenVecRSElems;
    if (Tcl_ListObjGetElements(interp, xVec, &xLen, &xVecElems) == TCL_ERROR) {
        return TCL_ERROR;
    }
    if (Tcl_ListObjGetElements(interp, findVec, &findVecLen, &findVecElems) == TCL_ERROR) {
        return TCL_ERROR;
    }
    if (Tcl_ListObjGetElements(interp, whenVecLS, &whenVecLSLen, &whenVecLSElems) == TCL_ERROR) {
        return TCL_ERROR;
    }
    if (Tcl_ListObjGetElements(interp, whenVecRS, &whenVecRSLen, &whenVecRSElems) == TCL_ERROR) {
        return TCL_ERROR;
    }
    if ((mode == FDW_SWITCH_WHEN) || (mode == FDW_SWITCH_WHENEQ) || (mode == FDW_SWITCH_FINDWHEN) ||
        (mode == FDW_SWITCH_FINDWHENEQ)) {
        if (xLen != whenVecLSLen) {
            Tcl_Obj *errorMsg =
                Tcl_ObjPrintf("Length of x '%ld' is not equal to length of whenVecLS '%ld'", xLen, whenVecLSLen);
            Tcl_SetObjResult(interp, errorMsg);
            return TCL_ERROR;
        }
    }
    if ((mode == FDW_SWITCH_WHENEQ) || (mode == FDW_SWITCH_FINDWHENEQ) || (mode == FDW_SWITCH_DERIVWHENEQ)) {
        if (xLen != whenVecRSLen) {
            Tcl_Obj *errorMsg =
                Tcl_ObjPrintf("Length of x '%ld' is not equal to length of whenVecRS '%ld'", xLen, whenVecRSLen);
            Tcl_SetObjResult(interp, errorMsg);
            return TCL_ERROR;
        }
    }
    if ((mode == FDW_SWITCH_FINDWHEN) || (mode == FDW_SWITCH_DERIVWHEN)) {
        if (xLen != findVecLen) {
            Tcl_Obj *errorMsg =
                Tcl_ObjPrintf("Length of x '%ld' is not equal to length of findVec '%ld'", xLen, findVecLen);
            Tcl_SetObjResult(interp, errorMsg);
            return TCL_ERROR;
        }
    }
    Tcl_WideInt whenVecCount = 0;
    int whenVecFoundFlag = 0;
    double xWhen, yFind, derY;
    if ((mode == FDW_SWITCH_WHEN) || (mode == FDW_SWITCH_FINDWHEN) || (mode == FDW_SWITCH_DERIVWHEN)) {
        for (Tcl_Size i = 0; i < whenVecLSLen - 1; ++i) {
            double xi;
            Tcl_GetDoubleFromObj(interp, xVecElems[i], &xi);
            if ((xi < (from + delay)) || (xi > to)) {
                continue;
            }
            double xip1, whenVecLSI, whenVecLSIp1;
            Tcl_GetDoubleFromObj(interp, xVecElems[i + 1], &xip1);
            Tcl_GetDoubleFromObj(interp, whenVecLSElems[i], &whenVecLSI);
            Tcl_GetDoubleFromObj(interp, whenVecLSElems[i + 1], &whenVecLSIp1);
            if (!whenVecFoundFlag) {
                int result;
                switch ((enum Conditions)whenVecCond) {
                case COND_RISE:
                    if ((whenVecLSI <= val) && (whenVecLSIp1 > val)) {
                        result = 1;
                    } else {
                        result = 0;
                    }
                    break;
                case COND_FALL:
                    if ((whenVecLSI >= val) && (whenVecLSIp1 < val)) {
                        result = 1;
                    } else {
                        result = 0;
                    }
                    break;
                case COND_CROSS:
                    if (((whenVecLSI <= val) && (whenVecLSIp1 > val)) ||
                        ((whenVecLSI >= val) && (whenVecLSIp1 < val))) {
                        result = 1;
                    } else {
                        result = 0;
                    }
                    break;
                };
                if (result) {
                    whenVecCount++;
                    if (whenVecCondCount == -1) {
                        xWhen = CalcXBetween(xi, whenVecLSI, xip1, whenVecLSIp1, val);
                        xWhenSet = 1;
                        lastWhenHit[0] = xi;
                        lastWhenHit[1] = whenVecLSI;
                        lastWhenHit[2] = xip1;
                        lastWhenHit[3] = whenVecLSIp1;
                        lastWhenHitSet = 1;
                        if (mode == FDW_SWITCH_FINDWHEN) {
                            lastFindWhenHit[0] = xi;
                            Tcl_GetDoubleFromObj(interp, findVecElems[i], &lastFindWhenHit[1]);
                            lastFindWhenHit[2] = xip1;
                            Tcl_GetDoubleFromObj(interp, findVecElems[i + 1], &lastFindWhenHit[3]);
                        } else if (mode == FDW_SWITCH_DERIVWHEN) {
                            double findVecElemITemp;
                            double findVecElemIp1Temp;
                            Tcl_GetDoubleFromObj(interp, findVecElems[i], &findVecElemITemp);
                            Tcl_GetDoubleFromObj(interp, findVecElems[i + 1], &findVecElemIp1Temp);
                            double yDeriv = CalcYBetween(xi, findVecElemITemp, xip1, findVecElemIp1Temp, xWhen);
                            DerivSelect(interp, i, xi, xWhen, xip1, xLen, xVecElems, findVecElems, yDeriv,
                                        lastDerYWhenHit, &lastDerYWhenHitPos);
                        }
                    } else if (whenVecCondCount == -2) {
                        double xWhenLoc = CalcXBetween(xi, whenVecLSI, xip1, whenVecLSIp1, val);
                        Tcl_ListObjAppendElement(interp, xWhenObj, Tcl_NewDoubleObj(xWhenLoc));
                        xWhenSet = 1;
                        if (mode == FDW_SWITCH_FINDWHEN) {
                            double findVecElemITemp;
                            double findVecElemIp1Temp;
                            Tcl_GetDoubleFromObj(interp, findVecElems[i], &findVecElemITemp);
                            Tcl_GetDoubleFromObj(interp, findVecElems[i + 1], &findVecElemIp1Temp);
                            double yFindLoc = CalcYBetween(xi, findVecElemITemp, xip1, findVecElemIp1Temp, xWhenLoc);
                            Tcl_ListObjAppendElement(interp, yFindObj, Tcl_NewDoubleObj(yFindLoc));
                        } else if (mode == FDW_SWITCH_DERIVWHEN) {
                            double findVecElemITemp;
                            double findVecElemIp1Temp;
                            double derivDataTemp[6];
                            int derivPosTemp;
                            Tcl_GetDoubleFromObj(interp, findVecElems[i], &findVecElemITemp);
                            Tcl_GetDoubleFromObj(interp, findVecElems[i + 1], &findVecElemIp1Temp);
                            double yDeriv = CalcYBetween(xi, findVecElemITemp, xip1, findVecElemIp1Temp, xWhenLoc);
                            DerivSelect(interp, i, xi, xWhenLoc, xip1, xLen, xVecElems, findVecElems, yDeriv,
                                        derivDataTemp, &derivPosTemp);
                            double derYLoc = Deriv(derivDataTemp[0], derivDataTemp[1], derivDataTemp[2],
                                                   derivDataTemp[3], derivDataTemp[4], derivDataTemp[5], derivPosTemp);
                            Tcl_ListObjAppendElement(interp, derYObj, Tcl_NewDoubleObj(derYLoc));
                        }
                    }
                }
            }
            if ((whenVecCondCount != -2) && (whenVecCondCount != -1)) {
                if ((whenVecCount == whenVecCondCount) && !whenVecFoundFlag) {
                    whenVecFoundFlag = 1;
                    xWhen = CalcXBetween(xi, whenVecLSI, xip1, whenVecLSIp1, val);
                    Tcl_ListObjAppendElement(interp, xWhenObj, Tcl_NewDoubleObj(xWhen));
                    xWhenSet = 1;
                    if (mode == FDW_SWITCH_FINDWHEN) {
                        double findVecElemITemp;
                        double findVecElemIp1Temp;
                        Tcl_GetDoubleFromObj(interp, findVecElems[i], &findVecElemITemp);
                        Tcl_GetDoubleFromObj(interp, findVecElems[i + 1], &findVecElemIp1Temp);
                        yFind = CalcYBetween(xi, findVecElemITemp, xip1, findVecElemIp1Temp, xWhen);
                        Tcl_ListObjAppendElement(interp, yFindObj, Tcl_NewDoubleObj(yFind));
                    } else if (mode == FDW_SWITCH_DERIVWHEN) {
                        double findVecElemITemp;
                        double findVecElemIp1Temp;
                        double derivDataTemp[6];
                        int derivPosTemp;
                        Tcl_GetDoubleFromObj(interp, findVecElems[i], &findVecElemITemp);
                        Tcl_GetDoubleFromObj(interp, findVecElems[i + 1], &findVecElemIp1Temp);
                        double yDeriv = CalcYBetween(xi, findVecElemITemp, xip1, findVecElemIp1Temp, xWhen);
                        DerivSelect(interp, i, xi, xWhen, xip1, xLen, xVecElems, findVecElems, yDeriv, derivDataTemp,
                                    &derivPosTemp);
                        derY = Deriv(derivDataTemp[0], derivDataTemp[1], derivDataTemp[2], derivDataTemp[3],
                                     derivDataTemp[4], derivDataTemp[5], derivPosTemp);
                        Tcl_ListObjAppendElement(interp, derYObj, Tcl_NewDoubleObj(derY));
                    }
                    break;
                }
            }
        }
    } else if ((mode == FDW_SWITCH_WHENEQ) || (mode == FDW_SWITCH_FINDWHENEQ) || (mode == FDW_SWITCH_DERIVWHENEQ)) {
        for (Tcl_Size i = 0; i < whenVecLSLen - 1; ++i) {
            double xi;
            Tcl_GetDoubleFromObj(interp, xVecElems[i], &xi);
            if ((xi < (from + delay)) || (xi > to)) {
                continue;
            }
            double xip1, whenVecLSI, whenVecLSIp1, whenVecRSI, whenVecRSIp1;
            Tcl_GetDoubleFromObj(interp, xVecElems[i + 1], &xip1);
            Tcl_GetDoubleFromObj(interp, whenVecLSElems[i], &whenVecLSI);
            Tcl_GetDoubleFromObj(interp, whenVecLSElems[i + 1], &whenVecLSIp1);
            Tcl_GetDoubleFromObj(interp, whenVecRSElems[i], &whenVecRSI);
            Tcl_GetDoubleFromObj(interp, whenVecRSElems[i + 1], &whenVecRSIp1);
            if (!whenVecFoundFlag) {
                // check two lines crossing
                if (((whenVecLSI >= whenVecRSI) && (whenVecLSIp1 <= whenVecRSIp1)) ||
                    ((whenVecLSI <= whenVecRSI) && (whenVecLSIp1 >= whenVecRSIp1))) {
                    int result;
                    switch ((enum Conditions)whenVecCond) {
                    case COND_RISE:
                        if (whenVecLSI < whenVecLSIp1) {
                            result = 1;
                        } else {
                            result = 0;
                        }
                        break;
                    case COND_FALL:
                        if (whenVecLSI > whenVecLSIp1) {
                            result = 1;
                        } else {
                            result = 0;
                        }
                        break;
                    case COND_CROSS:
                        result = 1;
                        break;
                    };
                    if (result) {
                        whenVecCount++;
                        if (whenVecCondCount == -1) {
                            xWhen =
                                CalcCrossPoint(xi, whenVecLSI, xip1, whenVecLSIp1, xi, whenVecRSI, xip1, whenVecRSIp1);
                            xWhenSet = 1;
                            lastWhenHitCross[0] = xi;
                            lastWhenHitCross[1] = whenVecLSI;
                            lastWhenHitCross[2] = xip1;
                            lastWhenHitCross[3] = whenVecLSIp1;
                            lastWhenHitCross[4] = xi;
                            lastWhenHitCross[5] = whenVecRSI;
                            lastWhenHitCross[6] = xip1;
                            lastWhenHitCross[7] = whenVecRSIp1;
                            lastWhenHitCrossSet = 1;
                            lastWhenHitSet = 1;
                            if (mode == FDW_SWITCH_FINDWHEN) {
                                lastFindWhenHit[0] = xi;
                                Tcl_GetDoubleFromObj(interp, findVecElems[i], &lastFindWhenHit[1]);
                                lastFindWhenHit[2] = xip1;
                                Tcl_GetDoubleFromObj(interp, findVecElems[i + 1], &lastFindWhenHit[3]);
                            } else if (mode == FDW_SWITCH_DERIVWHEN) {
                                double findVecElemITemp;
                                double findVecElemIp1Temp;
                                Tcl_GetDoubleFromObj(interp, findVecElems[i], &findVecElemITemp);
                                Tcl_GetDoubleFromObj(interp, findVecElems[i + 1], &findVecElemIp1Temp);
                                double yDeriv = CalcYBetween(xi, findVecElemITemp, xip1, findVecElemIp1Temp, xWhen);
                                DerivSelect(interp, i, xi, xWhen, xip1, xLen, xVecElems, findVecElems, yDeriv,
                                            lastDerYWhenHit, &lastDerYWhenHitPos);
                            }
                        } else if (whenVecCondCount == -2) {
                            double xWhenLoc =
                                CalcCrossPoint(xi, whenVecLSI, xip1, whenVecLSIp1, xi, whenVecRSI, xip1, whenVecRSIp1);
                            xWhenSet = 1;
                            Tcl_ListObjAppendElement(interp, xWhenObj, Tcl_NewDoubleObj(xWhenLoc));
                            if (mode == FDW_SWITCH_FINDWHENEQ) {
                                double findVecElemITemp;
                                double findVecElemIp1Temp;
                                Tcl_GetDoubleFromObj(interp, findVecElems[i], &findVecElemITemp);
                                Tcl_GetDoubleFromObj(interp, findVecElems[i + 1], &findVecElemIp1Temp);
                                double yFindLoc =
                                    CalcYBetween(xi, findVecElemITemp, xip1, findVecElemIp1Temp, xWhenLoc);
                                Tcl_ListObjAppendElement(interp, yFindObj, Tcl_NewDoubleObj(yFindLoc));
                            } else if (mode == FDW_SWITCH_DERIVWHENEQ) {
                                double findVecElemITemp;
                                double findVecElemIp1Temp;
                                double derivDataTemp[6];
                                int derivPosTemp;
                                Tcl_GetDoubleFromObj(interp, findVecElems[i], &findVecElemITemp);
                                Tcl_GetDoubleFromObj(interp, findVecElems[i + 1], &findVecElemIp1Temp);
                                double yDeriv = CalcYBetween(xi, findVecElemITemp, xip1, findVecElemIp1Temp, xWhenLoc);
                                DerivSelect(interp, i, xi, xWhenLoc, xip1, xLen, xVecElems, findVecElems, yDeriv,
                                            derivDataTemp, &derivPosTemp);
                                double derYLoc =
                                    Deriv(derivDataTemp[0], derivDataTemp[1], derivDataTemp[2], derivDataTemp[3],
                                          derivDataTemp[4], derivDataTemp[5], derivPosTemp);
                                Tcl_ListObjAppendElement(interp, derYObj, Tcl_NewDoubleObj(derYLoc));
                            }
                        }
                    }
                }
            }
            if ((whenVecCondCount != -2) && (whenVecCondCount != -1)) {
                if ((whenVecCount == whenVecCondCount) && !whenVecFoundFlag) {
                    whenVecFoundFlag = 1;
                    xWhen = CalcCrossPoint(xi, whenVecLSI, xip1, whenVecLSIp1, xi, whenVecRSI, xip1, whenVecRSIp1);
                    Tcl_ListObjAppendElement(interp, xWhenObj, Tcl_NewDoubleObj(xWhen));
                    xWhenSet = 1;
                    if (mode == FDW_SWITCH_FINDWHENEQ) {
                        double findVecElemITemp;
                        double findVecElemIp1Temp;
                        Tcl_GetDoubleFromObj(interp, findVecElems[i], &findVecElemITemp);
                        Tcl_GetDoubleFromObj(interp, findVecElems[i + 1], &findVecElemIp1Temp);
                        yFind = CalcYBetween(xi, findVecElemITemp, xip1, findVecElemIp1Temp, xWhen);
                        Tcl_ListObjAppendElement(interp, yFindObj, Tcl_NewDoubleObj(yFind));
                    } else if (mode == FDW_SWITCH_DERIVWHENEQ) {
                        double findVecElemITemp;
                        double findVecElemIp1Temp;
                        double derivDataTemp[6];
                        int derivPosTemp;
                        Tcl_GetDoubleFromObj(interp, findVecElems[i], &findVecElemITemp);
                        Tcl_GetDoubleFromObj(interp, findVecElems[i + 1], &findVecElemIp1Temp);
                        double yDeriv = CalcYBetween(xi, findVecElemITemp, xip1, findVecElemIp1Temp, xWhen);
                        DerivSelect(interp, i, xi, xWhen, xip1, xLen, xVecElems, findVecElems, yDeriv, derivDataTemp,
                                    &derivPosTemp);
                        derY = Deriv(derivDataTemp[0], derivDataTemp[1], derivDataTemp[2], derivDataTemp[3],
                                     derivDataTemp[4], derivDataTemp[5], derivPosTemp);
                        Tcl_ListObjAppendElement(interp, derYObj, Tcl_NewDoubleObj(derY));
                    }
                    break;
                }
            }
        }
    }
    if ((whenVecCondCount == -1) && (lastWhenHitSet || lastWhenHitCrossSet)) {
        if ((mode == FDW_SWITCH_WHEN) || (mode == FDW_SWITCH_FINDWHEN) || (mode == FDW_SWITCH_DERIVWHEN)) {
            xWhen = CalcXBetween(lastWhenHit[0], lastWhenHit[1], lastWhenHit[2], lastWhenHit[3], val);
            Tcl_ListObjAppendElement(interp, xWhenObj, Tcl_NewDoubleObj(xWhen));
            xWhenSet = 1;
            if (mode == FDW_SWITCH_FINDWHEN) {
                yFind =
                    CalcYBetween(lastFindWhenHit[0], lastFindWhenHit[1], lastFindWhenHit[2], lastFindWhenHit[3], xWhen);
                Tcl_ListObjAppendElement(interp, yFindObj, Tcl_NewDoubleObj(yFind));
            } else if (mode == FDW_SWITCH_DERIVWHEN) {
                derY = Deriv(lastDerYWhenHit[0], lastDerYWhenHit[1], lastDerYWhenHit[2], lastDerYWhenHit[3],
                             lastDerYWhenHit[4], lastDerYWhenHit[5], lastDerYWhenHitPos);
                Tcl_ListObjAppendElement(interp, derYObj, Tcl_NewDoubleObj(derY));
            }
        }
        if ((mode == FDW_SWITCH_WHENEQ) || (mode == FDW_SWITCH_FINDWHENEQ) || (mode == FDW_SWITCH_DERIVWHENEQ)) {
            xWhen = CalcCrossPoint(lastWhenHitCross[0], lastWhenHitCross[1], lastWhenHitCross[2], lastWhenHitCross[3],
                                   lastWhenHitCross[4], lastWhenHitCross[5], lastWhenHitCross[6], lastWhenHitCross[7]);
            Tcl_ListObjAppendElement(interp, xWhenObj, Tcl_NewDoubleObj(xWhen));
            xWhenSet = 1;
            if (mode == FDW_SWITCH_FINDWHENEQ) {
                yFind =
                    CalcYBetween(lastFindWhenHit[0], lastFindWhenHit[1], lastFindWhenHit[2], lastFindWhenHit[3], xWhen);
                Tcl_ListObjAppendElement(interp, yFindObj, Tcl_NewDoubleObj(yFind));
            } else if (mode == FDW_SWITCH_DERIVWHENEQ) {
                derY = Deriv(lastDerYWhenHit[0], lastDerYWhenHit[1], lastDerYWhenHit[2], lastDerYWhenHit[3],
                             lastDerYWhenHit[4], lastDerYWhenHit[5], lastDerYWhenHitPos);
                Tcl_ListObjAppendElement(interp, derYObj, Tcl_NewDoubleObj(derY));
            }
        }
    }
    if (((mode == FDW_SWITCH_WHEN) || (mode == FDW_SWITCH_FINDWHEN) || (mode == FDW_SWITCH_DERIVWHEN)) && !xWhenSet) {
        const char *condition;
        const char *vecCondCount;
        switch ((enum Conditions)whenVecCond) {
        case COND_RISE:
            condition = "rise";
            break;
        case COND_FALL:
            condition = "fall";
            break;
        case COND_CROSS:
            condition = "cross";
            break;
        };
        if (whenVecCondCount == -2) {
            vecCondCount = "all";
        } else if (whenVecCondCount == -1) {
            vecCondCount = "last";
        } else {
            vecCondCount = Tcl_GetString(Tcl_NewWideIntObj(whenVecCondCount));
        }
        Tcl_Obj *errorMsg =
            Tcl_ObjPrintf("When value '%f' with conditions '%s %s delay=%f from=%f to=%f' was not found", val,
                          condition, vecCondCount, delay, from, to);
        Tcl_SetObjResult(interp, errorMsg);
        return TCL_ERROR;
    } else if (((mode == FDW_SWITCH_WHENEQ) || (mode == FDW_SWITCH_FINDWHENEQ) || (mode == FDW_SWITCH_DERIVWHENEQ)) &&
               !xWhenSet) {
        const char *condition;
        const char *vecCondCount;
        switch ((enum Conditions)whenVecCond) {
        case COND_RISE:
            condition = "rise";
            break;
        case COND_FALL:
            condition = "fall";
            break;
        case COND_CROSS:
            condition = "cross";
            break;
        };
        if (whenVecCondCount == -2) {
            vecCondCount = "all";
        } else if (whenVecCondCount == -1) {
            vecCondCount = "last";
        } else {
            vecCondCount = Tcl_GetString(Tcl_NewWideIntObj(whenVecCondCount));
        }
        Tcl_Obj *errorMsg =
            Tcl_ObjPrintf("Cross between vectors with conditions '%s %s delay=%f from=%f to=%f' was not found",
                          condition, vecCondCount, delay, from, to);
        Tcl_SetObjResult(interp, errorMsg);
        return TCL_ERROR;
    }
    if ((mode == FDW_SWITCH_WHEN) || (mode == FDW_SWITCH_WHENEQ)) {
        Tcl_SetObjResult(interp, xWhenObj);
        return TCL_OK;
    } else if ((mode == FDW_SWITCH_FINDWHEN) || (mode == FDW_SWITCH_FINDWHENEQ)) {
        Tcl_SetObjResult(interp, yFindObj);
        return TCL_OK;
    } else if ((mode == FDW_SWITCH_DERIVWHEN) || (mode == FDW_SWITCH_DERIVWHENEQ)) {
        Tcl_SetObjResult(interp, derYObj);
        return TCL_OK;
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * FindAtCmdProc2 --
 *
 *      Implements a Tcl command that performs linear interpolation on a time-aligned signal. Given a scalar X value,
 *      this command scans a sequence of (x, y) points and returns the interpolated Y value at the specified X.
 *      The interpolation uses the segment between two bounding X values that bracket the input X.
 *
 * Parameters:
 *      void *clientData              - input: optional user data (unused)
 *      Tcl_Interp *interp            - input/output: interpreter used for result and error reporting
 *      Tcl_Size objc                 - input: number of command arguments
 *      Tcl_Obj *const objv[]         - input: command arguments, expected as:
 *
 *          objv[1] = x        - list of X (time) values
 *          objv[2] = val      - scalar X value to look up
 *          objv[3] = findVec  - list of Y values aligned with `x`
 *
 * Results:
 *      TCL_OK if `val` is within a segment in `x`; the corresponding interpolated Y value is returned via interpreter.
 *      TCL_ERROR if:
 *          - the number of arguments is invalid,
 *          - any list parsing fails,
 *          - list lengths mismatch,
 *          - `val` lies outside all segment ranges in `x`
 *
 * Side Effects:
 *      Sets interpreter result to the computed Y value or an error message on failure.
 *
 * Notes:
 *      - The function assumes `x` is sorted in ascending order.
 *      - Only the first matching segment where `xi <= val <= xi+1` is used.
 *      - Uses `CalcYBetween` to interpolate linearly between two Y values.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
static int FindAtCmdProc2(void *clientData, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[]) {
    if (objc != 4) {
        Tcl_WrongNumArgs(interp, 3, objv, "x val findVec");
        return TCL_ERROR;
    }
    Tcl_Size xLen, findVecLen;
    Tcl_Obj **xVecElems, **findVecElems;
    if (Tcl_ListObjGetElements(interp, objv[1], &xLen, &xVecElems) == TCL_ERROR) {
        return TCL_ERROR;
    }
    double val;
    Tcl_GetDoubleFromObj(interp, objv[2], &val);
    if (Tcl_ListObjGetElements(interp, objv[3], &findVecLen, &findVecElems) == TCL_ERROR) {
        return TCL_ERROR;
    }
    if (xLen != findVecLen) {
        Tcl_Obj *errorMsg =
            Tcl_ObjPrintf("Length of x '%ld' is not equal to length of findVec '%ld'", xLen, findVecLen);
        Tcl_SetObjResult(interp, errorMsg);
        return TCL_ERROR;
    }
    double yFind;
    int foundFlag = 0;
    for (Tcl_Size i = 0; i < xLen - 1; ++i) {
        double xi, xip1, findVecI, findVecIp1;
        Tcl_GetDoubleFromObj(interp, xVecElems[i], &xi);
        Tcl_GetDoubleFromObj(interp, xVecElems[i + 1], &xip1);
        Tcl_GetDoubleFromObj(interp, findVecElems[i], &findVecI);
        Tcl_GetDoubleFromObj(interp, findVecElems[i + 1], &findVecIp1);
        if ((xi <= val) && (xip1 >= val)) {
            yFind = CalcYBetween(xi, findVecI, xip1, findVecIp1, val);
            foundFlag = 1;
            break;
        }
    }
    if (!foundFlag) {
        Tcl_Obj *errorMsg = Tcl_ObjPrintf("Value of the vector at '%f' was not found", val);
        Tcl_SetObjResult(interp, errorMsg);
        return TCL_ERROR;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewDoubleObj(yFind));
        return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * DerivAtCmdProc2 --
 *
 *      Implements a Tcl command that computes the numerical derivative of a signal vector at a specified X position.
 *      The derivative is estimated using a 3-point stencil around the interpolation point. The function searches for
 *      a segment [xi, xi+1] that brackets the given value and uses neighboring data (when available) for accuracy.
 *
 * Parameters:
 *      void *clientData              - input: optional user data (unused)
 *      Tcl_Interp *interp            - input/output: interpreter for result or error message
 *      Tcl_Size objc                 - input: number of command arguments
 *      Tcl_Obj *const objv[]         - input: command arguments, expected as:
 *
 *          objv[1] = x           - list of X (independent) values
 *          objv[2] = val         - scalar X value where derivative should be evaluated
 *          objv[3] = derivVec    - list of Y (dependent) values aligned with `x`
 *
 * Results:
 *      TCL_OK on success, with interpreter result set to a double representing the estimated derivative.
 *      TCL_ERROR on:
 *          - wrong number of arguments,
 *          - list extraction failure,
 *          - mismatched vector lengths,
 *          - val lying outside the data range
 *
 * Side Effects:
 *      Uses `DerivSelect()` to extract the proper 3-point stencil and compute the interpolated derivative using `Deriv()`.
 *      Sets the interpreter result to either a floating-point derivative or a descriptive error message.
 *
 * Notes:
 *      - Linear interpolation is used to estimate the Y value at `val`, then finite-difference is applied.
 *      - The method adapts to edges (beginning or end of the dataset) using forward/backward biased stencils.
 *      - Requires at least 3 points in `x` and `derivVec` to compute valid derivatives.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
static int DerivAtCmdProc2(void *clientData, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[]) {
    if (objc != 4) {
        Tcl_WrongNumArgs(interp, 3, objv, "x val derivVec");
        return TCL_ERROR;
    }
    Tcl_Size xLen, derivVecLen;
    Tcl_Obj **xVecElems, **derivVecElems;
    if (Tcl_ListObjGetElements(interp, objv[1], &xLen, &xVecElems) == TCL_ERROR) {
        return TCL_ERROR;
    }
    double val;
    Tcl_GetDoubleFromObj(interp, objv[2], &val);
    if (Tcl_ListObjGetElements(interp, objv[3], &derivVecLen, &derivVecElems) == TCL_ERROR) {
        return TCL_ERROR;
    }
    if (xLen != derivVecLen) {
        Tcl_Obj *errorMsg =
            Tcl_ObjPrintf("Length of x '%ld' is not equal to length of derivVec '%ld'", xLen, derivVecLen);
        Tcl_SetObjResult(interp, errorMsg);
        return TCL_ERROR;
    }
    double yDeriv, derY;
    int foundFlag = 0;
    for (Tcl_Size i = 0; i < xLen - 1; ++i) {
        double xi, xip1, derivVecI, derivVecIp1;
        Tcl_GetDoubleFromObj(interp, xVecElems[i], &xi);
        Tcl_GetDoubleFromObj(interp, xVecElems[i + 1], &xip1);
        Tcl_GetDoubleFromObj(interp, derivVecElems[i], &derivVecI);
        Tcl_GetDoubleFromObj(interp, derivVecElems[i + 1], &derivVecIp1);
        if ((xi <= val) && (xip1 >= val)) {
            double derivDataTemp[6];
            int derivPosTemp;
            yDeriv = CalcYBetween(xi, derivVecI, xip1, derivVecIp1, val);
            DerivSelect(interp, i, xi, val, xip1, xLen, xVecElems, derivVecElems, yDeriv, derivDataTemp, &derivPosTemp);
            derY = Deriv(derivDataTemp[0], derivDataTemp[1], derivDataTemp[2], derivDataTemp[3], derivDataTemp[4],
                         derivDataTemp[5], derivPosTemp);
            foundFlag = 1;
            break;
        }
    }
    if (!foundFlag) {
        Tcl_Obj *errorMsg = Tcl_ObjPrintf("Derivative of the vector at '%f' was not found", val);
        Tcl_SetObjResult(interp, errorMsg);
        return TCL_ERROR;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewDoubleObj(derY));
        return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * IntegCmdProc2 --
 *
 *      Implements a Tcl command that numerically integrates a Y vector over a given interval in X using the trapezoidal
 *      rule. Supports optional output of cumulative integral values over the integration domain.
 *
 * Parameters:
 *      void *clientData              - input: optional user data (unused)
 *      Tcl_Interp *interp            - input/output: Tcl interpreter for error and result handling
 *      Tcl_Size objc                 - input: number of command arguments
 *      Tcl_Obj *const objv[]         - input: command arguments, expected as:
 *
 *          objv[1] = x        - list of X values (time domain)
 *          objv[2] = y        - list of Y values (to integrate over X)
 *          objv[3] = xstart   - start of the integration interval (must lie within `x`)
 *          objv[4] = xend     - end of the integration interval (must lie within `x`)
 *          objv[5] = cum      - boolean flag; if true, return cumulative integral series as dict with "x" and "y" keys
 *
 * Results:
 *      TCL_OK on success:
 *          - If `cum` is false: interpreter result is the total integral (double)
 *          - If `cum` is true: result is a dict with keys:
 *              "x" => list of X values from the integration path
 *              "y" => list of cumulative integral values
 *
 *      TCL_ERROR on:
 *          - invalid argument count
 *          - non-numeric or non-list inputs
 *          - mismatched lengths between `x` and `y`
 *          - `xstart`/`xend` outside the `x` domain
 *          - `xstart >= xend`
 *
 * Side Effects:
 *      Uses `CalcYBetween()` to interpolate values at exact `xstart` and `xend` positions for accurate integration.
 *      Accumulates the integral using trapezoidal rule:
 *          ∫(a to b) y dx ≈ Σ [(yi + yi+1)/2] * (xi+1 - xi)
 *      When `cum` is enabled, builds Tcl lists for cumulative output.
 *
 * Notes:
 *      - Assumes `x` is monotonically increasing.
 *      - All interpolation and accumulation is performed in double precision.
 *      - Caller is responsible for ensuring signal alignment and sampling accuracy.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
static int IntegCmdProc2(void *clientData, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[]) {
    if (objc != 6) {
        Tcl_WrongNumArgs(interp, 5, objv, "x y xstart xend cum");
        return TCL_ERROR;
    }
    Tcl_Size xLen, yLen;
    Tcl_Obj **xElems, **yElems;
    if (Tcl_ListObjGetElements(interp, objv[1], &xLen, &xElems) == TCL_ERROR) {
        return TCL_ERROR;
    }
    if (Tcl_ListObjGetElements(interp, objv[2], &yLen, &yElems) == TCL_ERROR) {
        return TCL_ERROR;
    }
    double xstart;
    Tcl_GetDoubleFromObj(interp, objv[3], &xstart);
    double xend;
    Tcl_GetDoubleFromObj(interp, objv[4], &xend);
    int cumFlag;
    Tcl_GetBooleanFromObj(interp, objv[5], &cumFlag);
    Tcl_Obj *xCum = NULL;
    Tcl_Obj *yCum = NULL;
    if (cumFlag) {
        xCum = Tcl_NewListObj(0, NULL);
        yCum = Tcl_NewListObj(0, NULL);
    }
    if (xLen != yLen) {
        Tcl_Obj *errorMsg = Tcl_ObjPrintf("Length of x '%ld' is not equal to length of y '%ld'", xLen, yLen);
        Tcl_SetObjResult(interp, errorMsg);
        return TCL_ERROR;
    }
    double xActualStart, xActualEnd;
    Tcl_GetDoubleFromObj(interp, xElems[0], &xActualStart);
    Tcl_GetDoubleFromObj(interp, xElems[xLen - 1], &xActualEnd);
    if (xstart < xActualStart) {
        Tcl_Obj *errorMsg = Tcl_ObjPrintf("Start of integration interval '%f' is outside the x values range", xstart);
        Tcl_SetObjResult(interp, errorMsg);
        return TCL_ERROR;
    } else if (xend > xActualEnd) {
        Tcl_Obj *errorMsg = Tcl_ObjPrintf("End of integration interval '%f' is outside the x values range", xend);
        Tcl_SetObjResult(interp, errorMsg);
        return TCL_ERROR;
    } else if (xstart >= xend) {
        Tcl_SetObjResult(
            interp, Tcl_NewStringObj("Start of the integration should be lower than the end of the integration", -1));
        return TCL_ERROR;
    }
    int startFlagFound = 0;
    int endFlagFound = 0;
    double ystart = 0, yend;
    int istart = 0, iend;
    double result = 0.0;
    for (Tcl_Size i = 0; i < xLen - 1; ++i) {
        double xi, xip1, yi, yip1;
        Tcl_GetDoubleFromObj(interp, xElems[i], &xi);
        Tcl_GetDoubleFromObj(interp, xElems[i + 1], &xip1);
        Tcl_GetDoubleFromObj(interp, yElems[i], &yi);
        Tcl_GetDoubleFromObj(interp, yElems[i + 1], &yip1);
        if ((xi <= xstart) && (xip1 >= xstart) && !startFlagFound) {
            ystart = CalcYBetween(xi, yi, xip1, yip1, xstart);
            istart = i;
            startFlagFound = 1;
        } else if ((xi <= xend) && (xip1 >= xend) && !endFlagFound) {
            yend = CalcYBetween(xi, yi, xip1, yip1, xend);
            iend = i;
            endFlagFound = 1;
        }
        if (startFlagFound) {
            if (i == istart) {
                xi = xstart;
                yi = ystart;
                result = result + (yip1 + yi) / 2.0 * (xip1 - xi);
                if (cumFlag) {
                    Tcl_ListObjAppendElement(interp, xCum, Tcl_NewDoubleObj(xi));
                    Tcl_ListObjAppendElement(interp, yCum, Tcl_NewDoubleObj(result));
                }
                continue;
            } else if (endFlagFound) {
                if (i == iend) {
                    xip1 = xend;
                    yip1 = yend;
                    result = result + (yip1 + yi) / 2.0 * (xip1 - xi);
                    if (cumFlag) {
                        Tcl_ListObjAppendElement(interp, xCum, Tcl_NewDoubleObj(xip1));
                        Tcl_ListObjAppendElement(interp, yCum, Tcl_NewDoubleObj(result));
                    }
                    break;
                }
            } else {
                result = result + (yip1 + yi) / 2.0 * (xip1 - xi);
                if (cumFlag) {
                    Tcl_ListObjAppendElement(interp, xCum, Tcl_NewDoubleObj(xi));
                    Tcl_ListObjAppendElement(interp, yCum, Tcl_NewDoubleObj(result));
                }
            }
        }
    }
    if (cumFlag) {
        Tcl_Obj *resultDict = Tcl_NewDictObj();
        Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("x", -1), xCum);
        Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("y", -1), yCum);
        Tcl_SetObjResult(interp, resultDict);
        return TCL_OK;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewDoubleObj(result));
        return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * findMinObj --
 *
 *      Find the minimum value among a list of Tcl_Obj pointers, each representing a numeric value. The function parses
 *      all elements as doubles and returns the smallest one.
 *
 * Parameters:
 *      Tcl_Interp *interp        - input/output: interpreter for error reporting if conversion fails
 *      Tcl_Obj *const objv[]     - input: array of Tcl_Obj pointers (assumed to be numeric values)
 *      Tcl_Size len              - input: number of elements in the `objv` array
 *      double *result            - output: pointer to store the minimum value found
 *
 * Results:
 *      TCL_OK on success, and *result is set to the minimum double value from the array
 *      TCL_ERROR if:
 *          - len is zero or negative
 *          - any element in `objv` fails conversion to double (error message set in interpreter)
 *
 * Side Effects:
 *      On failure, sets an error message in the interpreter result
 *
 * Notes:
 *      - Uses `fmin()` to preserve IEEE behavior with NaNs if present.
 *      - All numeric values are interpreted as double precision.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
int findMinObj(Tcl_Interp *interp, Tcl_Obj *const objv[], Tcl_Size len, double *result) {
    if (len <= 0)
        return TCL_ERROR;
    double min;
    if (Tcl_GetDoubleFromObj(interp, objv[0], &min) != TCL_OK)
        return TCL_ERROR;
    for (Tcl_Size i = 1; i < len; i++) {
        double val;
        if (Tcl_GetDoubleFromObj(interp, objv[i], &val) != TCL_OK)
            return TCL_ERROR;
        min = fmin(min, val);
    }
    *result = min;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * findMaxObj --
 *
 *      Find the maximum value among a list of Tcl_Obj pointers, each representing a numeric value. The function parses
 *      all elements as doubles and returns the largest one.
 *
 * Parameters:
 *      Tcl_Interp *interp        - input/output: interpreter for error reporting if conversion fails
 *      Tcl_Obj *const objv[]     - input: array of Tcl_Obj pointers (expected to hold numeric values)
 *      Tcl_Size len              - input: number of elements in the `objv` array
 *      double *result            - output: pointer to store the maximum value found
 *
 * Results:
 *      TCL_OK on success, and *result is set to the maximum double value from the array
 *      TCL_ERROR if:
 *          - len is zero or negative
 *          - any element in `objv` cannot be converted to a double (error set in interpreter)
 *
 * Side Effects:
 *      On failure, sets an error message in the interpreter result
 *
 * Notes:
 *      - Uses `fmax()` to ensure IEEE compliance (e.g., handling NaNs).
 *      - All values are treated as double-precision floating point.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
int findMaxObj(Tcl_Interp *interp, Tcl_Obj *const objv[], Tcl_Size len, double *result) {
    if (len <= 0)
        return TCL_ERROR;
    double max;
    if (Tcl_GetDoubleFromObj(interp, objv[0], &max) != TCL_OK)
        return TCL_ERROR;
    for (Tcl_Size i = 1; i < len; i++) {
        double val;
        if (Tcl_GetDoubleFromObj(interp, objv[i], &val) != TCL_OK)
            return TCL_ERROR;
        max = fmax(max, val);
    }
    *result = max;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * findMinIndexObj --
 *
 *      Find the index of the minimum value in an array of Tcl_Obj pointers. Each element is expected to be a numeric
 *      Tcl object. The function scans the list, converts values to doubles, and returns the index of the smallest one.
 *
 * Parameters:
 *      Tcl_Interp *interp        - input/output: interpreter used for error reporting on conversion failure
 *      Tcl_Obj *const objv[]     - input: array of Tcl_Obj pointers (expected to contain numeric values)
 *      Tcl_Size len              - input: number of elements in the array
 *      Tcl_Size *index           - output: pointer to store the index of the minimum value found
 *
 * Results:
 *      TCL_OK on success, with *index set to the position of the minimum value
 *      TCL_ERROR if:
 *          - len is zero or negative
 *          - any element cannot be converted to a double
 *
 * Side Effects:
 *      On error, sets a message in the interpreter result
 *
 * Notes:
 *      - Uses strict `<` comparison; if multiple equal minimum values exist, the first is returned.
 *      - All values are interpreted as doubles during comparison.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
int findMinIndexObj(Tcl_Interp *interp, Tcl_Obj *const objv[], Tcl_Size len, Tcl_Size *index) {
    if (len <= 0)
        return TCL_ERROR;
    double minVal;
    if (Tcl_GetDoubleFromObj(interp, objv[0], &minVal) != TCL_OK)
        return TCL_ERROR;
    Tcl_Size minIdx = 0;
    for (Tcl_Size i = 1; i < len; i++) {
        double val;
        if (Tcl_GetDoubleFromObj(interp, objv[i], &val) != TCL_OK)
            return TCL_ERROR;
        if (val < minVal) {
            minVal = val;
            minIdx = i;
        }
    }
    *index = minIdx;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * findMaxIndexObj --
 *
 *      Find the index of the maximum value in an array of Tcl_Obj pointers. Each element is expected to represent
 *      a numeric value. The function converts each element to a double and returns the index of the largest one.
 *
 * Parameters:
 *      Tcl_Interp *interp        - input/output: interpreter used for error reporting if conversion fails
 *      Tcl_Obj *const objv[]     - input: array of Tcl_Obj pointers (assumed to hold numeric values)
 *      Tcl_Size len              - input: number of elements in the array
 *      Tcl_Size *index           - output: pointer to store the index of the maximum value
 *
 * Results:
 *      TCL_OK on success, with *index set to the position of the maximum value
 *      TCL_ERROR if:
 *          - len is zero or negative
 *          - any element fails to convert to a double
 *
 * Side Effects:
 *      On failure, sets an error message in the interpreter
 *
 * Notes:
 *      - Uses strict `>` comparison; if multiple equal maximum values exist, the first is returned.
 *      - Comparison and value extraction are performed using double-precision floating point.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
int findMaxIndexObj(Tcl_Interp *interp, Tcl_Obj *const objv[], Tcl_Size len, Tcl_Size *index) {
    if (len <= 0)
        return TCL_ERROR;
    double maxVal;
    if (Tcl_GetDoubleFromObj(interp, objv[0], &maxVal) != TCL_OK)
        return TCL_ERROR;
    Tcl_Size maxIdx = 0;
    for (Tcl_Size i = 1; i < len; i++) {
        double val;
        if (Tcl_GetDoubleFromObj(interp, objv[i], &val) != TCL_OK)
            return TCL_ERROR;
        if (val > maxVal) {
            maxVal = val;
            maxIdx = i;
        }
    }
    *index = maxIdx;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * findPPObj --
 *
 *      Compute the peak-to-peak (PP) range of a list of Tcl_Obj values by summing the absolute values of the minimum
 *      and maximum elements. Each value is parsed as a double.
 *
 * Parameters:
 *      Tcl_Interp *interp        - input/output: interpreter used for error reporting if conversion fails
 *      Tcl_Obj *const objv[]     - input: array of Tcl_Obj pointers (expected to contain numeric values)
 *      Tcl_Size len              - input: number of elements in the array
 *      double *result            - output: pointer to store the computed peak-to-peak range
 *
 * Results:
 *      TCL_OK on success, with *result set to fabs(min) + fabs(max)
 *      TCL_ERROR if:
 *          - `findMinObj` or `findMaxObj` fails (due to invalid input or empty list)
 *
 * Side Effects:
 *      May set an error message in the interpreter if conversion fails
 *
 * Notes:
 *      - Unlike traditional peak-to-peak (max - min), this function returns `fabs(min) + fabs(max)`
 *        which is useful for symmetric or absolute range calculations.
 *      - All values are interpreted as double-precision numbers.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
int findPPObj(Tcl_Interp *interp, Tcl_Obj *const objv[], Tcl_Size len, double *result) {
    double min, max;
    if (findMinObj(interp, objv, len, &min) != TCL_OK)
        return TCL_ERROR;
    if (findMaxObj(interp, objv, len, &max) != TCL_OK)
        return TCL_ERROR;
    *result = fabs(min) + fabs(max);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * ListRange --
 *
 *      Create a new Tcl list consisting of a prefix element (`firstObj`), a subrange of elements from `listObj`,
 *      and a suffix element (`lastObj`). The subrange is defined by the `start` and `end` indices (inclusive).
 *
 * Parameters:
 *      Tcl_Interp *interp        - input: interpreter used for memory management (and list manipulation)
 *      Tcl_Obj *listObj          - input: Tcl list object from which to extract a subrange
 *      Tcl_Size start            - input: starting index of the subrange (inclusive)
 *      Tcl_Size end              - input: ending index of the subrange (inclusive)
 *      Tcl_Obj *firstObj         - input: object to prepend to the result list
 *      Tcl_Obj *lastObj          - input: object to append to the result list
 *
 * Results:
 *      Returns a newly created Tcl list with the structure:
 *          [firstObj elem(start) ... elem(end) lastObj]
 *
 *      If the start/end indices are out of range or empty, the result will contain only:
 *          [firstObj lastObj]
 *
 * Side Effects:
 *      None (allocates a new list Tcl_Obj and returns it)
 *
 * Notes:
 *      - If `start >= listLen`, an empty subrange is used.
 *      - If `end >= listLen`, it is clamped to `listLen - 1`.
 *      - If `start > end` or the list is too short, no subrange is included between `firstObj` and `lastObj`.
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
Tcl_Obj *ListRange(Tcl_Interp *interp, Tcl_Obj *listObj, Tcl_Size start, Tcl_Size end, Tcl_Obj *firstObj,
                   Tcl_Obj *lastObj) {
    Tcl_Obj **elemPtrs;
    Tcl_Size listLen;
    Tcl_ListObjGetElements(interp, listObj, &listLen, &elemPtrs);
    if (start >= listLen) {
        start = listLen;
    }
    if (end >= listLen) {
        end = listLen - 1;
    }
    if (start > end || start >= listLen) {
        return Tcl_NewListObj(0, NULL);
    }
    Tcl_Size rangeLen = end - start + 1;
    Tcl_Obj *resultList = Tcl_NewListObj(0, NULL);
    Tcl_ListObjAppendElement(interp, resultList, firstObj);
    Tcl_ListObjAppendList(interp, resultList, Tcl_NewListObj(rangeLen, &elemPtrs[start]));
    Tcl_ListObjAppendElement(interp, resultList, lastObj);
    return resultList;
}

/*
 *----------------------------------------------------------------------------------------------------------------------
 *
 * MinMaxPPMinAtMaxAtCmdProc2 --
 *
 *      Implements a Tcl command that analyzes a segment of a (x, y) signal and computes a statistical result over
 *      that interval. Supported result types include min/max values, peak-to-peak range, and the x-location of
 *      the min/max value. The interval is defined by `xstart` and `xend`, and the command supports optional
 *      extraction of the segment as a dictionary.
 *
 * Parameters:
 *      void *clientData              - input: optional user data (unused)
 *      Tcl_Interp *interp            - input/output: interpreter for result and error reporting
 *      Tcl_Size objc                 - input: number of command arguments
 *      Tcl_Obj *const objv[]         - input: command arguments, expected as:
 *
 *          objv[1] = x        - list of X values (monotonically increasing)
 *          objv[2] = y        - list of Y values (aligned with X)
 *          objv[3] = xstart   - start of the range (inclusive)
 *          objv[4] = xend     - end of the range (inclusive)
 *          objv[5] = type     - operation to perform:
 *                                  "min"     => minimum value of y in range
 *                                  "max"     => maximum value of y in range
 *                                  "pp"      => peak-to-peak (fabs(min) + fabs(max))
 *                                  "minat"   => x value at which min(y) occurs
 *                                  "maxat"   => x value at which max(y) occurs
 *                                  "between" => dict with "x" and "y" lists between xstart and xend
 *
 * Results:
 *      TCL_OK with one of the following:
 *          - A double value (min, max, pp)
 *          - A scalar X value where the min or max occurs
 *          - A dictionary {x -> list, y -> list} of the subrange
 *
 *      TCL_ERROR on:
 *          - incorrect argument count
 *          - failure to parse X or Y lists
 *          - mismatched X/Y lengths
 *          - xstart/xend outside of X range or invalid interval
 *          - unrecognized type keyword
 *
 * Side Effects:
 *      - Performs interpolation at the edges of the integration interval using `CalcYBetween`
 *      - Creates temporary lists to isolate subranges of x and y for processing
 *      - Allocates and returns result as either a scalar, list, or dictionary
 *
 * Notes:
 *      - The range [xstart, xend] must lie entirely within the input X domain
 *      - Subrange data includes interpolated boundary points at xstart and xend
 *      - Uses helper functions: `findMinObj`, `findMaxObj`, `findPPObj`, `findMinIndexObj`, `findMaxIndexObj`
 *      - Requires at least 2 X/Y samples in the interval to function correctly
 *
 *----------------------------------------------------------------------------------------------------------------------
 */
static int MinMaxPPMinAtMaxAtCmdProc2(void *clientData, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[]) {
    if (objc != 6) {
        Tcl_WrongNumArgs(interp, 5, objv, "x y xstart xend type");
        return TCL_ERROR;
    }
    Tcl_Size xLen, yLen;
    Tcl_Obj **xElems, **yElems;
    if (Tcl_ListObjGetElements(interp, objv[1], &xLen, &xElems) == TCL_ERROR) {
        return TCL_ERROR;
    }
    if (Tcl_ListObjGetElements(interp, objv[2], &yLen, &yElems) == TCL_ERROR) {
        return TCL_ERROR;
    }
    double xstart;
    Tcl_GetDoubleFromObj(interp, objv[3], &xstart);
    double xend;
    Tcl_GetDoubleFromObj(interp, objv[4], &xend);
    int type;
    const char *typeStr = Tcl_GetString(objv[5]);
    if (!strcmp(typeStr, "min")) {
        type = TYPE_MIN;
    } else if (!strcmp(typeStr, "max")) {
        type = TYPE_MAX;
    } else if (!strcmp(typeStr, "pp")) {
        type = TYPE_PP;
    } else if (!strcmp(typeStr, "minat")) {
        type = TYPE_MINAT;
    } else if (!strcmp(typeStr, "maxat")) {
        type = TYPE_MAXAT;
    } else if (!strcmp(typeStr, "between")) {
        type = TYPE_BETWEEN;
    } else {
        return TCL_ERROR;
    }
    if (xLen != yLen) {
        Tcl_Obj *errorMsg = Tcl_ObjPrintf("Length of x '%ld' is not equal to length of y '%ld'", xLen, yLen);
        Tcl_SetObjResult(interp, errorMsg);
        return TCL_ERROR;
    }
    double xActualStart, xActualEnd;
    Tcl_GetDoubleFromObj(interp, xElems[0], &xActualStart);
    Tcl_GetDoubleFromObj(interp, xElems[xLen - 1], &xActualEnd);
    if (xstart < xActualStart) {
        Tcl_Obj *errorMsg = Tcl_ObjPrintf("Start of integration interval '%f' is outside the x values range", xstart);
        Tcl_SetObjResult(interp, errorMsg);
        return TCL_ERROR;
    } else if (xend > xActualEnd) {
        Tcl_Obj *errorMsg = Tcl_ObjPrintf("End of integration interval '%f' is outside the x values range", xend);
        Tcl_SetObjResult(interp, errorMsg);
        return TCL_ERROR;
    } else if (xstart >= xend) {
        Tcl_SetObjResult(
            interp, Tcl_NewStringObj("Start of the integration should be lower than the end of the integration", -1));
        return TCL_ERROR;
    }
    int startFlagFound = 0;
    int endFlagFound = 0;
    double ystart = 0, yend;
    int istart = 0, iend;
    for (Tcl_Size i = 0; i < xLen - 1; ++i) {
        double xi, xip1, yi, yip1;
        Tcl_GetDoubleFromObj(interp, xElems[i], &xi);
        Tcl_GetDoubleFromObj(interp, xElems[i + 1], &xip1);
        Tcl_GetDoubleFromObj(interp, yElems[i], &yi);
        Tcl_GetDoubleFromObj(interp, yElems[i + 1], &yip1);
        if ((xi <= xstart) && (xip1 >= xstart) && !startFlagFound) {
            ystart = CalcYBetween(xi, yi, xip1, yip1, xstart);
            istart = i;
            startFlagFound = 1;
        } else if ((xi <= xend) && (xip1 >= xend) && !endFlagFound) {
            yend = CalcYBetween(xi, yi, xip1, yip1, xend);
            iend = i;
            endFlagFound = 1;
            break;
        }
    }
    Tcl_Obj *targetArrayObjs;
    Tcl_Obj **targetArrayObjsElems;
    Tcl_Size targetArrayLen;
    if (endFlagFound) {
        targetArrayObjs =
            ListRange(interp, objv[2], istart + 1, iend, Tcl_NewDoubleObj(ystart), Tcl_NewDoubleObj(yend));
        if (Tcl_ListObjGetElements(interp, targetArrayObjs, &targetArrayLen, &targetArrayObjsElems) == TCL_ERROR) {
            return TCL_ERROR;
        }
        double result;
        Tcl_Obj *targetXArrayObjs;
        Tcl_Obj **targetXArrayObjsElems;
        Tcl_Size targetXArrayLen;
        switch ((enum Types)type) {
        case TYPE_MIN:
            findMinObj(interp, targetArrayObjsElems, targetArrayLen, &result);
            Tcl_SetObjResult(interp, Tcl_NewDoubleObj(result));
            break;
        case TYPE_MAX:
            findMaxObj(interp, targetArrayObjsElems, targetArrayLen, &result);
            Tcl_SetObjResult(interp, Tcl_NewDoubleObj(result));
            break;
        case TYPE_PP:
            findPPObj(interp, targetArrayObjsElems, targetArrayLen, &result);
            Tcl_SetObjResult(interp, Tcl_NewDoubleObj(result));
            break;
        case TYPE_MINAT:
            targetXArrayObjs =
                ListRange(interp, objv[1], istart + 1, iend, Tcl_NewDoubleObj(xstart), Tcl_NewDoubleObj(xend));
            if (Tcl_ListObjGetElements(interp, targetXArrayObjs, &targetXArrayLen, &targetXArrayObjsElems) ==
                TCL_ERROR) {
                return TCL_ERROR;
            }
            Tcl_Size minIndex;
            findMinIndexObj(interp, targetArrayObjsElems, targetArrayLen, &minIndex);
            Tcl_SetObjResult(interp, targetXArrayObjsElems[minIndex]);
            break;
        case TYPE_MAXAT:
            targetXArrayObjs =
                ListRange(interp, objv[1], istart + 1, iend, Tcl_NewDoubleObj(xstart), Tcl_NewDoubleObj(xend));
            if (Tcl_ListObjGetElements(interp, targetXArrayObjs, &targetXArrayLen, &targetXArrayObjsElems) ==
                TCL_ERROR) {
                return TCL_ERROR;
            }
            Tcl_Size maxIndex;
            findMaxIndexObj(interp, targetArrayObjsElems, targetArrayLen, &maxIndex);
            Tcl_SetObjResult(interp, targetXArrayObjsElems[maxIndex]);
            break;
        case TYPE_BETWEEN:
            targetXArrayObjs =
                ListRange(interp, objv[1], istart + 1, iend, Tcl_NewDoubleObj(xstart), Tcl_NewDoubleObj(xend));
            Tcl_Obj *resultDict = Tcl_NewDictObj();
            Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("x", -1), targetXArrayObjs);
            Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("y", -1), targetArrayObjs);
            Tcl_SetObjResult(interp, resultDict);
            break;
        };
        return TCL_OK;
    } else {
        return TCL_ERROR;
    }
}
