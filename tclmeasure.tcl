package require argparse 0.58-
package provide tclmeasure 0.1

namespace eval ::tclmeasure {

    namespace import ::tcl::mathop::*
    namespace export tclmeasure
    interp alias {} dget {} dict get
    interp alias {} @ {} lindex
    interp alias {} = {} expr
    interp alias {} dexist {} dict exists
    interp alias {} dcreate {} dict create
}

proc ::tclmeasure::AliasesKeysCheck {arguments keys} {
    foreach key $keys {
        if {[dexist $arguments $key]} {
            return $key
        }
    }
    set formKeys [lmap key $keys {subst -$key}]
    return -code error "[join [lrange $formKeys 0 end-1] ", "] or [@ $formKeys end] must be presented"
}

proc ::tclmeasure::FromTo {argsDict data xname} {
    if {![dexist $argsDict from]} {
        set from [@ [dget $data $xname] 0]
    } else {
        set from [dget $argsDict from]
    }
    if {![dexist $argsDict to]} {
        set to [@ [dget $data $xname] end]
    } else {
        set to [dget $argsDict to]
    }
    uplevel 1 [list set from $from]
    uplevel 1 [list set to $to] 
}

proc ::tclmeasure::measure {args} {
    # Does different measurements of input data lists.
    #  -xname - name of x list in data dictionary. This list must be strictly increaing without duplicate elements.
    #  -data - dictionary that contains lists with names as the keys and lists as the values.
    #  -trig - contains conditions for trigger (see below), selects Trigger-Target measurement, requires -targ
    #  -targ - contains conditions for target (see below), requires -trig
    #  -find - contains conditions for find (see below), requires -when or -at
    #  -deriv - contains conditions for deriv (see below), requires -when or -at
    #  -when - contains conditions for when (see below)
    #  -at - time for -find or -deriv
    #  -avg - contains conditions for finding average value across the interval
    #  -rms - contains conditions for finding root meas square value across the interval
    #  -min - contains conditions for finding minimum value in the interval
    #  -max - contains conditions for finding maximum value in the interval
    #  -pp - contains conditions for finding peak to peak value in the interval
    #  -minat - contains conditions for finding time of minimum value in the interval
    #  -maxat - contains conditions for finding time of maximum value in the interval
    #  -between - contains conditions for fetching data in the interval
    # This procedure imitates the .meas command from SPICE3 and Ngspice in particular. It has mutiple modes, and each mod
    #  could have different forms:
    #  ###### **Trigger-Target**
    #  In this mode it measures the difference in x list between two points selected from one or two input lists 
    #  (vectors). First it searches for the trigger point along x list (vector) with certain value of input list,
    #  or certain x axis value, second it searches for the target point with with certain value of input list, or 
    #  certain x axis value, and finally calculates difference between trigger and target point along x axis.
    #  The conditions for trigger and target could be in two forms: hit of certain value by specified vector, or 
    #  certain exact point on x axis. These conditions are provided as a list of arguments to -trig and -targ switches:
    #   -vec - name of vector in data dictionary
    #   -val - value to match
    #   -td - x axis delay after which the search is start, default is 0.0.
    #   -cross - condition's count, cross conditions counts every time vector crosses value, and saves
    #     only n-th crossing the value. The possible values are positive integers, or `last` string.
    #   -rise - condition's count, rise conditions counts every time vector crosses value from lower to higher
    #     (rising slope), and saves only n-th crossing the value. The possible values are positive integers, or `last` 
    #     string.
    #   -fall - condition's count, fall conditions counts every time vector crosses value from higher to lower
    #     (falling slope), and saves only n-th crossing the value. The possible values are positive integers, or `last` 
    #     string.
    #
    #  or
    #
    #   -at - exact x value
    #
    # Examples of usages:
    # ```
    # ::measure::measure -xname x -data [dcreate x $x y1 $y1 y2 $y2] -trig {-vec y1 -val 0.1 -rise 3} -targ {-vec y2 -val 0.5 -fall 5}
    # ```
    # Here we use x key value as x axis, trigger vector point is when y1 crosses value 0.1, third rise, and target 
    # vector point is when y2 crosses value 0.5, fifth fall.
    # ```
    # ::measure::measure -xname x -data [dcreate x $x y1 $y1 y2 $y2] -trig {-vec y1 -val 0.7 -rise 2} -targ {-at 20.0}
    # ```
    # Here we use x key value as x axis, trigger vector point is when y1 crosses value 0.7, second rise, and target point 
    # is value 20.0 at x axis.
    # 
    # In this mode procedure returns dictionary with keys `xtrig`, `xtarg`, `xdelta` and corresponding values.
    # Synopsis: -xname value -data value -trig \{-vec value -val value ?-td value? -cross|rise|fall value\}
    #   -targ \{-vec value -val value ?-td value? -cross|rise|fall value\}
    # Synopsis: -xname value -data value -trig \{-at value\} -targ \{-vec value -val value ?-td value? -cross|rise|fall value\}
    # Synopsis: -xname value -data value -trig \{-vec value -val value ?-td value? -cross|rise|fall value\} -targ \{-at value\}
    #
    #  ###### **Find-When** or **Deriv-When**
    #  In this mode it measures any vector (or its derivative), when two signals cross each other or a signal crosses a given value. 
    #  Measurements start after a delay `-td` and may be restricted to a range between `-from` and `-to`. Possible
    #  combinations of switches are `-when {...}` or `-find {...} -when {...}`. For `-when` the possible switches are:
    #   -vec - name of vector in data dictionary
    #   -val - value to match
    #   -td - x axis delay after which the search is start, default is 0.0.
    #   -from - start of the range in which search happens, default is minimum value of x.
    #   -to - end of the range in which search happens, default is maximum value of x.
    #   -cross - condition's count, cross conditions counts every time vector crosses value, and saves
    #     only n-th crossing the value. The possible values are positive integers, `all` or `last` string.
    #   -rise - condition's count, rise conditions counts every time vector crosses value from lower to higher
    #     (rising slope), and saves only n-th crossing the value. The possible values are positive integers, `all` or
    #     `last` string.
    #   -fall - condition's count, fall conditions counts every time vector crosses value from higher to lower
    #     (falling slope), and saves only n-th crossing the value. The possible values are positive integers, `all` or 
    #     `last` string.
    #
    #  or
    #
    #   -vec1 - name of first vector in data dictionary
    #   -vec2 - name of second vector in data dictionary
    #   -td - x axis delay after which the search is start, default is 0.0.
    #   -from - start of the range in which search happens, default is minimum value of x.
    #   -to - end of the range in which search happens, default is maximum value of x.
    #   -cross - condition's count, cross conditions counts every time `-vec1` vector crosses value, and saves
    #     only n-th crossing the value. The possible values are positive integers, or `last` string.
    #   -rise - condition's count, rise conditions counts every time `-vec1` vector crosses value from lower to higher
    #     (rising slope), and saves only n-th crossing the value. The possible values are positive integers, or `last` 
    #     string.
    #   -fall - condition's count, fall conditions counts every time `-vec1` vector crosses value from higher to lower
    #     (falling slope), and saves only n-th crossing the value. The possible values are positive integers, or `last` 
    #     string.
    #
    #  For `-find` and `-deriv` we specify the vector name in data dictionary for which values (or derivative) should be
    #  found at `when` point.
    # Examples of usages:
    # ```
    # ::measure::measure -xname x -data [dcreate x $x y1 $y1 y2 $y2] -find y1 -when {-vec y2 -val 0.5 -fall 5}
    # ```
    # Here we use x key value as x axis, find vector is y1, and point is when vector y2 crosses value 0.5, fifth fall.
    # ```
    # ::measure::measure -xname x -data [dcreate x $x y1 $y1 y2 $y2] -find y1 -when {-vec1 y1 -vec2 y2 -fall 5}
    # ```
    # Here we use x key value as x axis, find vector is y1, and point is when vector y1 crosses y2, fifth fall of y1 
    # vector.
    # ```
    # ::measure::measure -xname x -data [dcreate x $x y1 $y1 y2 $y2] -when {-vec1 y1 -vec2 y2 -fall last -from 1 -to 30}
    # ```
    # Here we use x key value as x axis, point is when vector y1 crosses y2, last fall of y1 vector, searching range is 
    # [1,30].
    # In this mode procedure returns dictionary with keys `xwhen`, and `yfind` if `-find` switch is specified, and 
    # corresponding values.
    # Synopsis: -xname value -data value ?-find|deriv value? -when \{-vec value -val value ?-td value? ?-from value? 
    #   ?-to value? -cross|rise|fall value\}
    # Synopsis: -xname value -data value ?-find|deriv value? -when \{-vec1 value -vec2 value ?-td value? ?-from value? 
    #   ?-to value? -cross|rise|fall value\}
    #  ###### **Find-At**
    #  In this mode it finds value of the vector at specified time.
    # Examples of usages:
    # ```
    # ::measure::measure -xname x -data [dcreate x $x y1 $y1 y2 $y2] -find y1 -at 5
    # ```
    # Synopsis: -xname value -data value -find value -at value
    #  ###### **Deriv-At**
    #  In this mode it finds value of the vector's derivative at specified time.
    # Examples of usages:
    # ```
    # ::measure::measure -xname x -data [dcreate x $x y1 $y1 y2 $y2] -deriv y1 -at 5
    # ```
    # Synopsis: -xname value -data value -deriv value -at value
    #  ###### **Avg|Rms|Min|Max|PP|MinAt|MaxAt|Between**
    #  This mode is combination of many modes with the same interface.
    #   -vec - name of vector in data dictionary
    #   -from - start of the range in which search happens, default is minimum value of x.
    #   -to - end of the range in which search happens, default is maximum value of x.
    # Examples of usages:
    # ```
    # ::measure::measure -xname x -data [dcreate x $x y1 $y1 y2 $y2] -avg {-vec y1 -from 1 -to 5}
    # ```
    # In **Between** mode, the x and y values are returned within specified interval.
    # Synopsis: -xname value -data value -avg|rms|pp|min|max|minat|maxat|between \{-vec value ?-td value? ?-from value?
    #   ?-to value?\}
    #  ###### **Integ**
    #  This mode is combination of many modes with the same interface.
    #   -vec - name of vector in data dictionary
    #   -from - start of the integration range, default is minimum value of x.
    #   -to - end of the integration range, default is maximum value of x.
    #   -cum - optional flag to return cumulative integration result list instead of thhe final value
    # Examples of usages:
    # ```
    # ::measure::measure -xname x -data [dcreate x $x y1 $y1 y2 $y2] -avg {-vec y1 -from 1 -to 5}
    # ```
    # Synopsis: -xname value -data value -integ \{-vec value ?-td value? ?-from value? ?-to value? ?-cum?\}
    set keysList {trig targ find when at integ deriv avg min max pp rms minat maxat between}
    argparse -help {Does different measurements of input data lists. This procedure imitates the .meas command from\
                            SPICE3 and Ngspice in particular. It has mutiple modes, and each mod could have different\
                            forms: Trigger-Target, Find-When, Deriv-When, Find-At, Deriv-At,\
                            Avg|Rms|Min|Max|PP|MinAt|MaxAt|Between and Integ. See documentation for further details} {
        {-xname= -required -help {Name of x list in data dictionary. This list must be strictly increaing without\
                                          duplicate elements}}
        {-data= -required -help {Dictionary that contains lists with names as the keys and lists as the values}}
        {-trig= -require targ -allow {data xname targ} -help {Conditions for trigger, selects Trigger-Target\
                                                                      measurement}}
        {-targ= -require trig -allow {data xname trig}  -help {Conditions for target}}
        {-find= -allow {data xname when at} -help {Conditions for Find-When or Find-At mode}}
        {-when= -allow {data xname find deriv} -help {Conditions for Find-When or Deriv-When modes}}
        {-at= -type double -allow {data xname find deriv} -help {Time for Find-At or Deriv-At modes}}
        {-integ= -allow {data xname} -help {Conditions for Integ mode}}
        {-deriv= -allow {data xname deriv when at} -help {Conditions for Deriv-At mode}}
        {-avg= -allow {data xname} -help {Conditions for finding average value across the interval}}
        {-min= -allow {data xname} -help {Conditions for finding minimum value in the interval}}
        {-max= -allow {data xname} -help {Conditions for finding maximum value in the interval}}
        {-pp= -allow {data xname} -help {Conditions for finding peak to peak value in the interval}}
        {-rms= -allow {data xname} -help {Conditions for finding root meas square value across the interval}}
        {-minat= -allow {data xname} -help {Conditions for finding time of minimum value in the interval}}
        {-maxat= -allow {data xname} -help {Conditions for finding time of maximum value in the interval}}
        {-between= -allow {data xname} -help {Conditions for fetching data in the interval}}
    }
    if {[info exists at]} {
        if {![info exists find] && ![info exists deriv]} {
            return -code error "When -at switch is presented, -find switch or -deriv switch is required"
        }
    }
    if {[info exists find]} {
        if {![info exists when] && ![info exists at]} {
            return -code error "When -find switch is presented, -when switch or -at switch is required"
        }
    }
    if {[info exists trig]} {
        set definition {
            {-at= -forbid {vec val delay cross rise fall} -type double}
            {-vec= -forbid at -require val}
            {-val= -forbid at -type double}
            {-td|delay= -default 0.0 -forbid at -require {vec val} -type double}
            {-cross= -forbid {rise fall} -forbid at -require {vec val}}
            {-rise= -forbid {cross fall} -forbid at -require {vec val}}
            {-fall= -forbid {cross rise} -forbid at -require {vec val}}
        }
        set trigArgs [argparse -inline $definition $trig]
        set targArgs [argparse -inline $definition $targ]
        AliasesKeysCheck $trigArgs {at vec}
        AliasesKeysCheck $targArgs {at vec}
        if {![dexist $trigArgs at]} {
            set trigVecCond [AliasesKeysCheck $trigArgs {cross rise fall}]
            set trigVecCondCount [dget $trigArgs $trigVecCond]
            if {[string is integer $trigVecCondCount]} {
                if {$trigVecCondCount<=0} {
                    return -code error "Trig count '$trigVecCondCount' must be more than 0"
                }
            } elseif {$trigVecCondCount ne {last}} {
                return -code error "Trig count '$trigVecCondCount' must be an integer or 'last' string"
            }
            set trigData [dget $data [dget $trigArgs vec]]
            set trigVal [dget $trigArgs val]
        } else {
            set trigVecCond rise
            set trigVecCondCount 1
            set trigData [dget $data $xname]
            set trigVal [dget $trigArgs at]
        }
        if {![dexist $targArgs at]} {
            set targVecCond [AliasesKeysCheck $targArgs {cross rise fall}]
            set targVecCondCount [dget $targArgs $targVecCond]
            if {[string is integer $targVecCondCount]} {
                if {$targVecCondCount<=0} {
                    return -code error "Targ count '$targVecCondCount' must be more than 0"
                }
            } elseif {$targVecCondCount ne {last}} {
                return -code error "Targ count '$targVecCondCount' must be an integer or 'last' string"
            }
            set targData [dget $data [dget $targArgs vec]]
            set targVal [dget $targArgs val]
        } else {
            set targVecCond rise
            set targVecCondCount 1
            set targData [dget $data $xname]
            set targVal [dget $targArgs at]
        }
        return [::tclmeasure::TrigTarg [dget $data $xname] $trigData $trigVal $targData $targVal $trigVecCond $trigVecCondCount\
                        $targVecCond $targVecCondCount [dget $trigArgs delay] [dget $targArgs delay]]
    } elseif {[info exists find] && [info exists when]} {
        set whenArgs [argparse -inline {
            {-vec= -require val -forbid {vec1 vec2}}
            {-val= -require vec -forbid {vec1 vec2}}
            {-vec1= -require vec2 -forbid {vec val}}
            {-vec2= -require vec1 -forbid {vec val}}
            {-td|delay= -default 0.0 -type double}
            {-from= -type double}
            {-to= -type double}
            {-cross= -forbid {rise fall}}
            {-rise= -forbid {cross fall}}
            {-fall= -forbid {cross rise}}
        } $when]
        AliasesKeysCheck $whenArgs {vec vec1}
        set whenVecCond [AliasesKeysCheck $whenArgs {cross rise fall}]
        if {[string is integer [dget $whenArgs $whenVecCond]]} {
            if {[dget $whenArgs $whenVecCond]<=0} {
                return -code error "Trig count '[dget $whenArgs $whenVecCond]' must be more than 0"
            }
        } elseif {[dget $whenArgs $whenVecCond] ni {last all}} {
            return -code error "Trig count '[dget $whenArgs $whenVecCond]' must be an integer, 'last' or 'all' string"
        }
        FromTo $whenArgs $data $xname
        if {[dexist $whenArgs vec1]} {
            if {[dget $whenArgs vec1] eq [dget $whenArgs vec2]} {
                return -code error "vec1 must be different to vec2"
            }
            return [::tclmeasure::FindDerivWhen [dget $data $xname] findwheneq [dget $data $find] [dget $data [dget $whenArgs vec1]]\
                            {} [dget $data [dget $whenArgs vec2]] $whenVecCond [dget $whenArgs $whenVecCond]\
                            [dget $whenArgs delay] $from $to]
        } else {
            return [::tclmeasure::FindDerivWhen [dget $data $xname] findwhen [dget $data $find] [dget $data [dget $whenArgs vec]]\
                            [dget $whenArgs val] {} $whenVecCond [dget $whenArgs $whenVecCond] [dget $whenArgs delay]\
                            $from $to]
        }
    } elseif {[info exists deriv] && [info exists when]} {
        set whenArgs [argparse -inline {
            {-vec= -require val -forbid {vec1 vec2}}
            {-val= -require vec -forbid {vec1 vec2}}
            {-vec1= -require vec2 -forbid {vec val}}
            {-vec2= -require vec1 -forbid {vec val}}
            {-td|delay= -default 0.0 -type double}
            {-from= -type double}
            {-to= -type double}
            {-cross= -forbid {rise fall}}
            {-rise= -forbid {cross fall}}
            {-fall= -forbid {cross rise}}
        } $when]
        AliasesKeysCheck $whenArgs {vec vec1}
        set whenVecCond [AliasesKeysCheck $whenArgs {cross rise fall}]
        if {[string is integer [dget $whenArgs $whenVecCond]]} {
            if {[dget $whenArgs $whenVecCond]<=0} {
                return -code error "Trig count '[dget $whenArgs $whenVecCond]' must be more than 0"
            }
        } elseif {[dget $whenArgs $whenVecCond] ni {last all}} {
            return -code error "Trig count '[dget $whenArgs $whenVecCond]' must be an integer, 'last' or 'all' string"
        }
        FromTo $whenArgs $data $xname
        if {[dexist $whenArgs vec1]} {
            if {[dget $whenArgs vec1] eq [dget $whenArgs vec2]} {
                return -code error "vec1 must be different to vec2"
            }
            return [::tclmeasure::FindDerivWhen [dget $data $xname] derivwheneq [dget $data $deriv] [dget $data [dget $whenArgs vec1]]\
                            {} [dget $data [dget $whenArgs vec2]] $whenVecCond [dget $whenArgs $whenVecCond]\
                            [dget $whenArgs delay] $from $to]
        } else {
            return [::tclmeasure::FindDerivWhen [dget $data $xname] derivwhen [dget $data $deriv] [dget $data [dget $whenArgs vec]]\
                            [dget $whenArgs val] {} $whenVecCond [dget $whenArgs $whenVecCond] [dget $whenArgs delay]\
                            $from $to]
        }
    } elseif {[info exists when]} {
        set whenArgs [argparse -inline {
            {-vec= -require val -forbid {vec1 vec2}}
            {-val= -require vec -forbid {vec1 vec2}}
            {-vec1= -require vec2 -forbid {vec val}}
            {-vec2= -require vec1 -forbid {vec val}}
            {-td|delay= -default 0.0 -type double}
            {-from= -type double}
            {-to= -type double}
            {-cross= -forbid {rise fall}}
            {-rise= -forbid {cross fall}}
            {-fall= -forbid {cross rise}}
        } $when]
        AliasesKeysCheck $whenArgs {vec vec1}
        set whenVecCond [AliasesKeysCheck $whenArgs {cross rise fall}]
        if {[string is integer [dget $whenArgs $whenVecCond]]} {
            if {[dget $whenArgs $whenVecCond]<=0} {
                return -code error "Trig count '[dget $whenArgs $whenVecCond]' must be more than 0"
            }
        } elseif {[dget $whenArgs $whenVecCond] ni {last all}} {
            return -code error "Trig count '[dget $whenArgs $whenVecCond]' must be an integer, 'last' or 'all' string"
        }
        FromTo $whenArgs $data $xname
        if {[dexist $whenArgs vec1]} {
            return [::tclmeasure::FindDerivWhen [dget $data $xname] wheneq {} [dget $data [dget $whenArgs vec1]] {}\
                            [dget $data [dget $whenArgs vec2]] $whenVecCond [dget $whenArgs $whenVecCond]\
                            [dget $whenArgs delay] $from $to]
        } else {
            return [::tclmeasure::FindDerivWhen [dget $data $xname] when {} [dget $data [dget $whenArgs vec]] [dget $whenArgs val] {}\
                            $whenVecCond [dget $whenArgs $whenVecCond] [dget $whenArgs delay] $from $to]
        }
    } elseif {[info exists find] && [info exists at]} {
        return [::tclmeasure::FindAt [dget $data $xname] $at [dget $data $find]]
    } elseif {[info exists deriv] && [info exists at]} {
        return [::tclmeasure::DerivAt [dget $data $xname] $at [dget $data $deriv]]
    } elseif {[info exists integ]} {
        set integArgs [argparse -inline {
            {-vec= -required}
            {-from= -type double}
            {-to= -type double}
            {-cum -boolean}
        } $integ]
        FromTo $integArgs $data $xname
        return [::tclmeasure::Integ [dget $data $xname] [dget $data [dget $integArgs vec]] $from $to [dget $integArgs cum]]
    } elseif {[info exists avg]} {
        set avgArgs [argparse -inline {
            {-vec= -required}
            {-from= -type double}
            {-to= -type double}
        } $avg]
        FromTo $avgArgs $data $xname
        return [::tclmeasure::Avg [dget $data $xname] [dget $data [dget $avgArgs vec]] $from $to]
    } elseif {[info exists rms]} {
        set rmsArgs [argparse -inline {
            {-vec= -required}
            {-from= -type double}
            {-to= -type double}
        } $rms]
        FromTo $rmsArgs $data $xname
        return [::tclmeasure::Rms [dget $data $xname] [dget $data [dget $rmsArgs vec]] $from $to]
    } elseif {[info exists min] || [info exists max] || [info exists pp] || [info exists minat] || [info exists maxat]\
                      || [info exists between]} {
        if {[info exists min]} {
            set type min
            set argsDict $min
        } elseif {[info exists max]} {
            set type max
            set argsDict $max
        } elseif {[info exists pp]} {
            set type pp
            set argsDict $pp
        } elseif {[info exists minat]} {
            set type minat
            set argsDict $minat
        } elseif {[info exists maxat]} {
            set type maxat
            set argsDict $maxat
        } elseif {[info exists between]} {
            set type between
            set argsDict $between
        }
        set resDict [argparse -inline {
            {-vec= -required}
            {-from= -validate {[string is double $arg]}}
            {-to= -validate {[string is double $arg]}}
        } $argsDict]
        FromTo $resDict $data $xname
        return [::tclmeasure::MinMaxPPMinAtMaxAt [dget $data $xname] [dget $data [dget $resDict vec]] $from $to $type]
    }
}
