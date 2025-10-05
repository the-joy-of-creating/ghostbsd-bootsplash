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
    Status = Root->Open(Root, &File, FileName, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    
    // Get file size
    FileInfo = AllocatePool(BufferSize);
    if (FileInfo == NULL) {
        File->Close(File);
        return EFI_OUT_OF_RESOURCES;
    }
    
    Status = File->GetInfo(File, &gEfiFileInfoGuid, &BufferSize, FileInfo);
    if (EFI_ERROR(Status)) {
        File->Close(File);
        FreePool(FileInfo);
        return Status;
    }
    
    *ImageSize = FileInfo->FileSize;
    FreePool(FileInfo);
    
    // Allocate buffer and read file
    *ImageData = AllocatePool(*ImageSize);
    if (*ImageData == NULL) {
        File->Close(File);
        return EFI_OUT_OF_RESOURCES;
    }
    
    Status = File->Read(File, ImageSize, *ImageData);
    File->Close(File);
    
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

EFI_STATUS DisplayBMP(EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop, 
                      UINT8 *BmpData, UINTN BmpSize) {
    BMP_FILE_HEADER *FileHeader;
    BMP_INFO_HEADER *InfoHeader;
    UINT8 *PixelData;
    INT32 Width, Height;
    UINTN RowSize;
    UINT32 ScreenWidth, ScreenHeight;
    INT32 OffsetX, OffsetY;
    
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
    Gop->Blt(Gop, &Black, EfiBltVideoFill, 
             0, 0, 0, 0, 
             ScreenWidth, ScreenHeight, 0);
    
    // Draw BMP (BMPs are stored bottom-up unless height is negative)
    BOOLEAN TopDown = (Height < 0);
    if (TopDown) {
        Height = -Height;
    }
    
    for (INT32 y = 0; y < Height; y++) {
        for (INT32 x = 0; x < Width; x++) {
            UINTN BmpIndex;
            
            if (TopDown) {
                BmpIndex = y * RowSize + x * 3;
            } else {
                // Bottom-up: flip Y coordinate
                BmpIndex = (Height - 1 - y) * RowSize + x * 3;
            }
            
            // Bounds check
            if (BmpIndex + 2 >= BmpSize - FileHeader->OffBits) {
                continue;
            }
            
            EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel;
            Pixel.Blue = PixelData[BmpIndex];
            Pixel.Green = PixelData[BmpIndex + 1];
            Pixel.Red = PixelData[BmpIndex + 2];
            Pixel.Reserved = 0;
            
            // Draw pixel
            Gop->Blt(Gop, &Pixel, EfiBltVideoFill,
                     0, 0,
                     OffsetX + x, OffsetY + y,
                     1, 1, 0);
        }
    }
    
    return EFI_SUCCESS;
}
