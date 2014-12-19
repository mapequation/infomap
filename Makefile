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
ifeq "$(findstring lib, $(MAKECMDGOALS))" "lib"
	CXXFLAGS += -DUSE_NS
endif
endif

HEADERS := $(shell find src -name "*.h")
SOURCES := $(shell find src -name "*.cpp" -depth 2)

OBJECTS := $(SOURCES:src/%.cpp=build/%.o)
INFOMAP_OBJECT = build/Infomap.o
INFORMATTER_OBJECT = build/Informatter.o

LIBDIR = build/lib
LIBTARGET = $(LIBDIR)/libInfomap.a
LIBHEADERS := $(HEADERS:src/%.h=$(LIBDIR)/include/%.h)
INFOMAP_LIB_OBJECT = build/Infomaplib.o

SWIG_FILES := $(shell find swig -name "*.i")
SWIG_HEADERS = src/Infomap.h src/infomap/Network.h src/io/HierarchicalNetwork.h
PY_BUILD_DIR = build/py
PY_HEADERS := $(HEADERS:src/%.h=$(PY_BUILD_DIR)/src/%.h)
PY_SOURCES := $(SOURCES:src/%.cpp=$(PY_BUILD_DIR)/src/%.cpp)
PY_SOURCES += $(PY_BUILD_DIR)/src/Infomap.cpp

.PHONY: all clean noomp lib

## Rule for making the actual target
Infomap: $(OBJECTS) $(INFOMAP_OBJECT)
	@echo "Linking object files to target $@..."
	$(CXX) $(LDFLAGS) -o $@ $^
	@echo "-- Link finished --"

Infomap-formatter: $(OBJECTS) $(INFORMATTER_OBJECT)
	@echo "Making Informatter..."
	$(CXX) $(LDFLAGS) -o $@ $^

all: Infomap Infomap-formatter
	@true

python: py-build Makefile
	cd $(PY_BUILD_DIR) && python setup.py build_ext --inplace
	@true

setup.py:
	cd $(PY_BUILD_DIR) && python setup.py build_ext --inplace

py-build: Makefile $(PY_HEADERS) $(PY_SOURCES)
	@mkdir -p $(PY_BUILD_DIR)
	@cp -a $(SWIG_FILES) $(PY_BUILD_DIR)/
	@cp -a swig/setup.py $(PY_BUILD_DIR)/
	swig -c++ -python -outdir $(PY_BUILD_DIR) -o $(PY_BUILD_DIR)/infomap_wrap.cpp $(PY_BUILD_DIR)/Infomap.i

lib: $(LIBTARGET) $(LIBHEADERS)
	@echo "Wrote static library and headers to $(LIBDIR)"

$(LIBTARGET): $(INFOMAP_LIB_OBJECT) $(OBJECTS)
	@echo "Creating static library..."
	@mkdir -p $(LIBDIR)
	ar rcs $@ $^

$(LIBDIR)/include/%.h: src/%.h
	@mkdir -p $(dir $@)
	@cp -a $^ $@

$(PY_BUILD_DIR)/src/%.h: src/%.h
	@mkdir -p $(dir $@)
	@cp -a $^ $@

$(PY_BUILD_DIR)/src/%.cpp: src/%.cpp
	@mkdir -p $(dir $@)
	@cp -a $^ $@

$(INFOMAP_LIB_OBJECT): src/Infomap.cpp $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DNO_MAIN -c $< -o $@

## Generic compilation rule for object files from cpp files
build/%.o : src/%.cpp $(HEADERS) Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

noomp: Infomap
	@true

## Clean Rule
clean:
	$(RM) -r Infomap Informatter build