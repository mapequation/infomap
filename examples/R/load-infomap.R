loadInfomap <- function() {
	message("Loading Infomap...")
	dyn.load(paste("infomap/infomap", .Platform$dynlib.ext, sep=""))
	source("infomap/infomap.R")
	# The cacheMetaData(1) will cause R to refresh its object tables. Without it, inheritance of wrapped objects may fail.
	cacheMetaData(1)
}


# Only load if not already loaded
if (!exists("HierarchicalNetwork")) {
	loadInfomap()
} else {
	message("Infomap already loaded!")
}