.PHONY: ci-export-github-env

ci-export-github-env:
	@if [ "$(UNAME_S)" = "Darwin" ]; then \
		if [ -n "$(LIBOMP_PREFIX)" ]; then \
			echo "CPPFLAGS=$(strip $(CPPFLAGS) -I$(LIBOMP_PREFIX)/include)"; \
			echo "LDFLAGS=$(strip $(LDFLAGS) -L$(LIBOMP_PREFIX)/lib)"; \
		fi; \
		echo "MACOSX_DEPLOYMENT_TARGET=15.0"; \
		echo "CXX=/usr/bin/clang++"; \
	fi
