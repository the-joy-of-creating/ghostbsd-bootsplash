#include <efi.h>
#include <efilib.h>
#include "input.h"

// Wait for timeout or key press, whichever comes first
// Returns: TRUE if key was pressed, FALSE if timeout
BOOLEAN WaitForKeyOrTimeout(UINTN TimeoutMs) {
    EFI_STATUS Status;
    EFI_INPUT_KEY Key;
    UINTN Index;
    EFI_EVENT TimerEvent;
    EFI_EVENT WaitList[2];
    
    // Create a timer event
    Status = uefi_call_wrapper(BS->CreateEvent, 5,
                               EVT_TIMER, 0, NULL, NULL, &TimerEvent);
    if (EFI_ERROR(Status)) {
        return FALSE;
    }
    
    // Set timer for timeout (in 100ns units, so multiply by 10000 for ms)
    Status = uefi_call_wrapper(BS->SetTimer, 3,
                               TimerEvent, TimerRelative, TimeoutMs * 10000);
    if (EFI_ERROR(Status)) {
        uefi_call_wrapper(BS->CloseEvent, 1, TimerEvent);
        return FALSE;
    }
    
    // Clear any pending key presses
    uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);
    
    // Wait for either key press or timeout
    WaitList[0] = ST->ConIn->WaitForKey;
    WaitList[1] = TimerEvent;
    
    Status = uefi_call_wrapper(BS->WaitForEvent, 3, 2, WaitList, &Index);
    
    uefi_call_wrapper(BS->CloseEvent, 1, TimerEvent);
    
    if (EFI_ERROR(Status)) {
        return FALSE;
    }
    
    // Index 0 = key pressed, Index 1 = timeout
    if (Index == 0) {
        // Read the key to clear it
        uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);
        return TRUE;
    }
    
    return FALSE;
}

// Check if a key is currently pressed (non-blocking)
BOOLEAN IsKeyPressed(EFI_INPUT_KEY *Key) {
    EFI_STATUS Status;
    
    Status = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, Key);
    
    return !EFI_ERROR(Status);
}

// Wait for specific key press
BOOLEAN WaitForKey(EFI_INPUT_KEY *Key, UINTN TimeoutMs) {
    if (!WaitForKeyOrTimeout(TimeoutMs)) {
        return FALSE;
    }
    
    return IsKeyPressed(Key);
}

// Display message and wait for key press
void PressAnyKey(CHAR16 *Message) {
    EFI_INPUT_KEY Key;
    
    if (Message != NULL) {
        Print(Message);
    } else {
        Print(L"\nPress any key to continue...");
    }
    
    // Clear input buffer
    uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);
    
    // Wait for key
    uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ST->ConIn->WaitForKey, NULL);
    uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);
}
