# =============================================================================
# Makefile for Modern C++ Risk Engine Project
# =============================================================================

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic
INCLUDES = -I.

# Optimization flags (without parallel execution for compatibility)
RELEASE_FLAGS = -O3 -march=native -DNDEBUG
DEBUG_FLAGS = -g -O0 -DDEBUG

# Target executable
TARGET = risk_engine
TARGET_DEBUG = risk_engine_debug

# Source files (only .cpp files, headers are included automatically)
SOURCES = main.cpp

# Header files (for dependency tracking)
HEADERS = types.hpp \
          math_utils.hpp \
          curve_builder.hpp \
          pricing_models.hpp \
          monte_carlo.hpp \
          portfolio_calculator.hpp

# Object files (generated from sources)
OBJECTS = $(SOURCES:.cpp=.o)
OBJECTS_DEBUG = $(SOURCES:.cpp=_debug.o)

# Default target (release build)
.DEFAULT_GOAL := release

# =============================================================================
# Main Targets
# =============================================================================

# Release build (optimized)
release: CXXFLAGS += $(RELEASE_FLAGS)
release: $(TARGET)

# Debug build (with debugging symbols and sanitizers)
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: $(TARGET_DEBUG)

# Build both release and debug
all: release debug

# =============================================================================
# Build Rules
# =============================================================================

# Release executable
$(TARGET): $(SOURCES) $(HEADERS)
	@echo "🚀 Building release version..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $(TARGET)
	@echo "✅ Release build complete: $(TARGET)"

# Debug executable  
$(TARGET_DEBUG): $(SOURCES) $(HEADERS)
	@echo "🐛 Building debug version..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $(TARGET_DEBUG)
	@echo "✅ Debug build complete: $(TARGET_DEBUG)"

# =============================================================================
# Utility Targets
# =============================================================================

# Run the release version
run: release
	@echo "🏃 Running risk engine..."
	./$(TARGET)

# Run the debug version
run-debug: debug
	@echo "🐛 Running debug version..."
	./$(TARGET_DEBUG)

# Performance test with optimizations
perf: release
	@echo "⚡ Performance test..."
	time ./$(TARGET)

# Memory check with valgrind (if available)
memcheck: debug
	@echo "🔍 Memory check..."
	@if command -v valgrind >/dev/null 2>&1; then \
		valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET_DEBUG); \
	else \
		echo "❌ Valgrind not found. Install with: sudo apt install valgrind"; \
	fi

# Static analysis with cppcheck (if available)
analyze:
	@echo "🔍 Static analysis..."
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --enable=all --std=c++20 --suppress=missingInclude $(SOURCES) $(HEADERS); \
	else \
		echo "❌ cppcheck not found. Install with: sudo apt install cppcheck"; \
	fi

# Clean build artifacts
clean:
	@echo "🧹 Cleaning build artifacts..."
	rm -f $(TARGET) $(TARGET_DEBUG) $(OBJECTS) $(OBJECTS_DEBUG)
	rm -f *.o *_debug.o
	rm -f core core.*
	@echo "✅ Clean complete"

# Clean and rebuild everything
rebuild: clean all

# =============================================================================
# Development Helpers
# =============================================================================

# Format code (requires clang-format)
format:
	@echo "✨ Formatting code..."
	@if command -v clang-format >/dev/null 2>&1; then \
		clang-format -i $(SOURCES) $(HEADERS); \
		echo "✅ Code formatted"; \
	else \
		echo "❌ clang-format not found. Install with: sudo apt install clang-format"; \
	fi

# Check for C++20 compiler support
check-compiler:
	@echo "🔍 Checking C++20 support..."
	@$(CXX) --version
	@echo "Testing C++20 features..."
	@echo '#include <concepts>\n#include <ranges>\nint main(){return 0;}' | $(CXX) -std=c++20 -x c++ - -o /tmp/cpp20_test 2>/dev/null && echo "✅ C++20 supported" || echo "❌ C++20 not supported"
	@rm -f /tmp/cpp20_test

# Show help
help:
	@echo "🛠️  Risk Engine Makefile Help"
	@echo "================================="
	@echo ""
	@echo "Build Targets:"
	@echo "  release     - Build optimized version (default)"
	@echo "  debug       - Build debug version with sanitizers"
	@echo "  all         - Build both release and debug"
	@echo ""
	@echo "Run Targets:"
	@echo "  run         - Build and run release version"
	@echo "  run-debug   - Build and run debug version"
	@echo "  perf        - Run performance test"
	@echo ""
	@echo "Analysis:"
	@echo "  memcheck    - Run with valgrind memory checker"
	@echo "  analyze     - Static code analysis with cppcheck"
	@echo ""
	@echo "Maintenance:"
	@echo "  clean       - Remove build artifacts"
	@echo "  rebuild     - Clean and rebuild everything"
	@echo "  format      - Format code with clang-format"
	@echo ""
	@echo "Utilities:"
	@echo "  check-compiler - Check C++20 compiler support"
	@echo "  help           - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make              # Build release version"
	@echo "  make run          # Build and run"
	@echo "  make debug run-debug  # Build and run debug"
	@echo "  make clean rebuild   # Fresh build"

# =============================================================================
# Install Target (optional)
# =============================================================================

# Install prefix
PREFIX ?= /usr/local

install: release
	@echo "📦 Installing $(TARGET)..."
	install -d $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/
	@echo "✅ Installed to $(PREFIX)/bin/$(TARGET)"

uninstall:
	@echo "🗑️  Uninstalling $(TARGET)..."
	rm -f $(PREFIX)/bin/$(TARGET)
	@echo "✅ Uninstalled"

# =============================================================================
# Special Targets
# =============================================================================

# Declare phony targets (targets that don't create files)
.PHONY: release debug all run run-debug perf memcheck analyze clean rebuild format check-compiler help install uninstall

# Keep intermediate files
.PRECIOUS: $(OBJECTS) $(OBJECTS_DEBUG)

# =============================================================================
# Advanced Build Configurations
# =============================================================================

# Profile-guided optimization (advanced)
pgo-generate: CXXFLAGS += -O3 -fprofile-generate
pgo-generate: $(TARGET)_pgo_gen

$(TARGET)_pgo_gen: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $(TARGET)_pgo_gen

pgo-use: pgo-generate
	@echo "🎯 Running PGO training..."
	./$(TARGET)_pgo_gen
	@echo "🎯 Building PGO optimized version..."
	$(CXX) -std=c++20 -O3 -fprofile-use $(INCLUDES) $(SOURCES) -o $(TARGET)_pgo
	rm -f $(TARGET)_pgo_gen *.gcda
	@echo "✅ PGO build complete: $(TARGET)_pgo"

# Compiler-specific optimizations
gcc-native: CXX = g++
gcc-native: CXXFLAGS += -O3 -march=native -mtune=native -flto
gcc-native: $(TARGET)

clang-native: CXX = clang++
clang-native: CXXFLAGS += -O3 -march=native -flto
clang-native: $(TARGET)