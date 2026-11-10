package require ruff
package require fileutil

set docDir [file dirname [file normalize [info script]]]
set sourceDir "${docDir}/../"
source [file join $docDir startPage.ruff]
source [file join $sourceDir tclmeasure.tcl]

set packageVersion [package provide tclmeasure]
if {$packageVersion eq {}} {
    return -code error "Package version is empty"
}
set title "tclmeasure package"
set commonSphinx [list -title $title -sortnamespaces false -preamble $startPage -pagesplit namespace -recurse false\
                        -includesource false -pagesplit namespace -autopunctuate true -compact false\
                        -excludeprocs {^[A-Z].*} -includeprivate false -product tclmeasure -diagrammer\
                        "ditaa --border-width 1" -version $packageVersion -copyright "George Yashin" {*}$::argv]
set commonNroff [list -title $title -sortnamespaces false -preamble $startPage -pagesplit namespace -recurse false\
                         -pagesplit none -autopunctuate true -compact true -includeprivate false \
                         -excludeprocs {^[A-Z].*} -product tclmeasure -diagrammer "ditaa --border-width 1"\
                         -version $packageVersion -copyright "George Yashin" {*}$::argv]
set namespaces [list ::tclmeasure]

ruff::document $namespaces -format sphinx -outfile tclmeasure.rst -outdir [file join $docDir sphinx] {*}$commonSphinx
ruff::document $namespaces -format nroff -outdir $docDir -outfile tclmeasure.n {*}$commonNroff

::fileutil::appendToFile [file join $docDir sphinx conf.py] {
extensions = list(dict.fromkeys(globals().get("extensions", []) + [
    "sphinx_rtd_theme",
    "sphinx.ext.githubpages",
]))
html_theme = "sphinx_rtd_theme"
html_theme_options = {
    "collapse_navigation": False,
    "sticky_navigation": True,
    "navigation_depth": 4,
    "includehidden": True,
    "titles_only": False,
}
# Custom CSS is staged by doc_gen.tcl; the source lives inside the HTML outdir.
html_static_path = []
html_css_files = list(globals().get("html_css_files", [])) + ["manual.css"]
from pygments.lexers.tcl import TclLexer
from pygments.token import Operator

class MyTclLexer(TclLexer):
    def get_tokens_unprocessed(self, text):
        for i, t, v in super().get_tokens_unprocessed(text):
            if v == "=":
                yield i, Operator, v   # or Name.Builtin
            elif v == "$":
                yield i, Operator, v   # or Name.Builtin
            elif v == "\\":
                yield i, Operator, v   # or Name.Builtin
            elif v == "%":
                yield i, Operator, v   # or Name.Builtin
            elif v == "'":
                yield i, Operator, v   # or Name.Builtin
            else:
                yield i, t, v

def setup(app):
    from sphinx.highlighting import lexers
    lexers["tcl"] = MyTclLexer()
}
# Use one Python environment for Sphinx and the RTD theme; propagate failures.
set python python
if {[info exists ::env(DOC_PYTHON)] && $::env(DOC_PYTHON) ne {}} {
    set python $::env(DOC_PYTHON)
}
file mkdir [file join $docDir _static]
file copy -force [file join $docDir theme manual.css] [file join $docDir _static manual.css]
exec -- $python -m sphinx -E -a -b html [file join $docDir sphinx] $docDir >@stdout 2>@stderr
