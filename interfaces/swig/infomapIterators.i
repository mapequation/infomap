%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/core/infomapIterators.h"
%}

%include "treeIterators.i"
%include "std_deque.i"

%template(deque_uint) std::deque<unsigned int>;

%import "NodeBase.i"

/* Parse the header file to generate wrappers */
%include "src/core/infomapIterators.h"


namespace infomap {
    %template(InfomapDepthFirstIteratorNodeBase) InfomapDepthFirstIterator<NodeBase*>;
    %template(InfomapDepthFirstIteratorNodeBaseConst) InfomapDepthFirstIterator<NodeBase const*>;
}

#ifdef SWIGPYTHON
%extend InfomapDepthFirstIterator
{

	// Make the class iterable, and wait until
	// first call to next() to yield first element
	%insert("python") %{
		def __iter__(self):
			self._firstYielded = False
			return self

		def next(self):
			if not self._firstYielded:
				self._firstYielded = True
			else:
				self.stepForward()

			if self.isEnd():
				raise StopIteration

			return self

		def __next__(self):
			return self.next()
	%}
}
#endif
