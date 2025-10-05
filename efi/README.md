# GhostBSD EFI Boot Splash

Pre-kernel UEFI splash screen application for GhostBSD that displays a boot splash before the kernel initializes.

## Overview

This is a standalone UEFI application that:
- Runs **before** the FreeBSD/GhostBSD kernel and bootloader
- Displays a BMP splash image using EFI Graphics Output Protocol (GOP)
- Provides interruptible timeout (skip with any key press)
- Chainloads the FreeBSD bootloader seamlessly
- Includes debug mode for troubleshooting
- Gracefully degrades if splash display fails

## Architecture

```
Power On → UEFI Firmware → splash.efi → Display BMP → Chainload BOOTX64.EFI → Kernel
                                ↓
                          (2 second delay)
                                ↓
                          (Press any key to skip)
```

## Requirements

### Build Dependencies

**FreeBSD/GhostBSD:**
```bash
pkg install gnu-efi
```

**Debian/Ubuntu (for cross-compilation):**
```bash
apt install gnu-efi build-essential
```

### Runtime Requirements

- UEFI-based system (not BIOS/Legacy boot)
- EFI Graphics Output Protocol (GOP) support
- At least 64MB of free memory
- EFI System Partition (ESP) mounted

## File Structure

```
efi/
├── Makefile                  # Build system
├── README.md                 # This file
│
├── include/                  # Header files
│   ├── bmp.h                # BMP image handling
│   ├── input.h              # Keyboard input
│   └── error.h              # Error handling
│
└── src/                      # Source files
    ├── splash.c             # Main application (efi_main)
    ├── bmp.c                # BMP loading and display
    ├── input.c              # Input handling with timeout
    └── error.c              # Error messages and debugging
```

## Building

### Quick Build

```bash
cd efi/
make
```

### Build with Debug Symbols

```bash
make debug
```

### Build Targets

| Target | Description |
|--------|-------------|
| `make` | Build release version |
| `make debug` | Build with debug symbols |
| `make clean` | Remove build artifacts |
| `make install` | Install to EFI partition |
| `make info` | Show build information |

### Build Output

Successful build produces:
- `splash.efi` - The bootable UEFI application (~50-100KB)

### Verify Build

```bash
file splash.efi
# Expected: PE32+ executable (EFI application) x86-64
```

## Installation

### Automated Installation

```bash
sudo make install
```

This installs to `/boot/efi/EFI/GhostBSD/splash.efi`

### Manual Installation

```bash
# 1. Mount EFI partition (if not already mounted)
sudo mount -t msdosfs /dev/ada0p1 /boot/efi

# 2. Create directory
sudo mkdir -p /boot/efi/EFI/GhostBSD

# 3. Backup original bootloader
sudo cp /boot/efi/EFI/BOOT/BOOTX64.EFI \
        /boot/efi/EFI/BOOT/BOOTX64.EFI.backup

# 4. Copy original bootloader to GhostBSD directory
sudo cp /boot/efi/EFI/BOOT/BOOTX64.EFI \
        /boot/efi/EFI/GhostBSD/

# 5. Install splash application
sudo cp splash.efi /boot/efi/EFI/GhostBSD/

# 6. Copy splash image
sudo cp ../assets/generated/splash-1920x1080.bmp \
        /boot/efi/EFI/GhostBSD/splash.bmp

# 7. Replace bootloader with splash
sudo cp /boot/efi/EFI/GhostBSD/splash.efi \
        /boot/efi/EFI/BOOT/BOOTX64.EFI
```

### Alternative: Custom Boot Entry

Instead of replacing the default bootloader, create a custom boot entry:

```bash
efibootmgr -c -l '\EFI\GhostBSD\splash.efi' -L "GhostBSD Splash"
```

## Configuration

### File Locations

The application expects files at these locations on the EFI System Partition:

| File | Path | Purpose |
|------|------|---------|
| Splash image | `/EFI/GhostBSD/splash.bmp` | 24-bit BMP splash screen |
| Bootloader | `/EFI/GhostBSD/BOOTX64.EFI` | Original FreeBSD bootloader |

