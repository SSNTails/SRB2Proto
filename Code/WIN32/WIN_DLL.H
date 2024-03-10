// win_3dll.h : load/unload a DLL at run-time

#include <windows.h>

// this is made extern so win_dbg.c can log the load address
// of the dynamically loaded DLL functions
typedef struct loadfunc_s {
    char*   fnName;
    void*   fnPointer;
} loadfunc_t;
extern HINSTANCE  hwdInstance;
extern loadfunc_t hwdFuncTable[];

//for win_dbg.c
extern HINSTANCE hwdInstance;
extern loadfunc_t hwdFuncTable[];

    
HINSTANCE LoadDLL (char* dllName, loadfunc_t* funcTable);
void UnloadDLL (HINSTANCE* pInstance);

BOOL Init3DDriver (char* dllName);
void Shutdown3DDriver (void);
