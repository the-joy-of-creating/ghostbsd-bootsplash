# GhostBSD BootSplash (Ideation phase)

## 1. Purpose

We’re building a **boot splash** that:

* Displays an **EFI GOP or VBE splash** before kernel init.
* Hands over to `fbsplash.ko` during early kernel startup (via `vt(4)`).
* **Unloads** once the console is ready (clean transition, no text bleed).
* Uses GhostBSD branding.

---

## 2. Dependencies

| Component                      | Purpose                                        |
| ------------------------------ | ---------------------------------------------- |
| `vt(4)`                        | The framebuffer console used by GhostBSD.      |
| `vt_efifb.ko` or `vt_vbefb.ko` | The actual framebuffer backend (UEFI or BIOS). |
| `bitmap.ko`                    | Loads splash images.                           |
| `fbsplash.ko`                  | Displays framebuffer splash on boot.           |
| `loader.conf`                  | Controls module loading order.                 |
| `/boot/splash.bmp`             | The splash image (24-bit uncompressed BMP).    |

---

## 3. Loader Configuration

### For **UEFI (GOP / efifb)**

```conf
kern.vty="vt"
vt_efifb_load="YES"
bitmap_load="YES"
bitmap_name="/boot/splash.bmp"
fbsplash_load="YES"
```

### For **Legacy BIOS (VBE / vbefb)**

```conf
kern.vty="vt"
vt_vbefb_load="YES"
bitmap_load="YES"
bitmap_name="/boot/splash.bmp"
fbsplash_load="YES"
```

---

## 4. Create the Boot Splash Image

### Requirements:

* Format: **BMP v3**, 24-bit, uncompressed
* Size: **match your framebuffer resolution** (e.g. 1920×1080)
* No alpha channel

Find your framebuffer resolution:

```sh
dmesg | grep -E 'vt\((efifb|vbefb|drmfb)\).*([0-9]{3,4}x[0-9]{3,4})'
```

### Example: Simple centered GhostBSD logo

```sh
convert /boot/images/ghostbsd-logo.png \
  -background "#0b1220" -gravity center -extent 1920x1080 \
  -type TrueColor -define bmp:format=bmp3 \
  /boot/splash.bmp
```

Generate multiple resolutions (optional):

```sh
for r in 1024x768 1280x800 1366x768 1600x900 1920x1080; do
  convert /boot/images/ghostbsd-logo.png \
    -background "#0b1220" -gravity center -extent "$r" \
    -type TrueColor -define bmp:format=bmp3 \
    "/boot/splash-${r}.bmp"
done
```

---

## 5. Auto-Unload Script (`/usr/local/etc/rc.d/ghostbsd_splash`)

```sh
#!/bin/sh
#
# PROVIDE: ghostbsd_splash
# REQUIRE: FILESYSTEMS
# BEFORE:  LOGIN
# KEYWORD: nojail

. /etc/rc.subr
name="ghostbsd_splash"
rcvar="${name}_enable"
start_cmd="${name}_start"
stop_cmd=":"

: ${ghostbsd_splash_delay:=1}
: ${ghostbsd_splash_log:=/var/log/boot-splash.log}

ghostbsd_splash_start()
{
    mkdir -p "$(dirname ${ghostbsd_splash_log})"
    {
        echo "=== ghostbsd_splash $(date '+%Y-%m-%d %H:%M:%S') ==="
        dmesg | grep -E 'vt\((efifb|vbefb|drmfb)\).*([0-9]{3,4}x[0-9]{3,4})' || true
        kldstat | egrep 'fbsplash|bitmap|vt_(efi|vbe)fb' || true
        sysctl -n hw.consoles 2>/dev/null || true
        echo
    } >> "${ghostbsd_splash_log}" 2>&1

    sleep "${ghostbsd_splash_delay}" 2>/dev/null
    if kldstat -n fbsplash >/dev/null 2>&1; then
        echo "Unloading fbsplash.ko..."
        kldunload fbsplash || echo "Warning: could not unload fbsplash" >&2
    fi
    if kldstat -n bitmap >/dev/null 2>&1; then
        kldunload bitmap >/dev/null 2>&1 || true
    fi
}

load_rc_config $name
: ${ghostbsd_splash_enable:=YES}
run_rc_command "$1"
```

Enable and test:

```sh
sudo chmod +x /usr/local/etc/rc.d/ghostbsd_splash
sudo sysrc ghostbsd_splash_enable=YES
sudo service ghostbsd_splash start
cat /var/log/boot-splash.log
```

---

## 6. Optional: Auto-Select the Best Splash Size

```sh
#!/bin/sh
# /usr/local/sbin/ghostbsd-select-splash
LOG="/var/log/boot-splash.log"
FB=$(awk '/vt\((efifb|vbefb|drmfb)\).*([0-9]{3,4}x[0-9]{3,4})/ {
  match($0, /([0-9]{3,4}x[0-9]{3,4})/, a); print a[1]; exit }' "$LOG" 2>/dev/null)

[ -n "$FB" ] && [ -f "/boot/splash-${FB}.bmp" ] && cp -f "/boot/splash-${FB}.bmp" /boot/splash.bmp
```

Run it once after the first boot:

```sh
sudo /usr/local/sbin/ghostbsd-select-splash
```

---

## 7. Quick Tests

```sh
kldstat | egrep 'fbsplash|bitmap|vt_(efi|vbe)fb'
sysctl kern.vty
cat /var/log/boot-splash.log
```

Expected in the log:

```
vt(efifb): resolution 1920x1080
fbsplash.ko loaded, bitmap.ko loaded
```

---

## Result

* An EFI GOP or VBE splash appearing **before kernel init**.
* A seamless **handover to fbsplash.ko** until `vt(4)` initializes.
* Automatic unloading of the splash (and optional size logging).
* GhostBSD-branded 24-bit splash BMP matching your framebuffer.

