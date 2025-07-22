set path_to_hl_tcl "/home/georgtree/tcl/hl_tcl"
package require ruff
package require fileutil
package require hl_tcl
set docDir [file dirname [file normalize [info script]]]
set sourceDir "${docDir}/.."
source [file join $docDir startPage.ruff]
source [file join $sourceDir tclmeasure.tcl]

set packageVersion [package versions tclmeasure]
set title "tclmeasure package"
set commonHtml [list -title $title -sortnamespaces false -preamble $startPage -pagesplit namespace -recurse false\
                        -includesource true -autopunctuate true -compact false -includeprivate true -product tclmeasure\
                        -excludeprocs {^[A-Z].*} -diagrammer "ditaa --border-width 1" -version $packageVersion\
                        -copyright "George Yashin" {*}$::argv]
set commonNroff [list -title $title -sortnamespaces false  -recurse false -autopunctuate true -compact false\
                         -includeprivate true -product tclmeasure -excludeprocs {^[A-Z].*}\
                         -diagrammer "ditaa --border-width 1" -version $packageVersion\
                         -copyright "George Yashin" {*}$::argv]
set namespaces [list ::tclmeasure]

if {[llength $argv] == 0 || "html" in $argv} {
    ruff::document $namespaces -format html -outdir $docDir -outfile index.html {*}$commonHtml
    ruff::document $namespaces -format nroff -outdir $docDir -outfile tclmeasure.n {*}$commonNroff
}

foreach file [glob ${docDir}/*.html] {
    exec tclsh "${path_to_hl_tcl}/tcl_html.tcl" [file join ${docDir} $file]
}

proc processContents {fileContents} {
    return [string map {max-width:60rem max-width:100rem} $fileContents]
}

fileutil::updateInPlace [file join $docDir assets ruff-min.css] processContents
