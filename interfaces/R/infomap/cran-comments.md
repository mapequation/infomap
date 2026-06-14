## Submission

This is a new submission of the `infomap` package: R bindings for the Infomap
network clustering algorithm (the Map Equation), built as an R6 facade over
SWIG-generated bindings to the bundled C++ sources.

## R CMD check results

0 errors | 0 warnings | 1 note

The note is the standard new-submission note:

* checking CRAN incoming feasibility ... NOTE
  Maintainer: 'Daniel Edler <daniel.edler@umu.se>'
  New submission

Two informational items may also appear and are expected:

* Installed size is ~6 MB, dominated by the compiled shared library and the
  generated SWIG wrapper. The package vendors the Infomap C++ sources, so the
  compiled object is inherently sizeable; we have kept the shipped sources
  minimal (build artifacts are excluded via `.Rbuildignore`).
* The package requires a C++17 compiler (declared in `SystemRequirements`).

## Test environments

- local: macOS, R 4.6.0
- (fill in win-builder / R-hub / GitHub Actions results before submitting)

## Reverse dependencies

None (new package).
