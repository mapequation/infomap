CXXFLAGS = -Wall -O3

# Set INFOMAP_DIR to your Infomap directory
INFOMAP_DIR = ../../..
INFOMAP_LIB = $(INFOMAP_DIR)/lib/libInfomap.a
INC_INFOMAP = -I$(INFOMAP_DIR)/include -L$(INFOMAP_DIR)/lib -lInfomap

# Library to simplify Infomap-igraph interface
INTERFACE_DIR = $(INFOMAP_DIR)/examples/cpp/Infomap-igraph-interface-library
INC_INTERFACE = -I$(INTERFACE_DIR)/include -L$(INTERFACE_DIR)/lib -lInfomap-igraph-interface

# igraph paths
IGRAPH_LIB_DIR = /usr/local/lib
IGRAPH_INCLUDE_DIR = /usr/local/include/igraph
INC_IGRAPH = -I$(IGRAPH_INCLUDE_DIR) -L$(IGRAPH_LIB_DIR) -ligraph

.PHONY: example-igraph

example-igraph: example-igraph.cpp $(INFOMAP_LIB)
	$(CXX) $(CXXFLAGS) $< -o $@ -DNS_INFOMAP $(INC_IGRAPH) $(INC_INFOMAP) $(INC_INTERFACE)


# # Rule for lib objects
# build/%.o: src/%.cpp src/%.h $(INFOMAP_LIB) Makefile
# 	@mkdir -p $(dir $@)
# 	$(CXX) $(CXXFLAGS) -DNS_INFOMAP -c $< -o $@ -I$(IGRAPH_INCLUDE_DIR) -I$(INFOMAP_DIR)/include

# Make Infomap library
$(INFOMAP_LIB):
	cd $(INFOMAP_DIR) && $(MAKE) lib

clean:
	$(RM) example-igraph
