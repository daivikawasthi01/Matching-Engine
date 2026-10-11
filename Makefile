# Convenience wrapper around CMake. CMake is the single source of truth for the build.
#
#   make test           build (Release) and run all tests
#   make sanitize       build with AddressSanitizer + UBSan and run all tests
#   make run-benchmark  build (Release) and run the benchmark
#   make demo           run the demo walkthrough
#   make clean

BUILD_DIR ?= build
SAN_DIR   ?= build-sanitize
WERROR    ?= OFF
JOBS      ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)

all:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -DMATCHING_ENGINE_WERROR=$(WERROR)
	cmake --build $(BUILD_DIR) -j $(JOBS)

test: all
	cd $(BUILD_DIR) && ctest --output-on-failure -j $(JOBS)

sanitize:
	cmake -S . -B $(SAN_DIR) -DCMAKE_BUILD_TYPE=Debug -DMATCHING_ENGINE_SANITIZE=ON \
		-DMATCHING_ENGINE_BUILD_BENCHMARKS=OFF -DMATCHING_ENGINE_WERROR=$(WERROR)
	cmake --build $(SAN_DIR) -j $(JOBS)
	cd $(SAN_DIR) && ctest --output-on-failure -j $(JOBS)

run-benchmark: all
	./$(BUILD_DIR)/matching_benchmark

demo: all
	./$(BUILD_DIR)/matching_engine

clean:
	rm -rf $(BUILD_DIR) $(SAN_DIR)

.PHONY: all test sanitize run-benchmark demo clean
