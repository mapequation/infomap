.onLoad <- function(libname, pkgname) {
  # Refresh SWIG ref-class metadata so derived classes pick up
  # methods from the loaded shared library.
  methods::cacheMetaData(1L)
}

.onAttach <- function(libname, pkgname) {
  # Quiet by design.
}
