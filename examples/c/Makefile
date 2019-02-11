CXXFLAGS = -Wall

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

# Set INFOMAP_DIR to your Infomap directory
INFOMAP_DIR = ../..
INFOMAP_LIB = $(INFOMAP_DIR)/lib/libInfomap.a

.PHONY: clean distclean

example: example.o $(INFOMAP_LIB) Makefile
	$(CXX) $(CXXFLAGS) -DNS_INFOMAP $< -o $@ -I$(INFOMAP_DIR)/include -L$(INFOMAP_DIR)/lib -lInfomap

example.o: example.c $(INFOMAP_LIB) Makefile
	$(CC) -c $< -o $@ -I$(INFOMAP_DIR)/include

$(INFOMAP_LIB):
	$(MAKE) -C $(INFOMAP_DIR) lib

clean:
	$(RM) example

distclean:
	$(MAKE) -C $(INFOMAP_DIR) clean
