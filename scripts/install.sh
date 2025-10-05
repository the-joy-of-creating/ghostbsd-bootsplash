#!/bin/sh
#
# GhostBSD Boot Splash Installation Script
#

set -e

# Configuration
PREFIX=${PREFIX:-/usr/local}
EFIDIR=${EFIDIR:-/boot/efi/EFI/GhostBSD}
BOOTDIR=/boot
RCDIR=${PREFIX}/etc/rc.d
SBINDIR=${PREFIX}/sbin

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info() {
    echo "${GREEN}==>${NC} $*"
}

warn() {
    echo "${YELLOW}Warning:${NC} $*"
}

error() {
    echo "${RED}Error:${NC} $*" >&2
    exit 1
}

check_root() {
    if [ "$(id -u)" -ne 0 ]; then
        error "This script must be run as root"
    fi
}

check_efi_system() {
    if [ ! -d /boot/efi/EFI ]; then
        error "EFI system partition not found. Is this a UEFI system?"
    fi
}

backup_bootloader() {
    if [ -f /boot/efi/EFI/BOOT/BOOTX64.EFI ]; then
        if [ ! -f /boot/efi/EFI/BOOT/BOOTX64.EFI.bak ]; then
            info "Backing up original bootloader..."
            cp /boot/efi/EFI/BOOT/BOOTX64.EFI /boot/efi/EFI/BOOT/BOOTX64.EFI.bak
        else
            warn "Backup already exists, skipping"
        fi
    fi
}

install_efi_splash() {
    info "Installing EFI splash application..."
    
    # Create EFI directory
    mkdir -p "${EFIDIR}"
    
    # Install EFI application
    if [ -f dist/efi/splash.efi ]; then
        cp dist/efi/splash.efi "${EFIDIR}/"
        info "Installed splash.efi to ${EFIDIR}/"
    else
        error "splash.efi not found. Run 'make' first."
    fi
    
    # Copy original bootloader to GhostBSD directory
    if [ -f /boot/efi/EFI/BOOT/BOOTX64.EFI ]; then
        cp /boot/efi/EFI/BOOT/BOOTX64.EFI "${EFIDIR}/"
        info "Copied bootloader to ${EFIDIR}/"
    fi
}

install_splash_images() {
    info "Installing splash images..."
    
    mkdir -p "${BOOTDIR}/splash"
    
    if [ -d dist/bmp ]; then
        cp dist/bmp/*.bmp "${BOOTDIR}/splash/"
        
        # Create default splash symlink
        if [ -f "${BOOTDIR}/splash/splash-1920x1080.bmp" ]; then
            ln -sf "${BOOTDIR}/splash/splash-1920x1080.bmp" "${BOOTDIR}/splash.bmp"
        fi
        
        # Also install to EFI partition
        cp dist/bmp/splash-1920x1080.bmp "${EFIDIR}/splash.bmp"
        
        info "Installed splash images to ${BOOTDIR}/splash/"
    else
        warn "No splash images found. Run 'make assets' first."
    fi
}

install_rc_scripts() {
    info "Installing RC scripts..."
    
    # Install RC service
    if [ -f dist/rc/ghostbsd_splash ]; then
        cp dist/rc/ghostbsd_splash "${RCDIR}/"
        chmod 755 "${RCDIR}/ghostbsd_splash"
        info "Installed RC script to ${RCDIR}/"
    fi
    
    # Install utilities
    if [ -f dist/scripts/ghostbsd-select-splash ]; then
        cp dist/scripts/ghostbsd-select-splash "${SBINDIR}/"
        chmod 755 "${SBINDIR}/ghostbsd-select-splash"
        info "Installed utility to ${SBINDIR}/"
    fi
}

configure_loader() {
    info "Configuring loader..."
    
    # Check if loader.conf exists
    if [ ! -f /boot/loader.conf ]; then
        touch /boot/loader.conf
    fi
    
    # Add splash configuration if not already present
    if ! grep -q "ghostbsd.*splash" /boot/loader.conf; then
        cat >> /boot/loader.conf << 'EOF'

# GhostBSD Boot Splash
kern.vty="vt"
vt_efifb_load="YES"
bitmap_load="YES"
bitmap_name="/boot/splash.bmp"
fbsplash_load="YES"
EOF
        info "Added splash configuration to /boot/loader.conf"
    else
        info "Splash configuration already present in loader.conf"
    fi
}

enable_rc_service() {
    info "Enabling RC service..."
    sysrc ghostbsd_splash_enable=YES
}

configure_boot_order() {
    info "Configuring EFI boot order..."
    
    # Replace default bootloader with splash
    backup_bootloader
    cp "${EFIDIR}/splash.efi" /boot/efi/EFI/BOOT/BOOTX64.EFI
    
    info "EFI boot configured to use splash application"
}

main() {
    info "GhostBSD Boot Splash Installer"
    echo ""
    
    check_root
    check_efi_system
    
    install_efi_splash
    install_splash_images
    install_rc_scripts
    configure_loader
    enable_rc_service
    configure_boot_order
    
    echo ""
    info "Installation complete!"
    echo ""
    echo "Next steps:"
    echo "  1. Review /boot/loader.conf"
    echo "  2. Reboot to see the splash screen"
    echo "  3. Check /var/log/boot-splash.log for diagnostics"
    echo ""
    echo "To customize the splash image:"
    echo "  - Edit assets/source/ghostbsd-logo.png"
    echo "  - Run: make assets"
    echo "  - Run: make install"
}

main "$@"
