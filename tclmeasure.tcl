package require argparse 0.65
package provide tclmeasure 0.9

namespace eval ::tclmeasure {
    namespace import ::tcl::mathop::*
    namespace export measure
}

# Defaults are local to this interpreter; configure is deliberately not exported.
namespace eval ::tclmeasure {
    variable Defaults [dict create -output list -ifexists error]
}

proc ::tclmeasure::configure {args} {
    # Queries or changes package-wide output defaults in this Tcl interpreter.
    # -output mode - Default representation for -between and cumulative integration: list or vector; initially list.
    # -ifexists policy - Default vector collision policy: error or replace; initially error.
    #
    # With no arguments, returns the complete option/value dictionary. With one option, returns its value.
    # Updates validate every option before changing any defaults. Selecting vector output requires an RBC-enabled
    # build and a loadable RBC vector package. Per-measurement options override these defaults. Other measurement
    # results are unchanged. Input selection remains automatic: -data selects lists; omitting it selects vectors.
    # This command is not namespace-exported; call it as ::tclmeasure::configure.
    #
    # Synopsis: ?option?
    # Synopsis: ?-output list|vector? ?-ifexists error|replace?
    # Returns: The queried value, or the complete configuration dictionary.
    variable Defaults
    if {([llength $args] == 1) && ([lindex $args 0] ne {-help})} {
        set arguments [argparse -inline -help {Query package defaults.} {
            {-output -key query -value -output -help {Query the default output representation}}
            {-ifexists -key query -value -ifexists -help {Query the default collision policy}}
        }]
        return [dict get $Defaults [dict get $arguments query]]
    }
    set arguments [argparse -inline -help {Query or change package defaults atomically.} {
        {-output= -enum {list vector} -help {Default representation for between and cumulative output}}
        {-ifexists= -enum {error replace} -help {Default vector collision policy}}
    }]
    set next $Defaults
    dict for {key value} $arguments {dict set next -$key $value}
    if {[dict exists $arguments output] && ([dict get $arguments output] eq {vector})} {
        VectorSupport
    }
    set Defaults $next
    return $Defaults
}

proc ::tclmeasure::AliasesKeysCheck {arguments keys} {
    foreach key $keys {
        if {[dict exists $arguments $key]} {
            return $key
        }
    }
    set formKeys [lmap key $keys {subst -$key}]
    return -code error "[join [lrange $formKeys 0 end-1] ", "] or [lindex $formKeys end] must be presented"
}

proc ::tclmeasure::InputData {data name vectors} {
    # Lists come from the data dictionary. Vector names are resolved in measure's caller, without reading samples.
    if {!$vectors} {
        return [dict get $data $name]
    }
    set command [uplevel 2 [list namespace which -command $name]]
    if {$command eq {}} {
        return -code error "vector command \"$name\" does not exist"
    }
    return $command
}

proc ::tclmeasure::FromTo {argsDict xData vectors} {
    # Only the endpoints are read for defaults; full vector storage is consumed directly by the C command.
    if {![dict exists $argsDict from]} {
        if {$vectors} {
            set from [$xData index [$xData offset]]
        } else {
            set from [lindex $xData 0]
        }
    } else {
        set from [dict get $argsDict from]
    }
    if {![dict exists $argsDict to]} {
        if {$vectors} {
            set to [$xData index end]
        } else {
            set to [lindex $xData end]
        }
    } else {
        set to [dict get $argsDict to]
    }
    uplevel 1 [list set from $from]
    uplevel 1 [list set to $to]
}

