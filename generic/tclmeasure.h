#include <tcl.h>

enum Conditions {
    COND_RISE = 0,
    COND_FALL,
    COND_CROSS
};

const char *TclGetUnqualifiedName(const char *qualifiedName);
extern DLLEXPORT int Tclmeasure_Init(Tcl_Interp *interp);
static inline double CalcXBetween(double x1, double y1, double x2, double y2, double yBetween);
static inline double CalcYBetween(double x1, double y1, double x2, double y2, double xBetween);
static inline double CalcCrossPoint(double x11, double y11, double x21, double y21, double x12, double y12, double x22,
                                    double y22);
static int TrigTargCmdProc2(void *clientData, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[]);
