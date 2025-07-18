LOCAL_DIR := /usr/local/
BUILD_DIR := build

.PHONY: all build install build-tests run-tests test clean

all: build

build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && \
	    cmake -DBUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release .. && \
	    cmake --build . -- -j

install:
	@cd $(BUILD_DIR) && \
	    cmake --install . --prefix $(LOCAL_DIR)

build-tests:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && \
	    cmake -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug .. && \
	    cmake --build . -- -j

run-tests:
	@cd $(BUILD_DIR)/test && \
	    ASAN_OPTIONS="verbosity=0:detect_leaks=1:abort_on_error=1" \
	    UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1" \
	    ctest -V

test: build-tests run-tests

clean:
	@rm -rf $(BUILD_DIR)
