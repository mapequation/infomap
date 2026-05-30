NATIVE_BINARY := Infomap
NATIVE_OBJECT_DIR := build/native/$(MODE)-omp$(OPENMP)-arch$(NATIVE_ARCH)-features-$(FEATURE_CACHE_KEY)
LIB_OUTPUT := lib/libInfomap.a
LIB_OBJECT_DIR := build/lib/$(MODE)-omp$(OPENMP)-arch$(NATIVE_ARCH)-features-$(FEATURE_CACHE_KEY)
PUBLIC_INCLUDE_DIR := include

NATIVE_OBJECTS := $(SOURCES:src/%.cpp=$(NATIVE_OBJECT_DIR)/%.o)
LIB_OBJECTS := $(SOURCES:src/%.cpp=$(LIB_OBJECT_DIR)/%.o)
LIB_HEADERS := $(HEADERS:src/%.h=$(PUBLIC_INCLUDE_DIR)/%.h)

# On clang/gnu, let the compiler emit a per-object .d file recording the headers
# each translation unit actually includes (-MMD), with phony targets for headers
# so a deleted/renamed header doesn't break the build (-MP). These deps replace
# the blanket "every object depends on every header" prerequisite below, so an
# edit to one header only rebuilds the TUs that include it. Other toolchains
# (e.g. MSVC) keep the conservative $(HEADERS) prerequisite via NATIVE_HEADER_DEP.
NATIVE_DEP_TRACKING := $(filter clang gnu,$(BUILD_CONFIG_COMPILER_FAMILY))
DEP_FLAGS = $(if $(NATIVE_DEP_TRACKING),-MMD -MP -MF $(@:.o=.d))
NATIVE_HEADER_DEP := $(if $(NATIVE_DEP_TRACKING),,$(HEADERS))
NATIVE_DEPS := $(NATIVE_OBJECTS:.o=.d)
LIB_DEPS := $(LIB_OBJECTS:.o=.d)

.PHONY: build-native build-lib clean-native format-native format-native-check

build-native: $(NATIVE_BINARY)
	@printf "Built %s (MODE=%s OPENMP=%s FEATURES=%s)\n" "$(NATIVE_BINARY)" "$(MODE)" "$(OPENMP)" "$(if $(BUILD_CONFIG_ENABLED_FEATURES),$(BUILD_CONFIG_ENABLED_FEATURES),none)"

$(NATIVE_BINARY): $(NATIVE_OBJECTS)
	@echo "Linking object files to target $@..."
	$(CXX) $(NATIVE_LDFLAGS) -o $@ $^
	@echo "-- Link finished --"

$(NATIVE_OBJECT_DIR)/%.o: src/%.cpp $(NATIVE_HEADER_DEP) $(MK_FILES) Makefile
	@mkdir -p $(dir $@)
	$(CXX_COMPILE) $(NATIVE_CXXFLAGS) $(DEP_FLAGS) -c $< -o $@

build-lib: $(LIB_OUTPUT) $(LIB_HEADERS)
	@echo "Wrote static library to lib/ and headers to include/"

$(LIB_OUTPUT): $(LIB_OBJECTS) $(MK_FILES) Makefile
	@echo "Creating static library..."
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(PUBLIC_INCLUDE_DIR)/%.h: src/%.h
	@mkdir -p $(dir $@)
	@cp -a $^ $@

$(LIB_OBJECT_DIR)/%.o: src/%.cpp $(NATIVE_HEADER_DEP) $(MK_FILES) Makefile
	@mkdir -p $(dir $@)
	$(CXX_COMPILE) $(NATIVE_CXXFLAGS) $(DEP_FLAGS) -DNS_INFOMAP -DAS_LIB -c $< -o $@

format-native:
	$(CLANG_FORMAT) -i $(HEADERS) $(SOURCES)

format-native-check:
	$(CLANG_FORMAT) --dry-run --Werror $(HEADERS) $(SOURCES)

clean-native:
	$(RM) -r $(NATIVE_BINARY) build/native build/Infomap build/lib build/cmake build/cmake-sanitizers build/build_config.generated.mk lib include

# Pull in the auto-generated per-translation-unit header dependencies (no-op on
# a fresh tree; populated after the first compile). Guarded so `clean` targets
# don't regenerate them.
ifeq ($(filter clean clean-native,$(MAKECMDGOALS)),)
-include $(NATIVE_DEPS)
-include $(LIB_DEPS)
endif
