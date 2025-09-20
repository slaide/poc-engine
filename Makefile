CC = gcc
CFLAGS = -std=c23 -Wall -Wextra -Werror -Iinclude -Ideps/podi/include
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

PODI_REPO = https://github.com/slaide/podi.git
PODI_DIR = $(DEPSDIR)/podi
PODI_LIB = $(PODI_DIR)/lib/libpodi$(shell if [ "$(UNAME_S)" = "Darwin" ]; then echo ".dylib"; else echo ".so"; fi)

# To use a local podi directory for development, set PODI_LOCAL_DIR:
# make PODI_LOCAL_DIR=/path/to/local/podi

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

EXAMPLE_SOURCES = $(wildcard $(EXAMPLEDIR)/*.c)
EXAMPLE_TARGETS = $(EXAMPLE_SOURCES:$(EXAMPLEDIR)/%.c=$(EXAMPLEDIR)/%)

SHADER_SOURCES = $(wildcard $(SHADERDIR)/*.vert $(SHADERDIR)/*.frag)
SHADER_SPIRV = $(SHADER_SOURCES:%=%.spv)

.PHONY: all clean examples podi deps run shaders

all: deps shaders examples

deps: podi

podi: $(PODI_LIB)

shaders: $(SHADER_SPIRV)

%.vert.spv: %.vert
	@echo "Compiling vertex shader $<"
	@glslangValidator -V $< -o $@

%.frag.spv: %.frag
	@echo "Compiling fragment shader $<"
	@glslangValidator -V $< -o $@

$(PODI_LIB):
	@echo "Setting up podi dependency..."
	@if [ -n "$(PODI_LOCAL_DIR)" ] && [ -d "$(PODI_LOCAL_DIR)" ]; then \
		echo "Using local podi directory: $(PODI_LOCAL_DIR)"; \
		mkdir -p $(DEPSDIR) && \
		ln -sf $(PODI_LOCAL_DIR) $(PODI_DIR); \
	elif [ ! -d "$(PODI_DIR)" ]; then \
		echo "Cloning podi from remote..."; \
		mkdir -p $(DEPSDIR) && \
		git clone $(PODI_REPO) $(PODI_DIR); \
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