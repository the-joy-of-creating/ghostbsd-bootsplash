#include <efi.h>
#include <efilib.h>
#include "error.h"
#include "input.h"

// Display error message on screen with proper formatting
void DisplayError(CHAR16 *Title, CHAR16 *Message, EFI_STATUS Status) {
    // Clear screen
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, 
                      EFI_TEXT_ATTR(EFI_LIGHTRED, EFI_BLACK));
    
    // Display error box
    Print(L"\n\n");
    Print(L"  ╔════════════════════════════════════════════════════════════╗\n");
    Print(L"  ║                                                            ║\n");
    Print(L"  ║  ERROR: %-48s ║\n", Title);
    Print(L"  ║                                                            ║\n");
    Print(L"  ╠════════════════════════════════════════════════════════════╣\n");
    Print(L"  ║                                                            ║\n");
    Print(L"  ║  %s%-48s%s ║\n", L"", Message, L"");
    Print(L"  ║                                                            ║\n");
    
    if (Status != 0) {
        Print(L"  ║  Status: 0x%016lx                            ║\n", Status);
    }
    
    Print(L"  ║                                                            ║\n");
    Print(L"  ╚════════════════════════════════════════════════════════════╝\n");
    Print(L"\n");
    
    // Reset to normal colors
    uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut,
                      EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
}

// Display warning message
void DisplayWarning(CHAR16 *Title, CHAR16 *Message) {
    uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut,
                      EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLACK));
    Print(L"\n  WARNING: %s\n", Title);
    Print(L"  %s\n\n", Message);
    uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut,
                      EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
}

// Display info message
void DisplayInfo(CHAR16 *Message) {
    uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut,
                      EFI_TEXT_ATTR(EFI_LIGHTCYAN, EFI_BLACK));
    Print(L"  %s\n", Message);
    uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut,
                      EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
}

// Fatal error - display and halt
void FatalError(CHAR16 *Title, CHAR16 *Message, EFI_STATUS Status) {
    DisplayError(Title, Message, Status);
    Print(L"\n  System will attempt to continue booting in 10 seconds...\n");
    Print(L"  Or press any key to boot immediately.\n\n");
    
    WaitForKeyOrTimeout(10000);
}

// Convert EFI_STATUS to human-readable string
CHAR16* StatusToString(EFI_STATUS Status) {
    switch (Status) {
        case EFI_SUCCESS:           return L"Success";
        case EFI_LOAD_ERROR:        return L"Load Error";
        case EFI_INVALID_PARAMETER: return L"Invalid Parameter";
        case EFI_UNSUPPORTED:       return L"Unsupported";
        case EFI_BAD_BUFFER_SIZE:   return L"Bad Buffer Size";
        case EFI_BUFFER_TOO_SMALL:  return L"Buffer Too Small";
        case EFI_NOT_READY:         return L"Not Ready";
        case EFI_DEVICE_ERROR:      return L"Device Error";
        case EFI_WRITE_PROTECTED:   return L"Write Protected";
        case EFI_OUT_OF_RESOURCES:  return L"Out of Resources";
        case EFI_NOT_FOUND:         return L"Not Found";
        case EFI_TIMEOUT:           return L"Timeout";
        case EFI_ABORTED:           return L"Aborted";
        case EFI_SECURITY_VIOLATION: return L"Security Violation";
        default:                    return L"Unknown Error";
    }
}

// Display boot splash info screen (for debugging)
void DisplayBootInfo(EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop) {
    if (Gop == NULL) {
        Print(L"Graphics Output Protocol: Not Available\n");
        return;
    }
    
    Print(L"\n");
    Print(L"  GhostBSD Boot Splash\n");
    Print(L"  ════════════════════════════════════════\n");
    Print(L"  Resolution: %dx%d\n", 
          Gop->Mode->Info->HorizontalResolution,
          Gop->Mode->Info->VerticalResolution);
    Print(L"  Pixel Format: %d\n", Gop->Mode->Info->PixelFormat);
    Print(L"  Mode: %d of %d\n", Gop->Mode->Mode, Gop->Mode->MaxMode);
    Print(L"  ════════════════════════════════════════\n\n");
}
