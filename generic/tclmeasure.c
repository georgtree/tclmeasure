
#include "tclmeasure.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>

const char *TclGetUnqualifiedName(const char *qualifiedName) {
    const char *p = qualifiedName;
    const char *last = qualifiedName;

    while ((p = strstr(p, "::")) != NULL) {
        p += 2;
        last = p;
    }
    return last;
}

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
    /* Tcl_CreateObjCommand2(interp, "::tclmeasure::Integ", (Tcl_ObjCmdProc2 *)IntegCmdProc2, NULL, NULL); */
    /* Tcl_CreateObjCommand2(interp, "::tclmeasure::Avg", (Tcl_ObjCmdProc2 *)AvgCmdProc2, NULL, NULL); */
    /* Tcl_CreateObjCommand2(interp, "::tclmeasure::Rms", (Tcl_ObjCmdProc2 *)RmsCmdProc2, NULL, NULL); */
    /* Tcl_CreateObjCommand2(interp, "::tclmeasure::MinMaxPPMinAtMaxAt", (Tcl_ObjCmdProc2 *)MinMaxPPMinAtMaxAtCmdProc2, */
    /*                       NULL, NULL); */
    return TCL_OK;
}

static inline double CalcXBetween(double x1, double y1, double x2, double y2, double yBetween) {
    return (yBetween - y1) * (x2 - x1) / (y2 - y1) + x1;
}
static inline double CalcYBetween(double x1, double y1, double x2, double y2, double xBetween) {
    return (y2 - y1) / (x2 - x1) * (xBetween - x1) + y1;
}
static inline double CalcCrossPoint(double x11, double y11, double x21, double y21, double x12, double y12, double x22,
                                    double y22) {
    return (y12-(y22-y12)/(x22-x12)*x12-(y11-(y21-y11)/(x21-x11)*x11))/((y21-y11)/(x21-x11)-(y22-y12)/(x22-x12));
}

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
    //return 0.0;
}

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
        Tcl_GetDoubleFromObj(interp, xVecElems[i+1], &xip1);
        Tcl_GetDoubleFromObj(interp, trigVecElems[i], &trigVecI);
        Tcl_GetDoubleFromObj(interp, trigVecElems[i+1], &trigVecIp1);
        Tcl_GetDoubleFromObj(interp, targVecElems[i], &targVecI);
        Tcl_GetDoubleFromObj(interp, targVecElems[i+1], &targVecIp1);
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
    Tcl_DictObjPut(interp, result, Tcl_NewStringObj("xdelta", -1), Tcl_NewDoubleObj(xTarg-xTrig));
    Tcl_SetObjResult(interp, result);
    return TCL_OK;
}

static int FindDerivWhenCmdProc2(void *clientData, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[]) {
    double lastWhenHit[4] = {0.0, 0.0, 0.0, 0.0};
    int lastWhenHitSet = 0;
    double lastWhenHitCross[8] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    int lastWhenHitCrossSet = 0;
    double lastFindWhenHit[4] = {0.0, 0.0, 0.0, 0.0};
    int lastFindWhenHitSet = 0;
    double lastDerYWhenHit[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    int lastDerYWhenHitSet = 0;
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
                            lastFindWhenHitSet = 1;
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
                                lastFindWhenHitSet = 1;
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
        Tcl_Obj *errorMsg = Tcl_ObjPrintf("Length of x '%ld' is not equal to length of findVec '%ld'", xLen, findVecLen);
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
            DerivSelect(interp, i, xi, val, xip1, xLen, xVecElems, derivVecElems, yDeriv, derivDataTemp,
                        &derivPosTemp);
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