### Splash Image Requirements

- **Format**: BMP v3, 24-bit, uncompressed
- **Dimensions**: Match your screen resolution (e.g., 1920×1080)
- **No alpha channel**
- **File size**: Typically 5-20MB depending on resolution

### Compile-Time Configuration

Edit `src/splash.c` to customize:

```c
#define SPLASH_TIMEOUT_MS 2000              // Splash duration (ms)
#define BOOTLOADER_PATH L"\\EFI\\BOOT\\BOOTX64.EFI"
#define SPLASH_IMAGE_PATH L"\\EFI\\GhostBSD\\splash.bmp"
```

### Bootloader Fallback Chain

The application tries multiple bootloader paths in order:

1. `\EFI\BOOT\BOOTX64.EFI`
2. `\EFI\FreeBSD\loader.efi`
3. `\EFI\GhostBSD\BOOTX64.EFI`

## Usage

### Normal Boot

1. Power on system
2. UEFI loads `splash.efi`
3. Splash displays for 2 seconds
4. Bootloader chainloads automatically
5. System continues normal boot

### Skip Splash

Press **any key** during the 2-second timeout to boot immediately.

### Debug Mode

Hold **F8** during boot to enable debug output:

```
GhostBSD Splash v0.0.1 - Debug Mode
════════════════════════════════════════════════════

  GhostBSD Boot Splash
  ════════════════════════════════════════════════════
  Resolution: 1920x1080
  Pixel Format: 1
  Mode: 0 of 3
  ════════════════════════════════════════════════════

Debug: Press any key to boot immediately, or wait 2 seconds
  Trying bootloader...
  Path: \EFI\BOOT\BOOTX64.EFI
```

## Testing

### QEMU Testing (Recommended)

```bash
# Create test disk
dd if=/dev/zero of=test.img bs=1M count=512
mkfs.vfat test.img

# Mount and setup
mkdir -p mnt
sudo mount -o loop test.img mnt
sudo mkdir -p mnt/EFI/{BOOT,GhostBSD}

# Copy files
sudo cp splash.efi mnt/EFI/BOOT/BOOTX64.EFI
sudo cp /boot/efi/EFI/BOOT/BOOTX64.EFI mnt/EFI/GhostBSD/
sudo cp ../assets/generated/splash-1920x1080.bmp mnt/EFI/GhostBSD/splash.bmp

sudo umount mnt

# Test
qemu-system-x86_64 \
    -bios /usr/share/ovmf/OVMF.fd \
    -drive file=test.img,format=raw \
    -m 2048 \
    -serial stdio
```

### Virtual Machine Testing

Test in VirtualBox or VMware:
1. Enable EFI mode in VM settings
2. Install splash.efi as described above
3. Boot and verify splash appears

### Real Hardware Testing

**⚠️ Always backup first!**

```bash
# Backup
sudo cp /boot/efi/EFI/BOOT/BOOTX64.EFI \
        /boot/efi/EFI/BOOT/BOOTX64.EFI.backup

# Install
sudo make install

# Test
sudo reboot
```

## Troubleshooting

### Black Screen, No Boot

**Cause**: Splash or bootloader file missing/corrupt

**Solution**: Boot from USB and restore backup:
```bash
mount /dev/ada0p1 /mnt
cp /mnt/EFI/BOOT/BOOTX64.EFI.backup /mnt/EFI/BOOT/BOOTX64.EFI
umount /mnt
```

### No Splash, Boots Normally

**Causes**:
- BMP file not found
- GOP not available
- Invalid BMP format

**Debug**: Enable debug mode (hold F8) to see error messages

**Check**:
```bash
ls -lh /boot/efi/EFI/GhostBSD/splash.bmp
file /boot/efi/EFI/GhostBSD/splash.bmp
# Should show: PC bitmap, Windows 3.x format
```

### Hangs on Splash

**Cause**: Bootloader path incorrect or bootloader corrupt

