# Makefile for Priority-Based sched_ext Scheduler
#
# This Makefile compiles the eBPF kernel-space scheduler and the user-space loader.

CLANG ?= clang
LLC ?= llc
STRIP ?= llvm-strip
OBJCOPY ?= llvm-objcopy
BPFTOOL ?= bpftool

VMLINUX_BTF ?= /sys/kernel/btf/vmlinux

OUTPUT := build
LIBBPF_DIR := $(OUTPUT)/libbpf
BPFDIR := $(OUTPUT)/bpf

INCLUDES := -I$(OUTPUT) -I/usr/include/bpf
CLANG_BPF_SYS_INCLUDES = $(shell $(CLANG) -v -E - </dev/null 2>&1 | sed -n '/^#include <...>/,/^End/p' | sed '/#include/d;/^End/d;s/^ *//' | awk '{print "-isystem" $0}')

VMLINUX_H = $(OUTPUT)/vmlinux.h
VMLINUX_H_OBJS =

# Compiler flags for eBPF
BPF_CFLAGS := -g -O2 -target bpf -D__KERNEL__ -D__BPF_TRACING__ -I$(OUTPUT) $(CLANG_BPF_SYS_INCLUDES)

# C flags for user-space
CFLAGS := -Wall -Wextra -O2 -g

# Directories
SRCDIR := src
BINDIR := $(OUTPUT)/bin

# Source files
BPF_SRC := $(SRCDIR)/scheduler.bpf.c
BPF_OBJ := $(OUTPUT)/scheduler.bpf.o

LOADER_SRC := $(SRCDIR)/loader.c
LOADER_BIN := $(BINDIR)/loader

# Targets
.PHONY: all clean vmlinux_btf help

all: $(VMLINUX_H) $(BPF_OBJ) $(LOADER_BIN)
	@echo "Build complete!"
	@echo "  eBPF object: $(BPF_OBJ)"
	@echo "  Loader binary: $(LOADER_BIN)"

help:
	@echo "Available targets:"
	@echo "  make all          - Build everything (default)"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make vmlinux_btf   - Generate vmlinux.h from kernel BTF"

# Generate vmlinux.h from kernel BTF
vmlinux_btf: $(VMLINUX_H)

$(VMLINUX_H):
	@mkdir -p $(OUTPUT)
	@echo "Generating vmlinux.h from kernel BTF..."
	$(BPFTOOL) btf dump file $(VMLINUX_BTF) format c > $(VMLINUX_H)
	@echo "vmlinux.h generated: $(VMLINUX_H)"

# Compile eBPF object
$(BPF_OBJ): $(BPF_SRC) $(VMLINUX_H)
	@mkdir -p $(dir $@)
	@echo "Compiling eBPF object: $@"
	$(CLANG) $(BPF_CFLAGS) -c $(BPF_SRC) -o $@
	$(STRIP) -g $@

# Create binary output directory
$(BINDIR):
	@mkdir -p $(BINDIR)

# Compile user-space loader
$(LOADER_BIN): $(LOADER_SRC) $(BINDIR)
	@echo "Compiling loader: $@"
	gcc $(CFLAGS) -o $@ $(LOADER_SRC) -I/usr/include/bpf -lbpf -lelf -lz

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(OUTPUT)
	@echo "Clean complete!"

# Phony targets to avoid file conflicts
.PHONY: vmlinux_btf all clean help
