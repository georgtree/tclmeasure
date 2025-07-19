
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
    /* Tcl_CreateObjCommand2(interp, "::tclmeasure::FindDerivWhen", (Tcl_ObjCmdProc2 *)FindDerivWhenCmdProc2, NULL, NULL); */
    /* Tcl_CreateObjCommand2(interp, "::tclmeasure::FindAt", (Tcl_ObjCmdProc2 *)FindAtCmdProc2, NULL, NULL); */
    /* Tcl_CreateObjCommand2(interp, "::tclmeasure::DerivAt", (Tcl_ObjCmdProc2 *)DerivAtCmdProc2, NULL, NULL); */
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
        Tcl_Obj *errorMsg = Tcl_ObjPrintf("Length of x '%ld' is not equal to length of trigVec '%ld'", xLen, trigVecLen);
        Tcl_SetObjResult(interp, errorMsg);
        return TCL_ERROR;
    } else if (trigVecLen != targVecLen) {
        Tcl_Obj *errorMsg = Tcl_ObjPrintf("Length of trigVec '%ld' is not equal to length of targVec '%ld'", trigVecLen,
                                          targVecLen);
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
