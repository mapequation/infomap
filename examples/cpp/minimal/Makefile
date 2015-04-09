CXXFLAGS = -Wall -O3

# Set INFOMAP_DIR to your Infomap directory
INFOMAP_DIR = ../../..
INFOMAP_LIB = $(INFOMAP_DIR)/lib/libInfomap.a

.PHONY: clean distclean

example: example.cpp $(INFOMAP_LIB) Makefile
	$(CXX) $(CXXFLAGS) -DNS_INFOMAP $< -o $@ -I$(INFOMAP_DIR)/include -L$(INFOMAP_DIR)/lib -lInfomap

mem-example: mem-example.cpp $(INFOMAP_LIB) Makefile
	$(CXX) $(CXXFLAGS) -DNS_INFOMAP $< -o $@ -I$(INFOMAP_DIR)/include -L$(INFOMAP_DIR)/lib -lInfomap

multi-example: multi-example.cpp $(INFOMAP_LIB) Makefile
	$(CXX) $(CXXFLAGS) -DNS_INFOMAP $< -o $@ -I$(INFOMAP_DIR)/include -L$(INFOMAP_DIR)/lib -lInfomap

$(INFOMAP_LIB):
	cd $(INFOMAP_DIR) && $(MAKE) lib

clean:
	$(RM) example

distclean:
	cd $(INFOMAP_DIR) && $(MAKE) clean