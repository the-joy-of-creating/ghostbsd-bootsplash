#include <efi.h>
#include <efilib.h>
#include "bmp.h"
#include "input.h"
#include "error.h"

#define SPLASH_TIMEOUT_MS 2000
#define BOOTLOADER_PATH L"\\EFI\\BOOT\\BOOTX64.EFI"
#define SPLASH_IMAGE_PATH L"\\EFI\\GhostBSD\\splash.bmp"
#define VERSION_STRING L"GhostBSD Splash v1.0.0"

// Configuration flags
static BOOLEAN gDebugMode = FALSE;
static BOOLEAN gSkipOnKey = TRUE;

EFI_STATUS ChainloadBootloader(EFI_HANDLE ImageHandle, CHAR16 *BootloaderPath) {
    EFI_STATUS Status;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    EFI_HANDLE BootloaderHandle;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    
    // Get our own loaded image
    Status = uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle, 
                               &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
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
    
    // Start the bootloader (this should not return)
    Status = uefi_call_wrapper(BS->StartImage, 3, BootloaderHandle, NULL, NULL);
    
    return Status;
}

EFI_STATUS TryMultipleBootloaders(EFI_HANDLE ImageHandle) {
    EFI_STATUS Status;
    CHAR16 *BootloaderPaths[] = {
        L"\\EFI\\BOOT\\BOOTX64.EFI",
        L"\\EFI\\FreeBSD\\loader.efi",
        L"\\EFI\\GhostBSD\\BOOTX64.EFI",
        NULL
    };
    
    for (UINTN i = 0; BootloaderPaths[i] != NULL; i++) {
        if (gDebugMode) {
            DisplayInfo(L"Trying bootloader...");
            Print(L"  Path: %s\n", BootloaderPaths[i]);
        }
        
        Status = ChainloadBootloader(ImageHandle, BootloaderPaths[i]);
        
        if (!EFI_ERROR(Status)) {
            return Status; // Successfully booted
        }
        
        if (gDebugMode) {
            Print(L"  Failed: %s\n\n", StatusToString(Status));
        }
    }
    
    return EFI_NOT_FOUND;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE_PROTOCOL *Root;
    UINT8 *BmpData = NULL;
    UINTN BmpSize;
    BOOLEAN SplashDisplayed = FALSE;
    
    InitializeLib(ImageHandle, SystemTable);
    
    // Check for debug key (F8) held at boot
    EFI_INPUT_KEY Key;
    if (IsKeyPressed(&Key) && Key.ScanCode == SCAN_F8) {
        gDebugMode = TRUE;
        uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
        Print(L"\n%s - Debug Mode\n", VERSION_STRING);
        Print(L"════════════════════════════════════════════════════\n\n");
    }
    
    // Locate Graphics Output Protocol (CORRECTED)
    Status = uefi_call_wrapper(BS->LocateProtocol, 3, &gEfiGraphicsOutputProtocolGuid, 
                               NULL, (VOID **)&Gop);
    if (EFI_ERROR(Status)) {
        if (gDebugMode) {
            DisplayWarning(L"Graphics Not Available", 
                          L"GOP not found, skipping splash");
            uefi_call_wrapper(BS->Stall, 1, 2000000);
        }
        goto boot; // Skip splash, go straight to boot
    }
    
    if (gDebugMode) {
        DisplayBootInfo(Gop);
    }
    
    // Get filesystem access (CORRECTED)
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    Status = uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle, 
                               &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
    if (EFI_ERROR(Status)) {
        if (gDebugMode) {
            DisplayError(L"Failed to Get Loaded Image", 
                        L"Cannot access filesystem", Status);
        }
        goto boot;
    }
    
    // Open filesystem (CORRECTED)
    Status = uefi_call_wrapper(BS->HandleProtocol, 3, LoadedImage->DeviceHandle,
                               &gEfiSimpleFileSystemProtocolGuid, (VOID **)&FileSystem);
    if (EFI_ERROR(Status)) {
        if (gDebugMode) {
            DisplayError(L"Failed to Get Filesystem", 
                        L"Cannot open boot partition", Status);
        }
        goto boot;
    }
    
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    if (EFI_ERROR(Status)) {
        if (gDebugMode) {
            DisplayError(L"Failed to Open Volume", 
                        L"Cannot access files", Status);
        }
        goto boot;
    }
    
    // Load and display splash image
    Status = LoadBMPFromFile(Root, SPLASH_IMAGE_PATH, &BmpData, &BmpSize);
    if (EFI_ERROR(Status)) {
        if (gDebugMode) {
            DisplayWarning(L"Splash Image Not Found", 
                          L"Booting without splash screen");
            Print(L"  Expected path: %s\n\n", SPLASH_IMAGE_PATH);
            uefi_call_wrapper(BS->Stall, 1, 2000000);
        }
        Root->Close(Root);
        goto boot;
    }
    
    // Display the splash
    Status = DisplayBMP(Gop, BmpData, BmpSize);
    if (EFI_ERROR(Status)) {
        if (gDebugMode) {
            DisplayError(L"Failed to Display Splash", 
                        L"Invalid BMP format or display error", Status);
        }
        FreePool(BmpData);
        Root->Close(Root);
        goto boot;
    }
    
    SplashDisplayed = TRUE;
    FreePool(BmpData);
    Root->Close(Root);
    
    // Wait for timeout or key press
    if (gSkipOnKey) {
        if (gDebugMode) {
            // Show debug prompt on splash
            uefi_call_wrapper(ST->ConOut->SetCursorPosition, 3, ST->ConOut, 0, 0);
            Print(L" Debug: Press any key to boot immediately, or wait %d seconds\n", 
                  SPLASH_TIMEOUT_MS / 1000);
        }
        
        if (WaitForKeyOrTimeout(SPLASH_TIMEOUT_MS)) {
            if (gDebugMode) {
                DisplayInfo(L"Key pressed, booting immediately");
            }
        }
    } else {
        // Just wait for timeout
        uefi_call_wrapper(BS->Stall, 1, SPLASH_TIMEOUT_MS * 1000);
    }
    
boot:
    // Clear screen before booting
    if (SplashDisplayed || gDebugMode) {
        uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    }
    
    // Try to chainload bootloader
    Status = TryMultipleBootloaders(ImageHandle);
    
    // If we get here, all bootloaders failed
    FatalError(L"Boot Failure", 
              L"Could not load any bootloader", Status);
    
    // Last resort - try the original path one more time
    return ChainloadBootloader(ImageHandle, BOOTLOADER_PATH);
}
