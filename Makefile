CC = gcc
CFLAGS = -std=c23 -Wall -Wextra -Werror -Iinclude -Ideps/podi/include -Ideps/cglm/include
LDFLAGS =

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_M),x86_64)
    ARCH = x64
else ifeq ($(UNAME_M),aarch64)
    ARCH = arm64
else ifeq ($(UNAME_M),arm64)
    ARCH = arm64
else
    $(error Unsupported architecture: $(UNAME_M))
endif

ifeq ($(UNAME_S),Linux)
    PLATFORM_LIBS = -lX11 -lwayland-client -lxkbcommon -lvulkan -ldl -lm
    CFLAGS += -DPOC_PLATFORM_LINUX
    ifeq ($(ARCH),x64)
        CFLAGS += -DPOC_ARCH_X64
    else ifeq ($(ARCH),arm64)
        CFLAGS += -DPOC_ARCH_ARM64
    endif
else ifeq ($(UNAME_S),Darwin)
    PLATFORM_LIBS = -framework Cocoa -framework Metal -framework MetalKit
    CFLAGS += -fobjc-arc -DPOC_PLATFORM_MACOS
    ifeq ($(ARCH),arm64)
        CFLAGS += -DPOC_ARCH_ARM64 -arch arm64
        LDFLAGS += -arch arm64
    else
        $(error macOS x64 not supported - only arm64 macOS is supported)
    endif
else
    $(error Unsupported platform: $(UNAME_S))
endif

SRCDIR = src
OBJDIR = obj
EXAMPLEDIR = examples
DEPSDIR = deps
SHADERDIR = shaders

PODI_DIR = $(DEPSDIR)/podi
PODI_LIB = $(PODI_DIR)/lib/libpodi$(shell if [ "$(UNAME_S)" = "Darwin" ]; then echo ".dylib"; else echo ".so"; fi)

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

EXAMPLE_SOURCES = $(wildcard $(EXAMPLEDIR)/*.c)
EXAMPLE_TARGETS = $(EXAMPLE_SOURCES:$(EXAMPLEDIR)/%.c=$(EXAMPLEDIR)/%)

SHADER_SOURCES = $(wildcard $(SHADERDIR)/*.vert $(SHADERDIR)/*.frag)
SHADER_SPIRV = $(SHADER_SOURCES:%=%.spv)

.PHONY: all clean examples podi deps run shaders submodules

all: deps shaders examples

deps: submodules podi

submodules:
	@echo "Updating git submodules..."
	@git submodule update --init --recursive

podi: $(PODI_LIB)

shaders: $(SHADER_SPIRV)

%.vert.spv: %.vert
	@echo "Compiling vertex shader $<"
	@glslangValidator -V $< -o $@

%.frag.spv: %.frag
	@echo "Compiling fragment shader $<"
	@glslangValidator -V $< -o $@

$(PODI_LIB):
	@echo "Building podi dependency..."
	@if [ ! -d "$(PODI_DIR)" ]; then \
		echo "Error: podi submodule not found. Run 'git submodule update --init --recursive'"; \
		exit 1; \
	fi
	@cd $(PODI_DIR) && $(MAKE)

examples: $(EXAMPLE_TARGETS)

$(EXAMPLEDIR)/%: $(EXAMPLEDIR)/%.c $(OBJECTS) $(PODI_LIB) | $(OBJDIR)
	$(CC) $(CFLAGS) $< $(OBJECTS) -L$(PODI_DIR)/lib -lpodi $(PLATFORM_LIBS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR)
	rm -f $(EXAMPLE_TARGETS)
	rm -f $(SHADER_SPIRV)

clean-all: clean
	rm -rf $(DEPSDIR)

debug: CFLAGS += -g -DDEBUG
debug: all

release: CFLAGS += -O3 -DNDEBUG
release: all

run: examples/basic
	@echo "Running POC Engine basic example..."
	@LD_LIBRARY_PATH=$(PODI_DIR)/lib:$$LD_LIBRARY_PATH ./examples/basic

.SECONDARY: $(OBJECTS)