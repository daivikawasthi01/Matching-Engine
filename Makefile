CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -Werror -Iinclude -O3

BIN_DIR = bin
SRC = src/order_book.cpp

all: $(BIN_DIR)/matching_engine $(BIN_DIR)/test_headers $(BIN_DIR)/test_order_book $(BIN_DIR)/test_edge_cases $(BIN_DIR)/benchmark

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/matching_engine: src/main.cpp $(SRC) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BIN_DIR)/test_headers: tests/test_headers.cpp $(SRC) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BIN_DIR)/test_order_book: tests/test_order_book.cpp $(SRC) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BIN_DIR)/test_edge_cases: tests/test_edge_cases.cpp $(SRC) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BIN_DIR)/benchmark: tests/benchmark.cpp $(SRC) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

test: $(BIN_DIR)/test_headers $(BIN_DIR)/test_order_book $(BIN_DIR)/test_edge_cases
	@echo "\n=== [1/3] Running Header Checks ==="
	@./$(BIN_DIR)/test_headers
	@echo "\n=== [2/3] Running Core OrderBook Tests ==="
	@./$(BIN_DIR)/test_order_book
	@echo "\n=== [3/3] Running Edge Case Tests ==="
	@./$(BIN_DIR)/test_edge_cases
	@echo "\n>>> ALL TEST SUITES PASSED SUCCESSFULLY! <<<\n"

run-benchmark: $(BIN_DIR)/benchmark
	@./$(BIN_DIR)/benchmark

clean:
	rm -rf $(BIN_DIR) benchmark matching_engine test_order_book

.PHONY: all test run-benchmark clean
