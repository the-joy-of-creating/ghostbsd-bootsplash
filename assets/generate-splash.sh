#!/bin/sh
#
# Generate splash images in multiple resolutions
#

set -e

SCRIPT_DIR=$(dirname "$0")
SOURCE_DIR="${SCRIPT_DIR}/source"
OUTPUT_DIR="${SCRIPT_DIR}/generated"
LOGO="${SOURCE_DIR}/ghostbsd-logo.png"
BACKGROUND_COLOR="#0b1220"

# Common resolutions
RESOLUTIONS="
1024x768
1280x720
1280x800
1366x768
1440x900
1600x900
1920x1080
1920x1200
2560x1440
2560x1600
3840x2160
"

info() {
    echo "==> $*"
}

error() {
    echo "Error: $*" >&2
    exit 1
}

check_dependencies() {
    if ! command -v convert >/dev/null 2>&1; then
        error "ImageMagick not found. Install with: pkg install ImageMagick7"
    fi
}

create_output_dir() {
    mkdir -p "${OUTPUT_DIR}"
}

generate_splash() {
    local resolution=$1
    local output="${OUTPUT_DIR}/splash-${resolution}.bmp"
    
    info "Generating splash for ${resolution}..."
    
    convert "${LOGO}" \
        -background "${BACKGROUND_COLOR}" \
        -gravity center \
        -extent "${resolution}" \
        -type TrueColor \
        -define bmp:format=bmp3 \
        -compress None \
        "${output}"
    
    # Verify BMP format
    if file "${output}" | grep -q "PC bitmap"; then
        echo "    ✓ ${output}"
    else
        error "Failed to generate valid BMP: ${output}"
    fi
}

generate_all() {
    info "Generating splash images..."
    
    if [ ! -f "${LOGO}" ]; then
        error "Source logo not found: ${LOGO}"
    fi
    
    for res in ${RESOLUTIONS}; do
        generate_splash "${res}"
    done
    
    info "Generated $(ls -1 ${OUTPUT_DIR}/*.bmp | wc -l | tr -d ' ') splash images"
}

show_info() {
    echo ""
    info "Splash image details:"
    for bmp in "${OUTPUT_DIR}"/*.bmp; do
        size=$(stat -f %z "${bmp}" 2>/dev/null || stat -c %s "${bmp}" 2>/dev/null)
        size_mb=$(echo "scale=2; ${size} / 1048576" | bc)
        printf "  %-30s %8s MB\n" "$(basename ${bmp})" "${size_mb}"
    done
    echo ""
}

main() {
    check_dependencies
    create_output_dir
    generate_all
    show_info
    
    info "Complete! Splash images are in ${OUTPUT_DIR}/"
}

main "$@"
