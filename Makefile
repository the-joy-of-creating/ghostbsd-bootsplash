PROJECT = ghostbsd-bootsplash
VERSION = 0.0.1

PREFIX ?= /usr/local
EFIDIR = /boot/efi/EFI/GhostBSD
BOOTDIR = /boot
RCDIR = $(PREFIX)/etc/rc.d
SBINDIR = $(PREFIX)/sbin

.PHONY: all clean install uninstall package test assets efi rc

all: efi assets

# Build EFI application
efi:
	@echo "==> Building EFI splash application..."
	cd efi && $(MAKE)

# Generate splash images
assets:
	@echo "==> Generating splash images..."
	cd assets && ./generate-splash.sh

# Build everything and create distribution
dist: all
	@echo "==> Creating distribution..."
	mkdir -p dist/efi dist/bmp dist/rc dist/scripts
	cp efi/splash.efi dist/efi/
	cp assets/generated/*.bmp dist/bmp/
	cp rc/ghostbsd_splash dist/rc/
	cp rc/ghostbsd-select-splash dist/scripts/
	cp scripts/install.sh dist/
	cp README.md LICENSE dist/

# Clean build artifacts
clean:
	@echo "==> Cleaning build artifacts..."
	cd efi && $(MAKE) clean
	rm -rf dist/
	rm -f assets/generated/*.bmp

# Install everything
install: all
	@echo "==> Installing GhostBSD boot splash..."
	./scripts/install.sh

# Uninstall
uninstall:
	@echo "==> Uninstalling GhostBSD boot splash..."
	./scripts/uninstall.sh

# Run tests
test:
	@echo "==> Running tests..."
	cd tests && ./test-efi-app.sh
	cd tests && ./test-rc-script.sh
	cd tests && ./test-bmp-formats.sh

# Create package
package: dist
	@echo "==> Creating package..."
	tar -czf dist/package/$(PROJECT)-$(VERSION).tar.gz -C dist \
		efi bmp rc scripts install.sh README.md LICENSE

help:
	@echo "GhostBSD Boot Splash Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build EFI application and generate assets (default)"
	@echo "  efi        - Build EFI splash application only"
	@echo "  assets     - Generate splash images only"
	@echo "  dist       - Create distribution directory"
	@echo "  install    - Install to system"
	@echo "  uninstall  - Remove from system"
	@echo "  test       - Run test suite"
	@echo "  package    - Create distributable package"
	@echo "  clean      - Remove build artifacts"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Variables:"
	@echo "  PREFIX     - Installation prefix (default: /usr/local)"
	@echo "  EFIDIR     - EFI directory (default: /boot/efi/EFI/GhostBSD)"
