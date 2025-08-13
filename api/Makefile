# Makefile for Lumitec Poco CAN API Library
#
# This Makefile can be used to build the API library as both a static
# library and a shared library, along with example programs.

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -fPIC
LDFLAGS = 

# Library settings
LIB_NAME = lumitec_poco
STATIC_LIB = lib$(LIB_NAME).a
SHARED_LIB = lib$(LIB_NAME).so
VERSION = 1.0.0

# Source files
SOURCES = lumitec_poco_api.c
HEADERS = lumitec_poco_api.h
OBJECTS = $(SOURCES:.c=.o)

# Example programs
EXAMPLES = example_usage
EXAMPLE_SOURCES = example_usage.c

# Default target
all: $(STATIC_LIB) $(SHARED_LIB) $(EXAMPLES)

# Static library
$(STATIC_LIB): $(OBJECTS)
	ar rcs $@ $^
	@echo "Static library $(STATIC_LIB) created"

# Shared library
$(SHARED_LIB): $(OBJECTS)
	$(CC) -shared -Wl,-soname,$(SHARED_LIB).$(VERSION) -o $@ $^
	@echo "Shared library $(SHARED_LIB) created"

# Object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Example programs
example_usage: example_usage.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $@ $< -L. -l$(LIB_NAME)

# Install targets (optional)
install: $(STATIC_LIB) $(SHARED_LIB)
	mkdir -p /usr/local/lib
	mkdir -p /usr/local/include
	cp $(STATIC_LIB) /usr/local/lib/
	cp $(SHARED_LIB) /usr/local/lib/
	cp $(HEADERS) /usr/local/include/
	ldconfig

# Clean targets
clean:
	rm -f *.o $(STATIC_LIB) $(SHARED_LIB) $(EXAMPLES)

distclean: clean
	rm -f *~

# Test target (runs the example)
test: example_usage
	./example_usage

# Package target
package: clean
	tar -czf lumitec_poco_api_v$(VERSION).tar.gz *.c *.h Makefile README.md

# Help target
help:
	@echo "Available targets:"
	@echo "  all        - Build static library, shared library, and examples"
	@echo "  static     - Build static library only"
	@echo "  shared     - Build shared library only"
	@echo "  examples   - Build example programs"
	@echo "  install    - Install libraries and headers to system"
	@echo "  test       - Run example program"
	@echo "  clean      - Remove build artifacts"
	@echo "  distclean  - Remove all generated files"
	@echo "  package    - Create distribution package"
	@echo "  help       - Show this help message"

# Aliases
static: $(STATIC_LIB)
shared: $(SHARED_LIB)
examples: $(EXAMPLES)

.PHONY: all clean distclean install test package help static shared examples
