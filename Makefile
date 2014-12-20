CXXFLAGS = -Wall -pipe
LDFLAGS =
CXX_CLANG := $(shell $(CXX) --version 2>/dev/null | grep clang)
ifeq "$(CXX_CLANG)" ""
	CXXFLAGS += -O4
	ifneq "$(findstring noomp, $(MAKECMDGOALS))" "noomp"
		CXXFLAGS += -fopenmp
		LDFLAGS += -fopenmp
	endif
else
	CXXFLAGS += -O3
endif

##################################################
# General file dependencies
##################################################

HEADERS := $(shell find src -name "*.h")
# Only one main
SOURCES := $(shell find src -name "*.cpp" -depth 2)
SOURCES += src/Infomap.cpp
OBJECTS := $(SOURCES:src/%.cpp=build/%.o)

##################################################
# Stand-alone C++ targets
##################################################

INFORMATTER_OBJECTS = $(OBJECTS:Infomap.o=Informatter.o)

.PHONY: all noomp test

Infomap: $(OBJECTS)
	@echo "Linking object files to target $@..."
	$(CXX) $(LDFLAGS) -o $@ $^
	@echo "-- Link finished --"

Infomap-formatter: $(INFORMATTER_OBJECTS)
	@echo "Making Informatter..."
	$(CXX) $(LDFLAGS) -o $@ $^

all: Infomap Infomap-formatter
	@true

## Generic compilation rule for object files from cpp files
build/%.o : src/%.cpp $(HEADERS) Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

noomp: Infomap
	@true


##################################################
# Static C++ library
##################################################

# Use separate object files to compile with definitions
# USE_NS: Use namespace infomap
# NO_MAIN: Skip main function
LIB_DIR = build/lib
LIB_HEADERS := $(HEADERS:src/%.h=$(LIB_DIR)/include/%.h)
LIB_OBJECTS := $(SOURCES:src/%.cpp=$(LIB_DIR)/build/%.o)

.PHONY: lib

lib: $(LIB_DIR)/libInfomap.a $(LIB_HEADERS)
	@echo "Wrote static library and headers to $(LIB_DIR)"

$(LIB_DIR)/libInfomap.a: $(LIB_OBJECTS) Makefile
	@echo "Creating static library..."
	@mkdir -p $(LIB_DIR)
	ar rcs $@ $^

# Rule for $(LIB_HEADERS)
$(LIB_DIR)/include/%.h: src/%.h
	@mkdir -p $(dir $@)
	@cp -a $^ $@

# Rule for $(LIB_OBJECTS)
$(LIB_DIR)/build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DUSE_NS -DNO_MAIN -c $< -o $@


##################################################
# General SWIG helpers
##################################################

SWIG_FILES := $(shell find swig -name "*.i")


##################################################
# Python module
##################################################

PY_BUILD_DIR = build/py
PY_HEADERS := $(HEADERS:src/%.h=$(PY_BUILD_DIR)/src/%.h)
PY_SOURCES := $(SOURCES:src/%.cpp=$(PY_BUILD_DIR)/src/%.cpp)

.PHONY: python py-build

# Use python distutils to compile the module
python: py-build Makefile
	@cp -a swig/setup.py $(PY_BUILD_DIR)/
	cd $(PY_BUILD_DIR) && python setup.py build_ext --inplace
	@true

# Generate wrapper files from source and interface files
py-build: Makefile $(PY_HEADERS) $(PY_SOURCES)
	@mkdir -p $(PY_BUILD_DIR)
	@cp -a $(SWIG_FILES) $(PY_BUILD_DIR)/
	swig -c++ -python -outdir $(PY_BUILD_DIR) -o $(PY_BUILD_DIR)/infomap_wrap.cpp $(PY_BUILD_DIR)/Infomap.i

# Rule for $(PY_HEADERS) and $(PY_SOURCES)
$(PY_BUILD_DIR)/src/%: src/%
	@mkdir -p $(dir $@)
	@cp -a $^ $@



##################################################
# Clean
##################################################

clean:
	$(RM) -r Infomap Infomap-formatter build