NATIVE_BINARY := Infomap
NATIVE_OBJECT_DIR := build/native/$(MODE)-omp$(OPENMP)
LIB_OUTPUT := lib/libInfomap.a
LIB_OBJECT_DIR := build/lib/$(MODE)-omp$(OPENMP)
PUBLIC_INCLUDE_DIR := include

NATIVE_OBJECTS := $(SOURCES:src/%.cpp=$(NATIVE_OBJECT_DIR)/%.o)
LIB_OBJECTS := $(SOURCES:src/%.cpp=$(LIB_OBJECT_DIR)/%.o)
LIB_HEADERS := $(HEADERS:src/%.h=$(PUBLIC_INCLUDE_DIR)/%.h)

.PHONY: build-native build-lib clean-native format-native

build-native: $(NATIVE_BINARY)
	@printf "Built %s (MODE=%s OPENMP=%s)\n" "$(NATIVE_BINARY)" "$(MODE)" "$(OPENMP)"

$(NATIVE_BINARY): $(NATIVE_OBJECTS)
	@echo "Linking object files to target $@..."
	$(CXX) $(NATIVE_LDFLAGS) -o $@ $^
	@echo "-- Link finished --"

$(NATIVE_OBJECT_DIR)/%.o: src/%.cpp $(HEADERS) $(MK_FILES) Makefile
	@mkdir -p $(dir $@)
	$(CXX_COMPILE) $(NATIVE_CXXFLAGS) -c $< -o $@

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
	$(CXX_COMPILE) $(NATIVE_CXXFLAGS) -DNS_INFOMAP -DAS_LIB -c $< -o $@

format-native:
	clang-format -i $(HEADERS) $(SOURCES)

clean-native:
	$(RM) -r $(NATIVE_BINARY) build/native build/Infomap build/lib build/cmake build/cmake-sanitizers lib include
