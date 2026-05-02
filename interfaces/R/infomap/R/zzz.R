.onLoad <- function(libname, pkgname) {
  # Refresh SWIG ref-class metadata so derived classes pick up
  # methods from the loaded shared library.
  methods::cacheMetaData(1L)
  # Route Infomap's verbose log output through R's console
  # (Rprintf-backed streambuf installed via the SWIG-generated hook).
  if (exists("initRLogging", mode = "function")) {
    initRLogging()
  }
}

.onAttach <- function(libname, pkgname) {
  # Quiet by design.
}
