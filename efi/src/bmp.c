#include <efi.h>
#include <efilib.h>
#include "bmp.h"

EFI_STATUS LoadBMPFromFile(EFI_FILE_PROTOCOL *Root, CHAR16 *FileName, 
                           UINT8 **ImageData, UINTN *ImageSize) {
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL *File;
    EFI_FILE_INFO *FileInfo;
    UINTN BufferSize = sizeof(EFI_FILE_INFO) + 512;
    
    if (Root == NULL || FileName == NULL || ImageData == NULL || ImageSize == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Open the BMP file
    Status = uefi_call_wrapper(Root->Open, 5, Root, &File, FileName, 
                               EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    
    // Get file size
    FileInfo = AllocatePool(BufferSize);
    if (FileInfo == NULL) {
        uefi_call_wrapper(File->Close, 1, File);
        return EFI_OUT_OF_RESOURCES;
    }
    
    Status = uefi_call_wrapper(File->GetInfo, 4, File, &gEfiFileInfoGuid, 
                               &BufferSize, FileInfo);
    if (EFI_ERROR(Status)) {
        uefi_call_wrapper(File->Close, 1, File);
        FreePool(FileInfo);
        return Status;
    }
    
    *ImageSize = FileInfo->FileSize;
    FreePool(FileInfo);
    
    // Allocate buffer and read file
    *ImageData = AllocatePool(*ImageSize);
    if (*ImageData == NULL) {
        uefi_call_wrapper(File->Close, 1, File);
        return EFI_OUT_OF_RESOURCES;
    }
    
    Status = uefi_call_wrapper(File->Read, 3, File, ImageSize, *ImageData);
    uefi_call_wrapper(File->Close, 1, File);
    
    if (EFI_ERROR(Status)) {
        FreePool(*ImageData);
        *ImageData = NULL;
        return Status;
    }
    
    // Validate BMP
    if (!ValidateBMP(*ImageData, *ImageSize)) {
        FreePool(*ImageData);
        *ImageData = NULL;
        return EFI_INVALID_PARAMETER;
    }
    
    return EFI_SUCCESS;
}

BOOLEAN ValidateBMP(UINT8 *BmpData, UINTN BmpSize) {
    BMP_FILE_HEADER *FileHeader;
    BMP_INFO_HEADER *InfoHeader;
    
    if (BmpData == NULL || BmpSize < sizeof(BMP_FILE_HEADER) + sizeof(BMP_INFO_HEADER)) {
        return FALSE;
    }
    
    FileHeader = (BMP_FILE_HEADER *)BmpData;
    InfoHeader = (BMP_INFO_HEADER *)(BmpData + sizeof(BMP_FILE_HEADER));
    
    // Check BMP signature
    if (FileHeader->Type != 0x4D42) {
        return FALSE;
    }
    
    // Check for supported formats (24-bit uncompressed)
    if (InfoHeader->BitCount != 24 || InfoHeader->Compression != 0) {
        return FALSE;
    }
    
    // Sanity check dimensions
    if (InfoHeader->Width <= 0 || InfoHeader->Height == 0) {
        return FALSE;
    }
    
    if (InfoHeader->Width > 8192 || InfoHeader->Height > 8192) {
        return FALSE;
    }
    
    return TRUE;
}

EFI_STATUS GetBMPDimensions(UINT8 *BmpData, UINTN BmpSize, 
                            UINT32 *Width, UINT32 *Height) {
    BMP_INFO_HEADER *InfoHeader;
    
    if (!ValidateBMP(BmpData, BmpSize)) {
        return EFI_INVALID_PARAMETER;
    }
    
    InfoHeader = (BMP_INFO_HEADER *)(BmpData + sizeof(BMP_FILE_HEADER));
    
    *Width = (UINT32)InfoHeader->Width;
    *Height = (UINT32)(InfoHeader->Height > 0 ? InfoHeader->Height : -InfoHeader->Height);
    
    return EFI_SUCCESS;
}

// OPTIMIZED: Use buffer blitting instead of pixel-by-pixel
EFI_STATUS DisplayBMP(EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop, 
                      UINT8 *BmpData, UINTN BmpSize) {
    BMP_FILE_HEADER *FileHeader;
    BMP_INFO_HEADER *InfoHeader;
    UINT8 *PixelData;
    INT32 Width, Height;
    UINTN RowSize;
    UINT32 ScreenWidth, ScreenHeight;
    INT32 OffsetX, OffsetY;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer;
    EFI_STATUS Status;
    
    if (Gop == NULL || BmpData == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    if (!ValidateBMP(BmpData, BmpSize)) {
        return EFI_UNSUPPORTED;
    }
    
    FileHeader = (BMP_FILE_HEADER *)BmpData;
    InfoHeader = (BMP_INFO_HEADER *)(BmpData + sizeof(BMP_FILE_HEADER));
    PixelData = BmpData + FileHeader->OffBits;
    
    Width = InfoHeader->Width;
    Height = InfoHeader->Height;
    
    // BMP rows are padded to 4-byte boundaries
    RowSize = ((Width * 3 + 3) / 4) * 4;
    
    // Get screen dimensions
    ScreenWidth = Gop->Mode->Info->HorizontalResolution;
    ScreenHeight = Gop->Mode->Info->VerticalResolution;
    
    // Center the image on screen
    OffsetX = (ScreenWidth - Width) / 2;
    OffsetY = (ScreenHeight - Height) / 2;
    
    if (OffsetX < 0) OffsetX = 0;
    if (OffsetY < 0) OffsetY = 0;
    
    // Clear screen to black
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Black = {0, 0, 0, 0};
    Status = uefi_call_wrapper(Gop->Blt, 10, Gop, &Black, EfiBltVideoFill, 
                               0, 0, 0, 0, 
                               ScreenWidth, ScreenHeight, 0);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    
    // Determine if BMP is top-down or bottom-up
    BOOLEAN TopDown = (Height < 0);
    if (TopDown) {
        Height = -Height;
    }
    
    // Allocate buffer for entire image
    BltBuffer = AllocatePool(Width * Height * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
    if (BltBuffer == NULL) {
        // Fall back to row-by-row rendering if we can't allocate full buffer
        return DisplayBMPRowByRow(Gop, BmpData, BmpSize, OffsetX, OffsetY);
    }
    
    // Convert BMP to BltBuffer format
    for (INT32 y = 0; y < Height; y++) {
        for (INT32 x = 0; x < Width; x++) {
            UINTN BmpIndex;
            UINTN BltIndex;
            
            if (TopDown) {
                BmpIndex = y * RowSize + x * 3;
                BltIndex = y * Width + x;
            } else {
                // Bottom-up: flip Y coordinate
                BmpIndex = (Height - 1 - y) * RowSize + x * 3;
                BltIndex = y * Width + x;
            }
            
            // Bounds check
            if (BmpIndex + 2 >= BmpSize - FileHeader->OffBits) {
                FreePool(BltBuffer);
                return EFI_INVALID_PARAMETER;
            }
            
            // Convert BGR to RGB
            BltBuffer[BltIndex].Blue = PixelData[BmpIndex];
            BltBuffer[BltIndex].Green = PixelData[BmpIndex + 1];
            BltBuffer[BltIndex].Red = PixelData[BmpIndex + 2];
            BltBuffer[BltIndex].Reserved = 0;
        }
    }
    
    // Blit entire image in one call (much faster!)
    Status = uefi_call_wrapper(Gop->Blt, 10, Gop, BltBuffer, EfiBltBufferToVideo,
                               0, 0,                    // Source X, Y
                               OffsetX, OffsetY,        // Dest X, Y
                               Width, Height,           // Width, Height
                               0);                      // Delta (0 = width * pixel size)
    
    FreePool(BltBuffer);
    return Status;
}

// Fallback: Row-by-row rendering if full buffer allocation fails
static EFI_STATUS DisplayBMPRowByRow(EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop,
                                     UINT8 *BmpData, UINTN BmpSize,
                                     INT32 OffsetX, INT32 OffsetY) {
    BMP_FILE_HEADER *FileHeader = (BMP_FILE_HEADER *)BmpData;
    BMP_INFO_HEADER *InfoHeader = (BMP_INFO_HEADER *)(BmpData + sizeof(BMP_FILE_HEADER));
    UINT8 *PixelData = BmpData + FileHeader->OffBits;
    INT32 Width = InfoHeader->Width;
    INT32 Height = InfoHeader->Height;
    UINTN RowSize = ((Width * 3 + 3) / 4) * 4;
    BOOLEAN TopDown = (Height < 0);
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *RowBuffer;
    EFI_STATUS Status = EFI_SUCCESS;
    
    if (TopDown) Height = -Height;
    
    // Allocate buffer for one row
    RowBuffer = AllocatePool(Width * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
    if (RowBuffer == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }
    
    // Draw row by row
    for (INT32 y = 0; y < Height; y++) {
        // Convert one row
        for (INT32 x = 0; x < Width; x++) {
            UINTN BmpIndex;
            
            if (TopDown) {
                BmpIndex = y * RowSize + x * 3;
            } else {
                BmpIndex = (Height - 1 - y) * RowSize + x * 3;
            }
            
            if (BmpIndex + 2 >= BmpSize - FileHeader->OffBits) {
                FreePool(RowBuffer);
                return EFI_INVALID_PARAMETER;
            }
            
            RowBuffer[x].Blue = PixelData[BmpIndex];
            RowBuffer[x].Green = PixelData[BmpIndex + 1];
            RowBuffer[x].Red = PixelData[BmpIndex + 2];
            RowBuffer[x].Reserved = 0;
        }
        
        // Blit one row
        Status = uefi_call_wrapper(Gop->Blt, 10, Gop, RowBuffer, EfiBltBufferToVideo,
                                   0, 0,                    // Source X, Y
                                   OffsetX, OffsetY + y,    // Dest X, Y
                                   Width, 1,                // Width, Height (1 row)
                                   0);
        
        if (EFI_ERROR(Status)) {
            break;
        }
    }
    
    FreePool(RowBuffer);
    return Status;
}
