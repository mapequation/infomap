#Generated Makefile
CXX  = g++
CXXFLAGS = -Wall -O4 -pipe -fopenmp
LINKER = $(CXX)
LFLAGS = -fopenmp

HEADERS = \
src/infomap/Edge.h \
src/infomap/flowData.h \
src/infomap/flowData_traits.h \
src/infomap/FlowNetwork.h \
src/infomap/InfomapBase.h \
src/infomap/InfomapContext.h \
src/infomap/InfomapDirected.h \
src/infomap/InfomapDirectedUnrecordedTeleportation.h \
src/infomap/InfomapGreedy.h \
src/infomap/InfomapUndirdir.h \
src/infomap/InfomapUndirected.h \
src/infomap/Network.h \
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
src/infomap/InfomapDirected.cpp \
src/infomap/InfomapDirectedUnrecordedTeleportation.cpp \
src/infomap/InfomapGreedy.cpp \
src/infomap/InfomapUndirdir.cpp \
src/infomap/InfomapUndirected.cpp \
src/infomap/Network.cpp \
src/infomap/Node.cpp \
src/infomap/TreeData.cpp \
src/Infomap.cpp \
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

.PHONY: all clean

## Default rule executed
all: $(TARGET)
	@true

## Clean Rule
clean:
	@-rm -f $(TARGET) $(OBJECTS)

noomp: $(OBJECTS)
	@echo "Linking object files to target $(TARGET) without OpenMP..."
	@$(LINKER) -o $(TARGET) $^
	@echo "-- Link finished --"

## Rule for making the actual target
$(TARGET): $(OBJECTS)
	@echo "Linking object files to target $@..."
	@$(LINKER) $(LFLAGS) -o $@ $^
	@echo "-- Link finished --"

## Generic compilation rule for object files from cpp files
build/%.o : src/%.cpp $(HEADERS) Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@
