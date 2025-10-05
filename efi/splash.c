#include <efi.h>
#include <efilib.h>

// BMP file header structures
#pragma pack(push, 1)
typedef struct {
    UINT16 Type;
    UINT32 Size;
    UINT16 Reserved1;
    UINT16 Reserved2;
    UINT32 OffBits;
} BMP_FILE_HEADER;

typedef struct {
    UINT32 Size;
    INT32  Width;
    INT32  Height;
    UINT16 Planes;
    UINT16 BitCount;
    UINT32 Compression;
    UINT32 SizeImage;
    INT32  XPelsPerMeter;
    INT32  YPelsPerMeter;
    UINT32 ClrUsed;
    UINT32 ClrImportant;
} BMP_INFO_HEADER;
#pragma pack(pop)

EFI_STATUS LoadBMPFromFile(EFI_FILE_PROTOCOL *Root, CHAR16 *FileName, UINT8 **ImageData, UINTN *ImageSize) {
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL *File;
    EFI_FILE_INFO *FileInfo;
    UINTN BufferSize = sizeof(EFI_FILE_INFO) + 512;
    
    // Open the BMP file
    Status = Root->Open(Root, &File, FileName, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    
    // Get file size
    FileInfo = AllocatePool(BufferSize);
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
    
    return Status;
}

EFI_STATUS DisplayBMP(EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop, UINT8 *BmpData, UINTN BmpSize) {
    BMP_FILE_HEADER *FileHeader = (BMP_FILE_HEADER *)BmpData;
    BMP_INFO_HEADER *InfoHeader = (BMP_INFO_HEADER *)(BmpData + sizeof(BMP_FILE_HEADER));
    
    // Validate BMP signature
    if (FileHeader->Type != 0x4D42) { // "BM"
        return EFI_INVALID_PARAMETER;
    }
    
    // Only support 24-bit uncompressed BMPs
    if (InfoHeader->BitCount != 24 || InfoHeader->Compression != 0) {
        return EFI_UNSUPPORTED;
    }
    
    UINT8 *PixelData = BmpData + FileHeader->OffBits;
    INT32 Width = InfoHeader->Width;
    INT32 Height = InfoHeader->Height;
    
    // BMP rows are padded to 4-byte boundaries
    UINTN RowSize = ((Width * 3 + 3) / 4) * 4;
    
    // Center the image on screen
    UINT32 ScreenWidth = Gop->Mode->Info->HorizontalResolution;
    UINT32 ScreenHeight = Gop->Mode->Info->VerticalResolution;
    INT32 OffsetX = (ScreenWidth - Width) / 2;
    INT32 OffsetY = (ScreenHeight - Height) / 2;
    
    if (OffsetX < 0) OffsetX = 0;
    if (OffsetY < 0) OffsetY = 0;
    
    // Clear screen to black
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Black = {0, 0, 0, 0};
    Gop->Blt(Gop, &Black, EfiBltVideoFill, 0, 0, 0, 0, ScreenWidth, ScreenHeight, 0);
    
    // Draw BMP (BMPs are stored bottom-up)
    for (INT32 y = 0; y < Height; y++) {
        for (INT32 x = 0; x < Width; x++) {
            UINTN BmpIndex = (Height - 1 - y) * RowSize + x * 3;
            
            EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel;
            Pixel.Blue = PixelData[BmpIndex];
            Pixel.Green = PixelData[BmpIndex + 1];
            Pixel.Red = PixelData[BmpIndex + 2];
            Pixel.Reserved = 0;
            
            Gop->Blt(Gop, &Pixel, EfiBltVideoFill, 
                     0, 0, 
                     OffsetX + x, OffsetY + y, 
                     1, 1, 0);
        }
    }
    
    return EFI_SUCCESS;
}

EFI_STATUS ChainloadBootloader(EFI_HANDLE ImageHandle, CHAR16 *BootloaderPath) {
    EFI_STATUS Status;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    EFI_HANDLE BootloaderHandle;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    
    // Get our own loaded image
    Status = uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle, 
                               &LoadedImageProtocol, (VOID **)&LoadedImage);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    
    // Build device path for bootloader
    DevicePath = FileDevicePath(LoadedImage->DeviceHandle, BootloaderPath);
    if (DevicePath == NULL) {
        return EFI_NOT_FOUND;
    }
    
    // Load the bootloader
    Status = uefi_call_wrapper(BS->LoadImage, 6, FALSE, ImageHandle, 
                               DevicePath, NULL, 0, &BootloaderHandle);
    FreePool(DevicePath);
    
    if (EFI_ERROR(Status)) {
        return Status;
    }
    
    // Start the bootloader
    Status = uefi_call_wrapper(BS->StartImage, 3, BootloaderHandle, NULL, NULL);
    
    return Status;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE_PROTOCOL *Root;
    UINT8 *BmpData = NULL;
    UINTN BmpSize;
    
    InitializeLib(ImageHandle, SystemTable);
    
    // Locate Graphics Output Protocol
    Status = uefi_call_wrapper(BS->LocateProtocol, 3, &GraphicsOutputProtocol, 
                               NULL, (VOID **)&Gop);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to locate GOP: %r\n", Status);
        return Status;
    }
    
    // Get filesystem access
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    Status = uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle, 
                               &LoadedImageProtocol, (VOID **)&LoadedImage);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    
    Status = uefi_call_wrapper(BS->HandleProtocol, 3, LoadedImage->DeviceHandle,
                               &FileSystemProtocol, (VOID **)&FileSystem);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    
    // Load splash image
    Status = LoadBMPFromFile(Root, L"\\EFI\\GhostBSD\\splash.bmp", &BmpData, &BmpSize);
    if (!EFI_ERROR(Status)) {
        DisplayBMP(Gop, BmpData, BmpSize);
        FreePool(BmpData);
        
        // Display for 2 seconds
        uefi_call_wrapper(BS->Stall, 1, 2000000);
    }
    
    Root->Close(Root);
    
    // Chainload FreeBSD bootloader
    Status = ChainloadBootloader(ImageHandle, L"\\EFI\\BOOT\\BOOTX64.EFI");
    
    // If we get here, chainloading failed
    Print(L"Failed to chainload bootloader: %r\n", Status);
    uefi_call_wrapper(BS->Stall, 1, 3000000);
    
    return Status;
}
