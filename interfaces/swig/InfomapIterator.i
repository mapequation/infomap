%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/core/InfomapIterator.h"
%}

%include "std_deque.i"

namespace std {
    %template(deque_uint) std::deque<unsigned int>;
}

%include "InfoNode.i"

/* Parse the header file to generate wrappers */
%include "src/core/InfomapIterator.h"


#ifdef SWIGPYTHON
%extend infomap::InfomapIterator
{

	// Make the class iterable, and wait until
	// first call to next() to yield first element
	%insert("python") %{
		def __iter__(self):
			self._firstYielded = False
			return self

		def __next__(self):
			if not self._firstYielded:
				self._firstYielded = True
			else:
				self.stepForward()

			if self.isEnd():
				raise StopIteration

			return self

		
		@property
		def module_id(self):
			"""Get the module id of the node."""
			return self.moduleIndex() + 1

		_path = path
		@property
		def path(self):
			return self._path()
		
		_depth = depth
		@property
		def depth(self):
			return self._depth()
		
		@property
		def child_index(self):
			return self.childIndex()
		
		# Forward to the node it currently points to
		def __getattr__(self, name):
			return getattr(self.current(), name)

	%}
}



%extend infomap::InfomapLeafModuleIterator
{

	// Make the class iterable, and wait until
	// first call to next() to yield first element
	%insert("python") %{
		def __iter__(self):
			self._firstYielded = False
			return self

		def __next__(self):
			if not self._firstYielded:
				self._firstYielded = True
			else:
				self.stepForward()

			if self.isEnd():
				raise StopIteration

			return self
		
		@property
		def module_id(self):
			"""Get the module id of the node."""
			return self.moduleIndex() + 1
		
		_path = path
		@property
		def path(self):
			return self._path()
		
		_depth = depth
		@property
		def depth(self):
			return self._depth()
		
		@property
		def child_index(self):
			return self.childIndex()
		
		# Forward to the node it currently points to
		def __getattr__(self, name):
			return getattr(self.current(), name)

	%}
}



%extend infomap::InfomapLeafIterator
{

	// Make the class iterable, and wait until
	// first call to next() to yield first element
	%insert("python") %{
		def __iter__(self):
			self._firstYielded = False
			return self

		def __next__(self):
			if not self._firstYielded:
				self._firstYielded = True
			else:
				self.stepForward()

			if self.isEnd():
				raise StopIteration

			return self
		
		
		@property
		def module_id(self):
			"""Get the module id of the node."""
			return self.moduleIndex() + 1

		_path = path
		@property
		def path(self):
			return self._path()
		
		_depth = depth
		@property
		def depth(self):
			return self._depth()
		
		@property
		def child_index(self):
			return self.childIndex()
		
		# Forward to the node it currently points to
		def __getattr__(self, name):
			return getattr(self.current(), name)

	%}
}



%extend infomap::InfomapIteratorPhysical
{

	// Make the class iterable, and wait until
	// first call to next() to yield first element
	%insert("python") %{
		def __iter__(self):
			self._firstYielded = False
			return self

		def __next__(self):
			if not self._firstYielded:
				self._firstYielded = True
			else:
				self.stepForward()

			if self.isEnd():
				raise StopIteration

			return self
		
		
		@property
		def module_id(self):
			"""Get the module id of the node."""
			return self.moduleIndex() + 1
		
		_path = path
		@property
		def path(self):
			return self._path()
		
		_depth = depth
		@property
		def depth(self):
			return self._depth()
		
		@property
		def child_index(self):
			return self.childIndex()
		
		# Forward to the node it currently points to
		def __getattr__(self, name):
			return getattr(self.current(), name)

	%}
}



%extend infomap::InfomapLeafIteratorPhysical
{

	// Make the class iterable, and wait until
	// first call to next() to yield first element
	%insert("python") %{
		def __iter__(self):
			self._firstYielded = False
			return self

		def __next__(self):
			if not self._firstYielded:
				self._firstYielded = True
			else:
				self.stepForward()

			if self.isEnd():
				raise StopIteration

			return self
		
		
		@property
		def module_id(self):
			"""Get the module id of the node."""
			return self.moduleIndex() + 1

		_path = path
		@property
		def path(self):
			return self._path()
		
		_depth = depth
		@property
		def depth(self):
			return self._depth()
		
		@property
		def child_index(self):
			return self.childIndex()
		
		# Forward to the node it currently points to
		def __getattr__(self, name):
			return getattr(self.current(), name)

	%}
}
#endif