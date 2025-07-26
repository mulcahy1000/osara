# Makefile for KeymapParser

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
SRCDIR = ../src
OBJDIR = obj
BINDIR = bin

# Source files
SOURCES = $(SRCDIR)/KeymapParser.cpp
TEST_SOURCES = test_keymap_parser.cpp
HEADERS = $(SRCDIR)/KeymapParser.h

# Object files
OBJECTS = $(OBJDIR)/KeymapParser.o
TEST_OBJECTS = $(OBJDIR)/test_keymap_parser.o
DEMO_OBJECTS = $(OBJDIR)/keymap_demo.o

# Targets
TEST_TARGET = $(BINDIR)/test_keymap_parser
DEMO_TARGET = $(BINDIR)/keymap_demo

# Default target
all: $(TEST_TARGET)

# Create directories
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Compile KeymapParser
$(OBJDIR)/KeymapParser.o: $(SRCDIR)/KeymapParser.cpp $(HEADERS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile test
$(OBJDIR)/test_keymap_parser.o: $(SRCDIR)/test_keymap_parser.cpp $(HEADERS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile demo
$(OBJDIR)/keymap_demo.o: keymap_demo.cpp $(HEADERS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link test executable
$(TEST_TARGET): $(OBJECTS) $(TEST_OBJECTS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Link demo executable
$(DEMO_TARGET): $(OBJECTS) $(DEMO_OBJECTS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Run tests
test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Run demo
demo: $(DEMO_TARGET)
	./$(DEMO_TARGET)

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(BINDIR)

# Install (for integration with OSARA build system)
install: all
	@echo "KeymapParser library built successfully"
	@echo "Include $(SRCDIR)/KeymapParser.h in your project"
	@echo "Link with $(OBJECTS)"

# Help
help:
	@echo "Available targets:"
	@echo "  all     - Build the test executable"
	@echo "  test    - Build and run tests"
	@echo "  demo    - Build and run demo"
	@echo "  clean   - Remove build artifacts"
	@echo "  install - Build library for integration"
	@echo "  help    - Show this help message"

.PHONY: all test demo clean install help
