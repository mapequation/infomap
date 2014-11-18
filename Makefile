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

HEADERS = \
src/infomap/Edge.h \
src/infomap/flowData.h \
src/infomap/flowData_traits.h \
src/infomap/FlowNetwork.h \
src/infomap/InfomapBase.h \
src/infomap/InfomapContext.h \
src/infomap/InfomapGreedy.h \
src/infomap/InfomapGreedyCommon.h \
src/infomap/InfomapGreedySpecialized.h \
src/infomap/InfomapGreedyTypeSpecialized.h \
src/infomap/MemFlowNetwork.h \
src/infomap/MemNetwork.h \
src/infomap/MultiplexNetwork.h \
src/infomap/Network.h \
src/infomap/NetworkAdapter.h \
src/infomap/Node.h \
src/infomap/NodeFactory.h \
src/infomap/TreeData.h \
src/infomap/treeIterators.h \
src/io/ClusterReader.h \
src/io/Config.h \
src/io/convert.h \
src/io/HierarchicalNetwork.h \
src/io/Options.h \
src/io/ProgramInterface.h \
src/io/SafeFile.h \
src/io/TreeDataWriter.h \
src/io/version.h \
src/utils/Date.h \
src/utils/FileURI.h \
src/utils/gap_iterator.h \
src/utils/infomath.h \
src/utils/Logger.h \
src/utils/MersenneTwister.h \
src/utils/Stopwatch.h \
src/utils/types.h \

SOURCES = \
src/infomap/FlowNetwork.cpp \
src/infomap/InfomapBase.cpp \
src/infomap/InfomapContext.cpp \
src/infomap/MemFlowNetwork.cpp \
src/infomap/MemNetwork.cpp \
src/infomap/MultiplexNetwork.cpp \
src/infomap/Network.cpp \
src/infomap/NetworkAdapter.cpp \
src/infomap/Node.cpp \
src/infomap/TreeData.cpp \
src/io/ClusterReader.cpp \
src/io/HierarchicalNetwork.cpp \
src/io/Options.cpp \
src/io/ProgramInterface.cpp \
src/io/TreeDataWriter.cpp \
src/io/version.cpp \
src/utils/FileURI.cpp \
src/utils/Logger.cpp \

TARGET = Infomap

OBJECTS = $(SOURCES:src/%.cpp=build/%.o)
INFOMAP_OBJECT = build/Infomap.o
INFORMATTER_OBJECT = build/Informatter.o

.PHONY: all clean

## Default rule executed
all: $(TARGET) Informatter
	@true

## Clean Rule
clean:
	$(RM) $(TARGET) Informatter $(OBJECTS)

noomp: $(TARGET)
	@true

## Rule for making the actual target
$(TARGET): $(OBJECTS) $(INFOMAP_OBJECT)
	@echo "Linking object files to target $@..."
	$(CXX) $(LDFLAGS) -o $@ $^
	@echo "-- Link finished --"

Informatter: $(OBJECTS) $(INFORMATTER_OBJECT)
	@echo "Making Informatter..."
	$(CXX) $(LDFLAGS) -o $@ $^

## Generic compilation rule for object files from cpp files
build/%.o : src/%.cpp $(HEADERS) Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@
	