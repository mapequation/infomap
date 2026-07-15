.PHONY: ci-export-github-env

# The MACOSX_DEPLOYMENT_TARGET below is the make-based macOS build's copy of the
# value also set in .github/workflows/_python-package.yml (env.MACOS_DEPLOYMENT_TARGET,
# consumed by the cibuildwheel jobs). Keep the two in sync when bumping.
ci-export-github-env:
	@if [ "$(UNAME_S)" = "Darwin" ]; then \
		if [ -n "$(BUILD_CONFIG_LIBOMP_PREFIX)" ]; then \
			echo "CPPFLAGS=$(strip $(CPPFLAGS) -I$(BUILD_CONFIG_LIBOMP_PREFIX)/include)"; \
			echo "LDFLAGS=$(strip $(LDFLAGS) -L$(BUILD_CONFIG_LIBOMP_PREFIX)/lib)"; \
		fi; \
		echo "MACOSX_DEPLOYMENT_TARGET=15.0"; \
		echo "CXX=/usr/bin/clang++"; \
	fi
