ARCH            = x86_64
TARGET          = splash.efi

# Source files
SRCS            = src/splash.c src/bmp.c src/input.c src/error.c
OBJS            = $(SRCS:.c=.o)

# Directories
SRCDIR          = src
INCDIR          = include
BUILDDIR        = build

# GNU-EFI paths
EFIINC          = /usr/include/efi
EFIINCS         = -I$(EFIINC) -I$(EFIINC)/$(ARCH) -I$(EFIINC)/protocol -I$(INCDIR)
LIB             = /usr/lib
EFILIB          = /usr/lib
EFI_CRT_OBJS    = $(EFILIB)/crt0-efi-$(ARCH).o
EFI_LDS         = $(EFILIB)/elf_$(ARCH)_efi.lds

# Compiler flags
CFLAGS          = $(EFIINCS) -fno-stack-protector -fpic \
                  -fshort-wchar -mno-red-zone -Wall -Wextra \
                  -DEFI_FUNCTION_WRAPPER -std=c11 \
                  -DVERSION_STRING=L\"1.0.0\"

ifeq ($(ARCH),x86_64)
  CFLAGS += -DGNU_EFI_USE_MS_ABI
endif

# Linker flags
LDFLAGS         = -nostdlib -znocombreloc -T $(EFI_LDS) -shared \
                  -Bsymbolic -L $(EFILIB) -L $(LIB) $(EFI_CRT_OBJS)

# Debug build
DEBUG ?= 0
ifeq ($(DEBUG),1)
  CFLAGS += -g -DDEBUG
endif

.PHONY: all clean install debug

all: $(TARGET)

debug:
	$(MAKE) DEBUG=1

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Compile source files
%.o: %.c | $(BUILDDIR)
	@echo "CC $<"
	@gcc $(CFLAGS) -c $< -o $@

# Link object files to .so
splash.so: $(OBJS)
	@echo "LD $@"
	@ld $(LDFLAGS) $(OBJS) -o $@ -lefi -lgnuefi

# Convert .so to .efi
%.efi: %.so
	@echo "OBJCOPY $@"
	@objcopy -j .text -j .sdata -j .data -j .dynamic \
	         -j .dynsym  -j .rel -j .rela -j .reloc \
	         --target=efi-app-$(ARCH) $^ $@
	@echo ""
	@echo "Build complete: $@"
	@ls -lh $@

clean:
	@echo "Cleaning build files..."
	@rm -f $(TARGET) splash.so $(OBJS)
	@rm -f src/*.o
	@echo "Clean complete"

install: $(TARGET)
	@echo "Installing $(TARGET)..."
	@mkdir -p /boot/efi/EFI/GhostBSD
	@cp $(TARGET) /boot/efi/EFI/GhostBSD/
	@echo "Installed to /boot/efi/EFI/GhostBSD/$(TARGET)"

# Show build info
info:
	@echo "GhostBSD EFI Boot Splash"
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "Architecture: $(ARCH)"
	@echo "Target:       $(TARGET)"
	@echo "Sources:      $(SRCS)"
	@echo "Debug mode:   $(DEBUG)"
	@echo ""
	@echo "Targets:"
	@echo "  make        - Build release version"
	@echo "  make debug  - Build with debug symbols"
	@echo "  make clean  - Remove build files"
	@echo "  make install- Install to EFI partition"
	@echo "  make info   - Show this information"
