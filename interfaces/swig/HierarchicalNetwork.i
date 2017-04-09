%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/io/HierarchicalNetwork.h"
%}

%include "std_string.i"
%include "std_deque.i"

// Instantiate templates used
namespace std {
   %template(UIntDeque) deque<unsigned int>;
}

/* Parse the header file to generate wrappers */
#include "src/io/HierarchicalNetwork.h"

#ifdef SWIGPYTHON
%extend LeafIterator
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

%extend TreeIterator
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
