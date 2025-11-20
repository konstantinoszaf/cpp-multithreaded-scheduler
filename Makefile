LOCAL_DIR := /usr/local/
BUILD_DIR := build

.PHONY: all build build-main build-profiling run-main install build-tests run-tests test clean sanity

all: build

build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && \
	    cmake -DBUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release .. && \
	    cmake --build . -- -j

build-main:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && \
	    cmake -DBUILD_TESTS=OFF -DBUILD_EXAMPLE=ON -DCMAKE_BUILD_TYPE=Release .. && \
	    cmake --build . --target main -- -j

build-profiling:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && \
	    cmake -DBUILD_TESTS=OFF -DBUILD_EXAMPLE=ON -DCMAKE_BUILD_TYPE=Profiling \
			-DCMAKE_CXX_FLAGS_PROFILING="-O3 -g -fno-omit-frame-pointer -fno-optimize-sibling-calls" .. && \
	    cmake --build . --target main -- -j

run-main:
	./$(BUILD_DIR)/main

sanity:
	valgrind --tool=helgrind --history-level=approx  --log-file=helgrind.out  ./$(BUILD_DIR)/main
	valgrind --leak-check=full --show-leak-kinds=all --log-file=leak_check.out ./$(BUILD_DIR)/main

install:
	@cd $(BUILD_DIR) && \
	    cmake --install . --prefix $(LOCAL_DIR)

build-tests:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && \
	    cmake -DBUILD_TESTS=ON -DBUILD_EXAMPLE=OFF -DCMAKE_BUILD_TYPE=Debug .. && \
	    cmake --build . -- -j

run-tests:
	@cd $(BUILD_DIR)/test && \
	    ASAN_OPTIONS="verbosity=0:detect_leaks=1:abort_on_error=1" \
	    UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1" \
	    ctest -V

test: build-tests run-tests

clean:
	@$(RM) -r -- $(BUILD_DIR)
	@$(RM) -f -- *.out
	@$(RM) -f -- perf.*
