
package require ruff
#source /home/georgtree/tcl/ruff/src/ruff.tcl
package require fileutil

set docDir [file dirname [file normalize [info script]]]
set sourceDir "${docDir}/../"
source [file join $docDir startPage.ruff]
source [file join $sourceDir tclmeasure.tcl]

set packageVersion [package versions tclmeasure]
set title "tclmeasure package"
set commonSphinx [list -title $title -sortnamespaces false -preamble $startPage -pagesplit namespace -recurse false\
                        -includesource false -pagesplit namespace -autopunctuate true -compact false\
                        -excludeprocs {^[A-Z].*} -includeprivate false -product tclmeasure -diagrammer\
                        "ditaa --border-width 1" -version $packageVersion -copyright "George Yashin" {*}$::argv]
set commonNroff [list -title $title -sortnamespaces false -preamble $startPage -pagesplit namespace -recurse false\
                         -pagesplit namespace -autopunctuate true -compact true -includeprivate false \
                         -excludeprocs {^[A-Z].*} -product tclmeasure -diagrammer "ditaa --border-width 1"\
                         -version $packageVersion -copyright "George Yashin" {*}$::argv]
set namespaces [list ::tclmeasure]

ruff::document $namespaces -format sphinx -outdir [file join $docDir sphinx] {*}$commonSphinx
ruff::document $namespaces -format nroff -outdir $docDir -outfile tclmeasure.n {*}$commonNroff

::fileutil::appendToFile [file join $docDir sphinx conf.py] {html_theme = "classic"
extensions = [
    "sphinx.ext.githubpages",
]
from pygments.lexers.tcl import TclLexer
from pygments.token import Operator

class MyTclLexer(TclLexer):
    def get_tokens_unprocessed(self, text):
        for i, t, v in super().get_tokens_unprocessed(text):
            if v == "=":
                yield i, Operator, v   # or Name.Builtin
            else:
                yield i, t, v

def setup(app):
    from sphinx.highlighting import lexers
    lexers["tcl"] = MyTclLexer()
}
catch {exec sphinx-build -b html [file join $docDir sphinx] [file join $docDir]} errorStr
puts $errorStr
