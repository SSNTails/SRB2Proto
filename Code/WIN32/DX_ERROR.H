// dx_error.h : transform an unreadable DirectX error code
//              into a meaningful error message.

#ifndef __DX_ERROR_H__
#define __DX_ERROR_H__

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// Displays a message box containing the given formatted string.
//void __cdecl DXErrorMessageBox (LPSTR fmt, ... );

// Returns a pointer to a string describing the given DD, D3D or D3DRM error code.
char* DXErrorToString (HRESULT error);

#ifdef __cplusplus
};
#endif
#endif // __DX_ERROR_H__
