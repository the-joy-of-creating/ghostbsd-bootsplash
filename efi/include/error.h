#ifndef _ERROR_H_
#define _ERROR_H_

#include <efi.h>
#include <efilib.h>

// Display formatted error message
void DisplayError(CHAR16 *Title, CHAR16 *Message, EFI_STATUS Status);

// Display warning message
void DisplayWarning(CHAR16 *Title, CHAR16 *Message);

// Display info message
void DisplayInfo(CHAR16 *Message);

// Fatal error - display and attempt to continue
void FatalError(CHAR16 *Title, CHAR16 *Message, EFI_STATUS Status);

// Convert EFI_STATUS to string
CHAR16* StatusToString(EFI_STATUS Status);

// Display boot information (debug)
void DisplayBootInfo(EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop);

#endif // _ERROR_H_
