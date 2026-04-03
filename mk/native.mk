NATIVE_BINARY := Infomap
NATIVE_OBJECT_DIR := build/native/$(MODE)-omp$(OPENMP)
LIB_OUTPUT := lib/libInfomap.a
LIB_OBJECT_DIR := build/lib/$(MODE)-omp$(OPENMP)
PUBLIC_INCLUDE_DIR := include

NATIVE_OBJECTS := $(SOURCES:src/%.cpp=$(NATIVE_OBJECT_DIR)/%.o)
LIB_OBJECTS := $(SOURCES:src/%.cpp=$(LIB_OBJECT_DIR)/%.o)
LIB_HEADERS := $(HEADERS:src/%.h=$(PUBLIC_INCLUDE_DIR)/%.h)

CXX_VERSION := $(shell $(CXX) --version 2>/dev/null || true)
CXX_IS_CLANG := $(if $(findstring clang,$(CXX_VERSION)),1,0)

BASE_WARNING_FLAGS := -Wall -Wextra -pedantic -Wnon-virtual-dtor -std=c++14
MODE_CXXFLAGS :=
PLATFORM_SUPPORT_CXXFLAGS :=
PLATFORM_SUPPORT_LDFLAGS :=
OPENMP_CXXFLAGS :=
OPENMP_LDFLAGS :=

ifneq ($(BREW_PREFIX),)
PLATFORM_SUPPORT_CXXFLAGS += -I$(BREW_PREFIX)/include
PLATFORM_SUPPORT_LDFLAGS += -L$(BREW_PREFIX)/lib
endif

ifneq ($(MACOSX_DEPLOYMENT_TARGET),)
PLATFORM_SUPPORT_CXXFLAGS += -mmacosx-version-min=$(MACOSX_DEPLOYMENT_TARGET)
PLATFORM_SUPPORT_LDFLAGS += -mmacosx-version-min=$(MACOSX_DEPLOYMENT_TARGET)
endif

ifeq ($(MODE),debug)
MODE_CXXFLAGS += -O0 -g
else
ifeq ($(CXX_IS_CLANG),1)
MODE_CXXFLAGS += -Wshadow -O3
else
MODE_CXXFLAGS += -O4
endif
endif

ifeq ($(OPENMP),1)
ifeq ($(CXX_IS_CLANG),1)
OPENMP_CXXFLAGS += -Xpreprocessor -fopenmp
OPENMP_LDFLAGS += -lomp
ifneq ($(LIBOMP_PREFIX),)
PLATFORM_SUPPORT_CXXFLAGS += -I$(LIBOMP_PREFIX)/include
PLATFORM_SUPPORT_LDFLAGS += -L$(LIBOMP_PREFIX)/lib
endif
else
OPENMP_CXXFLAGS += -fopenmp
OPENMP_LDFLAGS += -fopenmp
endif
endif

NATIVE_CXXFLAGS := $(strip $(BASE_WARNING_FLAGS) $(MODE_CXXFLAGS) $(OPENMP_CXXFLAGS) $(PLATFORM_SUPPORT_CXXFLAGS) $(CPPFLAGS) $(CXXFLAGS))
NATIVE_LDFLAGS := $(strip $(PLATFORM_SUPPORT_LDFLAGS) $(OPENMP_LDFLAGS) $(LDFLAGS))
CMAKE_EXTRA_CXX_FLAGS := $(strip $(PLATFORM_SUPPORT_CXXFLAGS) $(CPPFLAGS) $(CXXFLAGS))
CMAKE_EXTRA_LINK_FLAGS := $(strip $(PLATFORM_SUPPORT_LDFLAGS) $(LDFLAGS))

.PHONY: build-native build-lib clean-native format-native

build-native: $(NATIVE_BINARY)
	@printf "Built %s (MODE=%s OPENMP=%s)\n" "$(NATIVE_BINARY)" "$(MODE)" "$(OPENMP)"

$(NATIVE_BINARY): $(NATIVE_OBJECTS)
	@echo "Linking object files to target $@..."
	$(CXX) $(NATIVE_LDFLAGS) -o $@ $^
	@echo "-- Link finished --"

$(NATIVE_OBJECT_DIR)/%.o: src/%.cpp $(HEADERS) $(MK_FILES) Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(NATIVE_CXXFLAGS) -c $< -o $@

build-lib: $(LIB_OUTPUT) $(LIB_HEADERS)
	@echo "Wrote static library to lib/ and headers to include/"

$(LIB_OUTPUT): $(LIB_OBJECTS) $(MK_FILES) Makefile
	@echo "Creating static library..."
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(PUBLIC_INCLUDE_DIR)/%.h: src/%.h
	@mkdir -p $(dir $@)
	@cp -a $^ $@

$(LIB_OBJECT_DIR)/%.o: src/%.cpp $(HEADERS) $(MK_FILES) Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(NATIVE_CXXFLAGS) -DNS_INFOMAP -DAS_LIB -c $< -o $@

format-native:
	clang-format -i $(HEADERS) $(SOURCES)

clean-native:
	$(RM) -r $(NATIVE_BINARY) build/native build/Infomap build/lib build/cmake build/cmake-sanitizers lib include
