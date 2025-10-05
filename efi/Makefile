ARCH            = x86_64
OBJS            = splash.o
TARGET          = splash.efi

EFIINC          = /usr/include/efi
EFIINCS         = -I$(EFIINC) -I$(EFIINC)/$(ARCH) -I$(EFIINC)/protocol
LIB             = /usr/lib
EFILIB          = /usr/lib
EFI_CRT_OBJS    = $(EFILIB)/crt0-efi-$(ARCH).o
EFI_LDS         = $(EFILIB)/elf_$(ARCH)_efi.lds

CFLAGS          = $(EFIINCS) -fno-stack-protector -fpic \
                  -fshort-wchar -mno-red-zone -Wall \
                  -DEFI_FUNCTION_WRAPPER

ifeq ($(ARCH),x86_64)
  CFLAGS += -DGNU_EFI_USE_MS_ABI
endif

LDFLAGS         = -nostdlib -znocombreloc -T $(EFI_LDS) -shared \
                  -Bsymbolic -L $(EFILIB) -L $(LIB) $(EFI_CRT_OBJS)

all: $(TARGET)

splash.so: $(OBJS)
	ld $(LDFLAGS) $(OBJS) -o $@ -lefi -lgnuefi

%.efi: %.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym  -j .rel -j .rela -j .reloc \
	        --target=efi-app-$(ARCH) $^ $@

clean:
	rm -f $(TARGET) splash.so $(OBJS)

install: $(TARGET)
	mkdir -p /boot/efi/EFI/GhostBSD
	cp $(TARGET) /boot/efi/EFI/GhostBSD/
	cp splash.bmp /boot/efi/EFI/GhostBSD/

.PHONY: all clean install
