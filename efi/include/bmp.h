#ifndef _BMP_H_
#define _BMP_H_

#include <efi.h>
#include <efilib.h>

// BMP file structures
#pragma pack(push, 1)

typedef struct {
    UINT16 Type;        // "BM" = 0x4D42
    UINT32 Size;        // File size in bytes
    UINT16 Reserved1;
    UINT16 Reserved2;
    UINT32 OffBits;     // Offset to pixel data
} BMP_FILE_HEADER;

typedef struct {
    UINT32 Size;            // Header size
    INT32  Width;           // Image width
    INT32  Height;          // Image height
    UINT16 Planes;          // Must be 1
    UINT16 BitCount;        // Bits per pixel (24 or 32)
    UINT32 Compression;     // 0 = uncompressed
    UINT32 SizeImage;       // Image size (can be 0 for uncompressed)
    INT32  XPelsPerMeter;   // Horizontal resolution
    INT32  YPelsPerMeter;   // Vertical resolution
    UINT32 ClrUsed;         // Colors used
    UINT32 ClrImportant;    // Important colors
} BMP_INFO_HEADER;

#pragma pack(pop)

// Function declarations

// Load BMP file from filesystem
EFI_STATUS LoadBMPFromFile(
    EFI_FILE_PROTOCOL *Root,
    CHAR16 *FileName,
    UINT8 **ImageData,
    UINTN *ImageSize
);

// Display BMP on screen using GOP
EFI_STATUS DisplayBMP(
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop,
    UINT8 *BmpData,
    UINTN BmpSize
);

// Validate BMP format
BOOLEAN ValidateBMP(UINT8 *BmpData, UINTN BmpSize);

// Get BMP dimensions
EFI_STATUS GetBMPDimensions(
    UINT8 *BmpData,
    UINTN BmpSize,
    UINT32 *Width,
    UINT32 *Height
);

#endif // _BMP_H_
