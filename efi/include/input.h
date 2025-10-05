#ifndef _INPUT_H_
#define _INPUT_H_

#include <efi.h>
#include <efilib.h>

// Wait for timeout or key press
// Returns TRUE if key pressed, FALSE if timeout
BOOLEAN WaitForKeyOrTimeout(UINTN TimeoutMs);

// Check if key is currently pressed (non-blocking)
BOOLEAN IsKeyPressed(EFI_INPUT_KEY *Key);

// Wait for specific key with timeout
BOOLEAN WaitForKey(EFI_INPUT_KEY *Key, UINTN TimeoutMs);

// Display message and wait for any key
void PressAnyKey(CHAR16 *Message);

#endif // _INPUT_H_
