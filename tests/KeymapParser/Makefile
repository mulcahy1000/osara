# Makefile for KeymapParser

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
SRCDIR = ../src
OBJDIR = obj
BINDIR = bin

# Source files
SOURCES = $(SRCDIR)/KeymapParser.cpp $(SRCDIR)/KeymapMerger.cpp
TEST_SOURCES = test_keymap_parser.cpp test_roundtrip.cpp
HEADERS = $(SRCDIR)/KeymapParser.h $(SRCDIR)/KeymapMerger.h

# Object files
OBJECTS = $(OBJDIR)/KeymapParser.o $(OBJDIR)/KeymapMerger.o
TEST_OBJECTS = $(OBJDIR)/test_keymap_parser.o $(OBJDIR)/test_roundtrip.o
DEMO_OBJECTS = $(OBJDIR)/keymap_demo.o

# Targets
TEST_TARGET = $(BINDIR)/test_keymap_parser
ROUNDTRIP_TARGET = $(BINDIR)/test_roundtrip
DEMO_TARGET = $(BINDIR)/keymap_demo

# Default target
all: $(TEST_TARGET) $(ROUNDTRIP_TARGET)

# Create directories
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Compile KeymapParser
$(OBJDIR)/KeymapParser.o: $(SRCDIR)/KeymapParser.cpp $(SRCDIR)/KeymapParser.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile KeymapMerger
$(OBJDIR)/KeymapMerger.o: $(SRCDIR)/KeymapMerger.cpp $(SRCDIR)/KeymapMerger.h $(SRCDIR)/KeymapParser.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile test
$(OBJDIR)/test_keymap_parser.o: test_keymap_parser.cpp $(HEADERS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -c $< -o $@

# Compile roundtrip test
$(OBJDIR)/test_roundtrip.o: test_roundtrip.cpp $(HEADERS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -c $< -o $@

# Compile demo
$(OBJDIR)/keymap_demo.o: keymap_demo.cpp $(HEADERS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -c $< -o $@

# Link test executable
$(TEST_TARGET): $(OBJECTS) $(OBJDIR)/test_keymap_parser.o | $(BINDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Link roundtrip test executable
$(ROUNDTRIP_TARGET): $(OBJECTS) $(OBJDIR)/test_roundtrip.o | $(BINDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Link demo executable
$(DEMO_TARGET): $(OBJECTS) $(DEMO_OBJECTS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Run tests
test: $(TEST_TARGET) $(ROUNDTRIP_TARGET)
	@echo "Running basic KeymapParser tests..."
	./$(TEST_TARGET)
	@echo ""
	@echo "Running round-trip tests..."
	./$(ROUNDTRIP_TARGET)

# Run demo
demo: $(DEMO_TARGET)
	./$(DEMO_TARGET)

# Run only roundtrip tests
roundtrip: $(ROUNDTRIP_TARGET)
	./$(ROUNDTRIP_TARGET)

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(BINDIR)

# Install (for integration with OSARA build system)
install: all
	@echo "KeymapParser library built successfully"
	@echo "Include $(SRCDIR)/KeymapParser.h in your project"
	@echo "Include $(SRCDIR)/KeymapMerger.h for merge functionality"
	@echo "Link with $(OBJECTS)"

# Help
help:
	@echo "Available targets:"
	@echo "  all       - Build all test executables"
	@echo "  test      - Build and run all tests"
	@echo "  roundtrip - Build and run round-trip tests only"
	@echo "  demo      - Build and run demo"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Build library for integration"
	@echo "  help      - Show this help message"

.PHONY: all test demo roundtrip clean install help