# Private argument definitions shared by measurement execution and interactive help.
proc ::tclmeasure::MeasureDefinition {} {
    return {
        {-output= -enum {list vector} -help {Output representation for -between and -integ with -cum}}
        {-name= -help {Destination for the y output, or #auto; vector output only}}
        {-names= -help {Dictionary mapping x and/or y to destination names or #auto; vector output only}}
        {-namespace= -help {Output namespace relative to the caller; created if missing}}
        {-ifexists= -enum {error replace} -help {Collision policy for output vectors}}
        {-xname= -required -help {Name of x list in data dictionary. This list must be strictly increaing without\
                                          duplicate elements}}
        {-data= -help {Dictionary of named lists; omit to use RBC vector command names}}
        {-trig= -require targ -allow {output name names namespace ifexists data xname targ}\
                 -help {Conditions for trigger, selects Trigger-Target measurement}}
        {-targ= -require trig -allow {output name names namespace ifexists data xname trig}\
                 -help {Conditions for target}}
        {-find= -allow {output name names namespace ifexists data xname when at} -help {Conditions for Find-When or\
                                                                                                Find-At mode}}
        {-when= -allow {output name names namespace ifexists data xname find deriv}\
                 -help {Conditions for Find-When or Deriv-When modes}}
        {-at= -type double -allow {output name names namespace ifexists data xname find deriv}\
                 -help {Time for Find-At or Deriv-At modes}}
        {-integ= -allow {output name names namespace ifexists data xname} -help {Conditions for Integ mode}}
        {-deriv= -allow {output name names namespace ifexists data xname deriv when at} -help {Conditions for\
                                                                                                       Deriv-At mode}}
        {-avg= -allow {output name names namespace ifexists data xname} -help {Conditions for finding average value\
                                                                                       across the interval}}
        {-min= -allow {output name names namespace ifexists data xname} -help {Conditions for finding minimum value in\
                                                                                       the interval}}
        {-max= -allow {output name names namespace ifexists data xname} -help {Conditions for finding maximum value in\
                                                                                       the interval}}
        {-pp= -allow {output name names namespace ifexists data xname} -help {Conditions for finding peak to peak\
                                                                                      value in the interval}}
        {-rms= -allow {output name names namespace ifexists data xname} -help {Conditions for finding root meas square\
                                                                                       value across the interval}}
        {-minat= -allow {output name names namespace ifexists data xname} -help {Conditions for finding time of minimum\
                                                                                         value in the interval}}
        {-maxat= -allow {output name names namespace ifexists data xname} -help {Conditions for finding time of maximum\
                                                                                         value in the interval}}
        {-between= -allow {output name names namespace ifexists data xname} -help {Conditions for fetching data in the\
                                                                                           interval}}
    }
}

proc ::tclmeasure::ConditionDefinition {kind} {
    switch -- $kind {
        trigger {
            return {
                {-at= -forbid {vec val delay cross rise fall} -type double\
                         -help {Fixed x-coordinate instead of a crossing condition}}
                {-vec= -forbid at -require val -help {Signal to measure or compare with -val}}
                {-val= -forbid at -type double -help {Comparison level for -vec}}
                {-td|delay= -default 0.0 -forbid at -require {vec val} -type double\
                         -help {Ignore crossings before this x-coordinate; default 0}}
                {-cross= -forbid {rise fall} -forbid at -require {vec val}\
                         -help {Either-direction crossing count: positive integer or last}}
                {-rise= -forbid {cross fall} -forbid at -require {vec val}\
                         -help {Rising crossing count: positive integer or last}}
                {-fall= -forbid {cross rise} -forbid at -require {vec val}\
                         -help {Falling crossing count: positive integer or last}}
            }
        }
        when {
            return {
                {-vec= -require val -forbid {vec1 vec2} -help {Signal to measure or compare with -val}}
                {-val= -require vec -forbid {vec1 vec2} -help {Comparison level for -vec}}
                {-vec1= -require vec2 -forbid {vec val} -help {First signal for an equality crossing}}
                {-vec2= -require vec1 -forbid {vec val} -help {Second signal for an equality crossing}}
                {-td|delay= -default 0.0 -type double -help {Ignore crossings before this x-coordinate; default 0}}
                {-from= -type double -help {Start of the measurement interval; defaults to the first x value}}
                {-to= -type double -help {End of the measurement interval; defaults to the last x value}}
                {-cross= -forbid {rise fall} -help {Either-direction crossing count: positive integer or last or all}}
                {-rise= -forbid {cross fall} -help {Rising crossing count: positive integer or last or all}}
                {-fall= -forbid {cross rise} -help {Falling crossing count: positive integer or last or all}}
            }
        }
        integ {
            return {
                {-vec= -required -help {Signal to measure}}
                {-from= -type double -help {Start of the measurement interval; defaults to the first x value}}
                {-to= -type double -help {End of the measurement interval; defaults to the last x value}}
                {-cum -boolean -help {Return cumulative integrals rather than a single integral}}
            }
        }
        interval {
            return {
                {-vec= -required -help {Signal to measure}}
                {-from= -type double -help {Start of the measurement interval; defaults to the first x value}}
                {-to= -type double -help {End of the measurement interval; defaults to the last x value}}
            }
        }
        extrema {
            return {
                {-vec= -required -help {Signal to measure}}
                {-from= -validate {[string is double $arg]}\
                         -help {Start of the measurement interval; defaults to the first x value}}
                {-to= -validate {[string is double $arg]}\
                         -help {End of the measurement interval; defaults to the last x value}}
            }
        }
        default {
            return -code error "unknown condition definition: $kind"
        }
    }
}

# Selector-only help is handled before required inputs or vector support are checked.
# Calls containing actual option values retain the ordinary top-level help behavior.
proc ::tclmeasure::MeasureHelp {arguments} {
    if {({-help} ni $arguments) && ({-h} ni $arguments)} {
        return 0
    }
    set choices {-trig -targ -find -when -at -deriv -integ -avg -rms -min -max -pp -minat -maxat -between}
    set selected {}
    foreach word $arguments {
        if {$word in {-help -h}} {
            continue
        }
        if {$word in $choices} {
            set option $word
        } else {
            # Match prefixes literally, without treating user text as a glob pattern.
            set matches {}
            foreach choice $choices {
                if {([string first $word $choice]==0) && ($word ne {}) && ($word ne {-})} {
                    lappend matches $choice
                }
            }
            if {[llength $matches]!=1} {
                return 0
            }
            set option [lindex $matches 0]
        }
        if {$option ni $selected} {lappend selected $option}
    }
    if {$selected eq {}} {
        return 0
    }
    set selected [lsort $selected]
    set common {measure -xname name ?-data dictionary?}
    switch -- $selected {
        {-targ -trig} - 
        -trig - 
        -targ {
            set kind trigger
            set synopsis "$common -trig {conditions} -targ {conditions}"
            set detail {Options below apply separately to both trigger and target conditions}
        }
        {-find -when} - 
        {-deriv -when} - 
        -when {
            set kind when
            set mode {}
            if {{-find} in $selected} {
                set mode {-find signal }
            }
            if {{-deriv} in $selected} {
                set mode {-deriv signal }
            }
            set synopsis "$common ${mode}-when {conditions}"
            set detail {Options below describe -when conditions. Select a level crossing or equality of two signals}
        }
        {-at -find} - 
        {-at -deriv} {
            set definition {}
            foreach item [MeasureDefinition] {
                set name [string trimright [lindex $item 0] =]
                if {$name in $selected} {
                    # The help subset omits other outer options; remove relationships to those options.
                    set display [list [lindex $item 0]]
                    foreach {key value} [lrange $item 1 end] {
                        if {$key ni {-allow -require -forbid}} {
                            lappend display $key $value
                        }
                    }
                    lappend definition $display
                }
            }
            set mode [expr {{-find} in $selected ? {-find} : {-deriv}}]
            set synopsis "$common $mode signal -at x"
            set detail {This mode uses scalar outer options, not a nested condition list}
        }
        -integ {
            set kind integ
        }
        -avg - 
        -rms {
            set kind interval
        }
        -min - 
        -max - 
        -pp - 
        -minat - 
        -maxat - 
        -between {
            set kind extrema
        }
        default {
            return -code error "invalid measurement help combination: [join $selected { }]"
        }
    }
    if {![info exists synopsis]} {
        set synopsis "$common [lindex $selected 0] {options}"
        set detail {Options below describe the nested measurement argument.}
    }
    if {[info exists kind]} {
        set definition [ConditionDefinition $kind]
    }
    if {{-between} in $selected || {-integ} in $selected} {
        append synopsis { ?-output list|vector? ?-names dictionary? ?-name name? ?-namespace name?\
                                  ?-ifexists error|replace?}
    }
    puts "Usage: $synopsis"
    argparse -inline -helplevel 1 -help $detail $definition {-help}
    return 1
}

proc ::tclmeasure::measure {args} {
    # Read-only RBC vectors are valid inputs. Storage is changed only when explicitly used as an output destination;
    # locked vectors cannot be replaced and are never unlocked.
    # Does different measurements of input data lists or real RBC vectors.
    # With -data, names select lists in the supplied dictionary. Without -data, names select RBC vector commands
    # in the caller's namespace; direct vector support must be enabled at build time with --with-rbc.
    # Vector names are passed directly to C without converting the samples to lists. All modes preserve their
    # existing scalar/list/dictionary results by default. Mapped array variables are optional.
    # Complex vectors are rejected. X must be strictly increasing and all inputs must have equal sample counts.
    # Vector index offsets do not change sample alignment. Derivatives need at least three samples; other modes
    # need at least two. See the installation page for optional RBC build and runtime dependencies.
    #  -xname - name of x list in data dictionary, or RBC vector command when -data is omitted. This list must be
    #   strictly increaing without duplicate elements.
    #  -data - optional dictionary of named lists; omitting it selects direct RBC vector input.
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
    #  -output - list or vector for -between and cumulative integration; defaults to the package configuration.
    #  -names - dictionary mapping x and/or y to output vector names; omitted fields use #auto.
    #  -name - shorthand for the y output name; cannot be combined with the y entry in -names.
    #  -namespace - vector destination namespace, relative to the caller; missing parents are created.
    #  -ifexists - error or replace for output vector collisions; defaults to the package configuration.
    #
    # ###### **Vector output**
    # -between and -integ with nested -cum return dictionaries with x and y fields. With -output vector, the values
    # are fully qualified real RBC vector command names instead of lists. All other modes keep their existing
    # results, even with a global vector default. Input selection and output selection are independent.
    #
    # Omitted names and #auto use RBC's own name allocator. auto without # is an ordinary literal name. Relative
    # names use -namespace, or the caller's namespace when omitted. Fully qualified names override that namespace;
    # their own parent namespace must already exist. Empty names/namespaces, unknown -names fields and duplicate
    # destinations are errors. Naming options require vector output from one of the two supported forms.
    #
    # Explicit names are literal, including parentheses, and no mapped array variable is created. The caller owns
    # returned vectors. With replace, existing writable real vectors are updated in place, retaining graph clients.
    # Other commands, aliases, complex vectors and read-only vectors cannot be replaced. All calculations finish
    # before publication, so destinations may also be inputs. On failure, newly created vectors are removed;
    # replacement of existing vectors is not transactional against application notification callbacks.
    #
    # Example:
    # ```tcl
    # set data [dict create time {0 1 2 3} current {0 2 4 6}]
    # set result [measure -data $data -xname time -between {-vec current -from 0.5 -to 2.5}\
    #                 -output vector -namespace ::results -names {x time y #auto}]
    # set samples [[dict get $result y] range 0 end]
    # rbc::vector destroy {*}[dict values $result]
    # ```
    #
    # Synopsis: -xname name ?-data dictionary? -between conditions ?-output list|vector? ?-name name?
    # ?-names dictionary? ?-namespace name? ?-ifexists error|replace?
    # Synopsis: -xname name ?-data dictionary? -integ \{-vec name -cum ?-from value? ?-to value?\}
    # ?-output list|vector? ?-name name? ?-names dictionary? ?-namespace name? ?-ifexists error|replace?
    #
    # This procedure imitates the .meas command from SPICE3 and Ngspice in particular. It has mutiple modes, and each
    #  mod could have different forms:
    #  ###### **Trigger-Target**
    #  In this mode it measures the difference in x list between two points selected from one or two input lists 
    #  (vectors). First it searches for the trigger point along x list (vector) with certain value of input list,
    #  or certain x axis value, second it searches for the target point with with certain value of input list, or 
    #  certain x axis value, and finally calculates difference between trigger and target point along x axis.
    #  The conditions for trigger and target could be in two forms: hit of certain value by specified vector, or 
    #  certain exact point on x axis. These conditions are provided as a list of arguments to -trig and -targ switches:
    #   -vec - key in data dictionary, or RBC vector command when -data is omitted
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
    # ```tcl
    # measure -xname x -data [dict create x $x y1 $y1 y2 $y2] -trig {-vec y1 -val 0.1 -rise 3}\
    #                                -targ {-vec y2 -val 0.5 -fall 5}
    # ```
    # Here we use x key value as x axis, trigger vector point is when y1 crosses value 0.1, third rise, and target 
    # vector point is when y2 crosses value 0.5, fifth fall.
    #
    # ```tcl
    # measure -xname x -data [dict create x $x y1 $y1 y2 $y2] -trig {-vec y1 -val 0.7 -rise 2} -targ {-at 20.0}
    # ```
    # Here we use x key value as x axis, trigger vector point is when y1 crosses value 0.7, second rise, and target
    # point is value 20.0 at x axis.
    # 
    # In this mode procedure returns dictionary with keys `xtrig`, `xtarg`, `xdelta` and corresponding values.
    #
    # Synopsis: -xname value -data value -trig \{-vec value -val value ?-td value? -cross|rise|fall value\} -targ \{-vec
    # value -val value ?-td value? -cross|rise|fall value\}
    # Synopsis: -xname value -data value -trig \{-at value\} -targ \{-vec value -val value ?-td value? -cross|rise|fall
    # value\}
    # Synopsis: -xname value -data value -trig \{-vec value -val value ?-td value? -cross|rise|fall value\} -targ \{-at
    # value\}
    #
    # ###### **Find-When** or **Deriv-When**
    # In this mode it measures any vector (or its derivative), when two signals cross each other or a signal crosses 
    # a given value. Measurements start after a delay `-td` and may be restricted to a range between `-from` and `-to`.
    # Possible combinations of switches are `-when {...}` or `-find {...} -when {...}`. For `-when` the possible 
    # switches are:
    #  -vec - key in data dictionary, or RBC vector command when -data is omitted
    #  -val - value to match
    #  -td - x axis delay after which the search is start, default is 0.0.
    #  -from - start of the range in which search happens, default is minimum value of x.
    #  -to - end of the range in which search happens, default is maximum value of x.
    #  -cross - condition's count, cross conditions counts every time vector crosses value, and saves
    #    only n-th crossing the value. The possible values are positive integers, `all` or `last` string.
    #  -rise - condition's count, rise conditions counts every time vector crosses value from lower to higher
    #    (rising slope), and saves only n-th crossing the value. The possible values are positive integers, `all` or
    #    `last` string.
    #  -fall - condition's count, fall conditions counts every time vector crosses value from higher to lower
    #    (falling slope), and saves only n-th crossing the value. The possible values are positive integers, `all` or 
    #    `last` string.
    #
    # or
    #
    #  -vec1 - first key in data dictionary, or RBC vector command when -data is omitted
    #  -vec2 - second key in data dictionary, or RBC vector command when -data is omitted
    #  -td - x axis delay after which the search is start, default is 0.0.
    #  -from - start of the range in which search happens, default is minimum value of x.
    #  -to - end of the range in which search happens, default is maximum value of x.
    #  -cross - condition's count, cross conditions counts every time `-vec1` vector crosses value, and saves
    #    only n-th crossing the value. The possible values are positive integers, or `last` string.
    #  -rise - condition's count, rise conditions counts every time `-vec1` vector crosses value from lower to higher
    #    (rising slope), and saves only n-th crossing the value. The possible values are positive integers, or `last` 
    #     string.
    #  -fall - condition's count, fall conditions counts every time `-vec1` vector crosses value from higher to lower
    #    (falling slope), and saves only n-th crossing the value. The possible values are positive integers, or `last` 
    #    string.
    #
    # For `-find` and `-deriv` we specify the vector name in data dictionary for which values (or derivative) should be
    # found at `when` point.
    #
    # Examples of usages:
    # ```tcl
    # measure -xname x -data [dict create x $x y1 $y1 y2 $y2] -find y1 -when {-vec y2 -val 0.5 -fall 5}
    # ```
    # Here we use x key value as x axis, find vector is y1, and point is when vector y2 crosses value 0.5, fifth fall.
    #
    # ```tcl
    # measure -xname x -data [dict create x $x y1 $y1 y2 $y2] -find y1 -when {-vec1 y1 -vec2 y2 -fall 5}
    # ```
    # Here we use x key value as x axis, find vector is y1, and point is when vector y1 crosses y2, fifth fall of y1 
    # vector.
    #
    # ```tcl
    # measure -xname x -data [dict create x $x y1 $y1 y2 $y2] -when {-vec1 y1 -vec2 y2 -fall last -from 1 -to 30}
    # ```
    # Here we use x key value as x axis, point is when vector y1 crosses y2, last fall of y1 vector, searching range is
    # [1,30].  In this mode procedure returns dictionary with keys `xwhen`, and `yfind` if `-find` switch is specified,
    # and corresponding values.
    #
    # Synopsis: -xname value -data value ?-find|deriv value? -when \{-vec value -val value ?-td value? ?-from value? 
    #   ?-to value? -cross|rise|fall value\}
    # Synopsis: -xname value -data value ?-find|deriv value? -when \{-vec1 value -vec2 value ?-td value? ?-from value? 
    #   ?-to value? -cross|rise|fall value\}
    #
    # ###### **Find-At**
    # In this mode it finds value of the vector at specified time.
    #
    # Examples of usages:
    # ```tcl
    # measure -xname x -data [dict create x $x y1 $y1 y2 $y2] -find y1 -at 5
    # ```
    #
    # Synopsis: -xname value -data value -find value -at value
    #
    # ###### **Deriv-At**
    # In this mode it finds value of the vector's derivative at specified time.
    # Examples of usages:
    # ```tcl
    # measure -xname x -data [dict create x $x y1 $y1 y2 $y2] -deriv y1 -at 5
    # ```
    #
    # Synopsis: -xname value -data value -deriv value -at value
    #
    # ###### **Avg|Rms|Min|Max|PP|MinAt|MaxAt|Between**
    # This mode is combination of many modes with the same interface.
    #  -vec - key in data dictionary, or RBC vector command when -data is omitted
    #  -from - start of the range in which search happens, default is minimum value of x.
    #  -to - end of the range in which search happens, default is maximum value of x.
    # Examples of usages:
    # ```tcl
    # measure -xname x -data [dict create x $x y1 $y1 y2 $y2] -avg {-vec y1 -from 1 -to 5}
    # ```
    # In **Between** mode, the x and y values are returned within specified interval.
    # Synopsis: -xname value -data value -avg|rms|pp|min|max|minat|maxat|between \{-vec value ?-td value? ?-from value?
    #   ?-to value?\}
    #
    # ###### **Integ**
    # This mode is combination of many modes with the same interface.
    #  -vec - key in data dictionary, or RBC vector command when -data is omitted
    #  -from - start of the integration range, default is minimum value of x.
    #  -to - end of the integration range, default is maximum value of x.
    #  -cum - returns the cumulative x/y dictionary instead of the final scalar; values follow -output.
    # Examples of usages:
    # ```tcl
    # measure -xname x -data [dict create x $x y1 $y1 y2 $y2] -avg {-vec y1 -from 1 -to 5}
    # ```
    #
    # Interactive help:
    # Use `measure -help` for general options, or select a mode before -help:
    # `measure -avg -help`, `measure -trig -targ -help`, `measure -find -when -help`,
    # `measure -deriv -at -help`, or `measure -when -help`.
    # Mode help accepts unambiguous selector prefixes and needs no input data or RBC support.
    # Bare selectors without -help remain measurement calls and require their normal arguments.
    #
    # Synopsis: -xname value -data value -integ \{-vec value ?-td value? ?-from value? ?-to value? ?-cum?\}
    set keysList {trig targ find when at integ deriv avg min max pp rms minat maxat between}
    if {[MeasureHelp $args]} {
        return
    }
    argparse -help {Does different measurements of input data lists. This procedure imitates the .meas command from\
                            SPICE3 and Ngspice in particular. It has mutiple modes, and each mod could have different\
                            forms: Trigger-Target, Find-When, Deriv-When, Find-At, Deriv-At,\
                            Avg|Rms|Min|Max|PP|MinAt|MaxAt|Between and Integ. See documentation for further details}\
                            [MeasureDefinition]
    variable Defaults
    if {![info exists output]} {set output [dict get $Defaults -output]}
    if {![info exists ifexists]} {set ifexists [dict get $Defaults -ifexists]}
    set sequence [info exists between]
    if {[info exists integ]} {
        set integArgs [argparse -inline [ConditionDefinition integ] $integ]
        set sequence [dict get $integArgs cum]
    }
    set outputOptions [dict create ifexists $ifexists]
    foreach key {name names namespace} {
        if {[info exists $key]} {
            if {!$sequence || $output ne {vector}} {
                return -code error "-$key requires vector output from -between or -integ with -cum"
            }
            dict set outputOptions $key [set $key]
        }
    }
    if {$sequence && $output eq {vector}} {VectorSupport}
    set vectors [expr {![info exists data]}]
    set suffix {}
    if {$vectors} {
        # Fail clearly in a list-only build before attempting vector command calls.
        VectorSupport
        set data {}
        set suffix Vectors
    }
    set xData [InputData $data $xname $vectors]
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
        set definition [ConditionDefinition trigger]
        set trigArgs [argparse -inline $definition $trig]
        set targArgs [argparse -inline $definition $targ]
        AliasesKeysCheck $trigArgs {at vec}
        AliasesKeysCheck $targArgs {at vec}
        if {![dict exists $trigArgs at]} {
            set trigVecCond [AliasesKeysCheck $trigArgs {cross rise fall}]
            set trigVecCondCount [dict get $trigArgs $trigVecCond]
            if {[string is integer -strict $trigVecCondCount]} {
                if {$trigVecCondCount<=0} {
                    return -code error "Trig count '$trigVecCondCount' must be more than 0"
                }
            } elseif {$trigVecCondCount ne {last}} {
                return -code error "Trig count '$trigVecCondCount' must be an integer or 'last' string"
            }
            set trigData [InputData $data [dict get $trigArgs vec] $vectors]
            set trigVal [dict get $trigArgs val]
        } else {
            set trigVecCond rise
            set trigVecCondCount 1
            set trigData $xData
            set trigVal [dict get $trigArgs at]
        }
        if {![dict exists $targArgs at]} {
            set targVecCond [AliasesKeysCheck $targArgs {cross rise fall}]
            set targVecCondCount [dict get $targArgs $targVecCond]
            if {[string is integer -strict $targVecCondCount]} {
                if {$targVecCondCount<=0} {
                    return -code error "Targ count '$targVecCondCount' must be more than 0"
                }
            } elseif {$targVecCondCount ne {last}} {
                return -code error "Targ count '$targVecCondCount' must be an integer or 'last' string"
            }
            set targData [InputData $data [dict get $targArgs vec] $vectors]
            set targVal [dict get $targArgs val]
        } else {
            set targVecCond rise
            set targVecCondCount 1
            set targData $xData
            set targVal [dict get $targArgs at]
        }
        return [::tclmeasure::TrigTarg${suffix} $xData $trigData $trigVal $targData $targVal $trigVecCond\
                        $trigVecCondCount $targVecCond $targVecCondCount [dict get $trigArgs delay]\
                        [dict get $targArgs delay]]
    } elseif {[info exists find] && [info exists when]} {
        set whenArgs [argparse -inline [ConditionDefinition when] $when]
        AliasesKeysCheck $whenArgs {vec vec1}
        set whenVecCond [AliasesKeysCheck $whenArgs {cross rise fall}]
        if {[string is integer -strict [dict get $whenArgs $whenVecCond]]} {
            if {[dict get $whenArgs $whenVecCond]<=0} {
                return -code error "Trig count '[dict get $whenArgs $whenVecCond]' must be more than 0"
            }
        } elseif {[dict get $whenArgs $whenVecCond] ni {last all}} {
            return -code error "Trig count '[dict get $whenArgs $whenVecCond]' must be an integer, 'last' or 'all'\
                    string"
        }
        FromTo $whenArgs $xData $vectors
        if {[dict exists $whenArgs vec1]} {
            if {[dict get $whenArgs vec1] eq [dict get $whenArgs vec2]} {
                return -code error "vec1 must be different to vec2"
            }
            return [::tclmeasure::FindDerivWhen${suffix} $xData findwheneq [InputData $data $find $vectors]\
                            [InputData $data [dict get $whenArgs vec1] $vectors] {}\
                            [InputData $data [dict get $whenArgs vec2] $vectors]\
                            $whenVecCond [dict get $whenArgs $whenVecCond] [dict get $whenArgs delay] $from $to]
        } else {
            return [::tclmeasure::FindDerivWhen${suffix} $xData findwhen [InputData $data $find $vectors]\
                            [InputData $data [dict get $whenArgs vec] $vectors] [dict get $whenArgs val] {}\
                            $whenVecCond [dict get $whenArgs $whenVecCond] [dict get $whenArgs delay] $from $to]
        }
    } elseif {[info exists deriv] && [info exists when]} {
        set whenArgs [argparse -inline [ConditionDefinition when] $when]
        AliasesKeysCheck $whenArgs {vec vec1}
        set whenVecCond [AliasesKeysCheck $whenArgs {cross rise fall}]
        if {[string is integer -strict [dict get $whenArgs $whenVecCond]]} {
            if {[dict get $whenArgs $whenVecCond]<=0} {
                return -code error "Trig count '[dict get $whenArgs $whenVecCond]' must be more than 0"
            }
        } elseif {[dict get $whenArgs $whenVecCond] ni {last all}} {
            return -code error "Trig count '[dict get $whenArgs $whenVecCond]' must be an integer, 'last' or 'all'\
                    string"
        }
        FromTo $whenArgs $xData $vectors
        if {[dict exists $whenArgs vec1]} {
            if {[dict get $whenArgs vec1] eq [dict get $whenArgs vec2]} {
                return -code error "vec1 must be different to vec2"
            }
            return [::tclmeasure::FindDerivWhen${suffix} $xData derivwheneq [InputData $data $deriv $vectors]\
                            [InputData $data [dict get $whenArgs vec1] $vectors] {}\
                            [InputData $data [dict get $whenArgs vec2] $vectors]\
                            $whenVecCond [dict get $whenArgs $whenVecCond] [dict get $whenArgs delay] $from $to]
        } else {
            return [::tclmeasure::FindDerivWhen${suffix} $xData derivwhen [InputData $data $deriv $vectors]\
                            [InputData $data [dict get $whenArgs vec] $vectors]\
                            [dict get $whenArgs val] {} $whenVecCond\
                            [dict get $whenArgs $whenVecCond] [dict get $whenArgs delay] $from $to]
        }
    } elseif {[info exists when]} {
        set whenArgs [argparse -inline [ConditionDefinition when] $when]
        AliasesKeysCheck $whenArgs {vec vec1}
        set whenVecCond [AliasesKeysCheck $whenArgs {cross rise fall}]
        if {[string is integer -strict [dict get $whenArgs $whenVecCond]]} {
            if {[dict get $whenArgs $whenVecCond]<=0} {
                return -code error "Trig count '[dict get $whenArgs $whenVecCond]' must be more than 0"
            }
        } elseif {[dict get $whenArgs $whenVecCond] ni {last all}} {
            return -code error "Trig count '[dict get $whenArgs $whenVecCond]' must be an integer, 'last' or 'all'\
                    string"
        }
        FromTo $whenArgs $xData $vectors
        if {[dict exists $whenArgs vec1]} {
            return [::tclmeasure::FindDerivWhen${suffix} $xData wheneq {}\
                            [InputData $data [dict get $whenArgs vec1] $vectors] {}\
                            [InputData $data [dict get $whenArgs vec2] $vectors]\
                            $whenVecCond [dict get $whenArgs $whenVecCond] [dict get $whenArgs delay] $from $to]
        } else {
            return [::tclmeasure::FindDerivWhen${suffix} $xData when {}\
                            [InputData $data [dict get $whenArgs vec] $vectors] [dict get $whenArgs val] {}\
                            $whenVecCond [dict get $whenArgs $whenVecCond] [dict get $whenArgs delay] $from $to]
        }
    } elseif {[info exists find] && [info exists at]} {
        return [::tclmeasure::FindAt${suffix} $xData $at [InputData $data $find $vectors]]
    } elseif {[info exists deriv] && [info exists at]} {
        return [::tclmeasure::DerivAt${suffix} $xData $at [InputData $data $deriv $vectors]]
    } elseif {[info exists integ]} {
        FromTo $integArgs $xData $vectors
        set result [::tclmeasure::Integ${suffix} $xData [InputData $data [dict get $integArgs vec] $vectors] $from $to\
                        [dict get $integArgs cum]]
        if {$sequence && $output eq {vector}} {
            return [uplevel 1 [list ::tclmeasure::NativePublish $result $outputOptions]]
        }
        return $result
    } elseif {[info exists avg]} {
        set avgArgs [argparse -inline [ConditionDefinition interval] $avg]
        FromTo $avgArgs $xData $vectors
        return [::tclmeasure::Avg${suffix} $xData [InputData $data [dict get $avgArgs vec] $vectors] $from $to]
    } elseif {[info exists rms]} {
        set rmsArgs [argparse -inline [ConditionDefinition interval] $rms]
        FromTo $rmsArgs $xData $vectors
        if {$vectors} {
            return [::tclmeasure::RmsVectors $xData [InputData $data [dict get $rmsArgs vec] $vectors] $from $to false]
        }
        return [::tclmeasure::Rms $xData [InputData $data [dict get $rmsArgs vec] $vectors] $from $to]
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
        set resDict [argparse -inline [ConditionDefinition extrema] $argsDict]
        FromTo $resDict $xData $vectors
        set result [::tclmeasure::MinMaxPPMinAtMaxAt${suffix} $xData [InputData $data [dict get $resDict vec] $vectors]\
                            $from $to $type]
        if {$type eq {between} && $output eq {vector}} {
            return [uplevel 1 [list ::tclmeasure::NativePublish $result $outputOptions]]
        }
        return $result
    }
}

proc ::tclmeasure::Avg {x y xstart xend} {
    set integral [Integ $x $y $xstart $xend false]
    return [expr {$integral/($xend-$xstart)}]
}

proc ::tclmeasure::Rms {x y xstart xend} {
    set ySq [lmap yVal $y {expr {$yVal*$yVal}}]
    set integral [Integ $x $ySq $xstart $xend false]
    return [expr {sqrt($integral/($xend-$xstart))}]
}

proc ::tclmeasure::AvgVectors {x y xstart xend} {
    set integral [IntegVectors $x $y $xstart $xend false]
    return [expr {$integral/($xend-$xstart)}]
}