**Debug**: Use F8 debug mode to see which path is failing

**Fix**: Verify bootloader exists:
```bash
ls -lh /boot/efi/EFI/GhostBSD/BOOTX64.EFI
```

### Compilation Errors

**Error**: `undefined reference to 'gEfiGraphicsOutputProtocolGuid'`

**Solution**: Install gnu-efi package:
```bash
pkg install gnu-efi
```

**Error**: `fatal error: efi.h: No such file or directory`

**Solution**: GNU-EFI headers not found. Check installation:
```bash
ls /usr/include/efi/
```

### Slow Splash Display

**Symptom**: Takes 5+ seconds to display splash

**Cause**: Using pixel-by-pixel rendering instead of buffer blitting

**Solution**: Ensure you're using the optimized `bmp.c` with buffer blitting

## Performance

### Display Times (1920×1080 BMP)

| Method | Time |
|--------|------|
| Pixel-by-pixel | 8-12 seconds |
| Row-by-row | 1-3 seconds |
| **Buffer blit (current)** | **0.1-0.3 seconds** |

### Memory Usage

- Code: ~100KB
- BMP Buffer: Width × Height × 4 bytes
- Example (1920×1080): ~8MB peak

## Technical Details

### EFI Boot Services Used

- `LocateProtocol` - Find GOP and filesystem
- `HandleProtocol` - Access loaded image and filesystem
- `LoadImage` - Load bootloader
- `StartImage` - Execute bootloader
- `CreateEvent` / `SetTimer` - Timeout implementation

### Graphics Output Protocol

- Mode detection and querying
- `Blt` operations:
  - `EfiBltVideoFill` - Clear screen
  - `EfiBltBufferToVideo` - Fast image display

### Error Handling

- Graceful degradation (boot continues if splash fails)
- Multiple bootloader fallback paths
- User-friendly error messages in debug mode
- Non-fatal errors don't prevent boot

## Development

### Adding New Features

1. Edit source files in `src/`
2. Update headers in `include/` if needed
3. Rebuild: `make clean && make`
4. Test in QEMU before real hardware

### Code Style

- Follow FreeBSD style guidelines
- Use UEFI data types (UINT32, CHAR16, etc.)
- Wrap all EFI calls with `uefi_call_wrapper()`
- Free all allocated memory
- Check all return statuses

### Debugging

**Printf debugging:**
```c
Print(L"Debug: value = %d\n", someValue);
```

**Serial output** (with QEMU `-serial stdio`):
```c
Print(L"Boot stage: %s\n", L"Loading BMP");
```

## Security Considerations

### Secure Boot

To sign for Secure Boot:

```bash
# Generate keys (once)
openssl req -new -x509 -newkey rsa:2048 \
    -keyout DB.key -out DB.crt \
    -days 3650 -nodes -sha256

# Sign binary
sbsign --key DB.key --cert DB.crt \
    --output splash.efi.signed splash.efi
```

### Input Validation

- BMP files are validated before display
- File sizes checked for sanity
- Paths sanitized
- Buffer overflows prevented

## License

See main project LICENSE file.

## Contributing

1. Test thoroughly in QEMU
2. Follow existing code style
3. Add error handling for new features
4. Update this README for new features
5. Submit pull request with detailed description

## Support

For issues and questions:
- GitHub Issues: https://github.com/the-joy-of-creating/ghostbsd-bootsplash/issues
- Check troubleshooting section above
- Enable debug mode (F8) for diagnostics

## Version History

**v0.0.1** (Current)
- Initial release
- BMP splash display
- Keyboard interrupt support
- Multiple bootloader fallback
- Debug mode
- Optimized buffer blitting

## See Also

- [Main Project README](../README.md)
- [Asset Generation Guide](../assets/README.md)
- [Installation Guide](../docs/INSTALLATION.md)
- [UEFI Specification](https://uefi.org/specifications)
- [GNU-EFI Documentation](https://sourceforge.net/projects/gnu-efi/)
