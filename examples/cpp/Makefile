CXXFLAGS = -Wall -O3

# Set INFOMAP_DIR to your Infomap directory
INFOMAP_DIR = ../..
LIB_DIR = $(INFOMAP_DIR)/build/lib
INFOMAP_LIB = $(LIB_DIR)/libInfomap.a

example: example.cpp $(INFOMAP_LIB) Makefile
	$(CXX) $(CXXFLAGS) -DUSE_NS $< -o $@ -I$(LIB_DIR)/include -L$(LIB_DIR) -lInfomap

$(INFOMAP_LIB):
	cd $(INFOMAP_DIR) && $(MAKE) lib