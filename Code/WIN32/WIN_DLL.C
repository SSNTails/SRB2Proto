// win_3dll.c : load and initialise the 3D driver DLL

#include "win_dll.h"
#include "win_main.h"       // I_GetLastErrorMsgBox()

// m_misc.h
char *va(char *format, ...);


// ==========================================================================
// STANDARD 3D DRIVER DLL FOR DOOM LEGACY
// ==========================================================================

// note : the 3D driver loading should be put somewhere else..

HINSTANCE hwdInstance = NULL;

loadfunc_t hwdFuncTable[] = {
    {NULL,NULL}
};

BOOL Init3DDriver (char* dllName)
{
    hwdInstance = LoadDLL (dllName, hwdFuncTable);
    return (hwdInstance != NULL);
}

void Shutdown3DDriver (void)
{
    UnloadDLL (&hwdInstance);
}


// --------------------------------------------------------------------------
// Load a DLL, returns the HINSTANCE handle or NULL
// --------------------------------------------------------------------------
HINSTANCE LoadDLL (char* dllName, loadfunc_t* funcTable)
{
    void*       funcPtr;
    loadfunc_t* loadfunc;
    HINSTANCE   hInstance;

    if ((hInstance = LoadLibrary (dllName)) != NULL)
    {
        // get function pointers for all functions we use
        for (loadfunc = funcTable; loadfunc->fnName!=NULL; loadfunc++)
        {
            funcPtr = GetProcAddress (hInstance, loadfunc->fnName);
            if (!funcPtr) {
                //I_GetLastErrorMsgBox ();
                MessageBox( NULL, va("GetProcAddress() FAILED : function '%s' not found in %s", loadfunc->fnName, dllName), "Error", MB_OK|MB_ICONINFORMATION );
                return FALSE;
            }
            // store function address
            *((void**)loadfunc->fnPointer) = funcPtr;
        }
    }
    else
    {
        MessageBox( NULL, va("LoadLibrary() FAILED : couldn't load '%s'\r\n", dllName), "Warning", MB_OK|MB_ICONINFORMATION );
        //I_GetLastErrorMsgBox ();
    }

    return hInstance;
}


// --------------------------------------------------------------------------
// Unload the DLL
// --------------------------------------------------------------------------
void UnloadDLL (HINSTANCE* pInstance)
{
    if (FreeLibrary (*pInstance))
        *pInstance = NULL;
    else
        I_GetLastErrorMsgBox ();
}
