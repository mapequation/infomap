# SWIG emits delete S4 methods that refer to namespace-qualified helper names,
# while the generated helper functions use shorter R names. Keep the aliases in
# hand-written R code so R CMD check can resolve the methods statically.
delete_infomap__DeltaFlow <- function(obj) delete_DeltaFlow(obj)
delete_infomap__EdgeData <- function(obj) delete_EdgeData(obj)
delete_infomap__FlowData <- function(obj) delete_FlowData(obj)
delete_infomap__InfoEdge <- function(obj) delete_InfoEdge(obj)
delete_infomap__LinkResult <- function(obj) delete_LinkResult(obj)
delete_infomap__InfomapIterator <- function(obj) delete_InfomapIterator(obj)
delete_infomap__InfomapIteratorPhysical <- function(obj) {
  delete_InfomapIteratorPhysical(obj)
}
delete_infomap__InfomapLeafIterator <- function(obj) {
  delete_InfomapLeafIterator(obj)
}
delete_infomap__InfomapLeafIteratorPhysical <- function(obj) {
  delete_InfomapLeafIteratorPhysical(obj)
}
delete_infomap__InfomapLeafModuleIterator <- function(obj) {
  delete_InfomapLeafModuleIterator(obj)
}
delete_infomap__InfomapModuleIterator <- function(obj) {
  delete_InfomapModuleIterator(obj)
}
delete_infomap__InfomapParentIterator <- function(obj) {
  delete_InfomapParentIterator(obj)
}
delete_infomap__InfomapWrapper <- function(obj) delete_InfomapWrapper(obj)
delete_infomap__InfoNode <- function(obj) delete_InfoNode(obj)
delete_infomap__MemDeltaFlow <- function(obj) delete_MemDeltaFlow(obj)
delete_infomap__PhysData <- function(obj) delete_PhysData(obj)
delete_std__dequeT_unsigned_int_t <- function(obj) delete_deque_uint(obj)
delete_std__mapT_std__pairT_unsigned_int_unsigned_int_t_double_t <- function(
  obj
) {
  delete_map_pair_uint_uint_double(obj)
}
delete_std__mapT_unsigned_int_std__string_t <- function(obj) {
  delete_map_uint_string(obj)
}
delete_std__mapT_unsigned_int_std__vectorT_unsigned_int_t_t <- function(obj) {
  delete_map_uint_vector_uint(obj)
}
delete_std__mapT_unsigned_int_unsigned_int_t <- function(obj) {
  delete_map_uint_uint(obj)
}
delete_std__pairT_unsigned_int_unsigned_int_t <- function(obj) {
  delete_pair_uint_uint(obj)
}
delete_std__vectorT_double_t <- function(obj) delete_vector_double(obj)
delete_std__vectorT_infomap__LinkResult_t <- function(obj) {
  delete_vector_link_result(obj)
}
delete_std__vectorT_unsigned_int_t <- function(obj) delete_vector_uint(obj)
