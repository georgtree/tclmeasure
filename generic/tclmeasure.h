#include <tcl.h>

enum Conditions {
    COND_RISE = 0,
    COND_FALL,
    COND_CROSS
};
enum FindDerivWhenSwitchId {
    FDW_SWITCH_WHEN = 0,
    FDW_SWITCH_WHENEQ,
    FDW_SWITCH_FINDWHEN,
    FDW_SWITCH_DERIVWHEN,
    FDW_SWITCH_FINDWHENEQ,
    FDW_SWITCH_DERIVWHENEQ,
    FDW_SWITCH_COUNT // total number of global switches
};
static const char *FindDerivWhenSwitches[] = {"when",       "wheneq",      "findwhen", "derivwhen",
                                              "findwheneq", "derivwheneq", NULL};

const char *TclGetUnqualifiedName(const char *qualifiedName);
extern DLLEXPORT int Tclmeasure_Init(Tcl_Interp *interp);
static inline double CalcXBetween(double x1, double y1, double x2, double y2, double yBetween);
static inline double CalcYBetween(double x1, double y1, double x2, double y2, double xBetween);
static inline double CalcCrossPoint(double x11, double y11, double x21, double y21, double x12, double y12, double x22,
                                    double y22);
static int TrigTargCmdProc2(void *clientData, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[]);
static int FindDerivWhenCmdProc2(void *clientData, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[]);
static void DerivSelect(Tcl_Interp *interp, Tcl_WideInt i, double xi, double xwhen, double xip1, Tcl_WideInt xlen,
                        Tcl_Obj **x, Tcl_Obj **vec, double ywhen, double *out, int *pos);
static double Deriv(double xim1, double xi, double xip1, double yim1, double yi, double yip1, int type);
