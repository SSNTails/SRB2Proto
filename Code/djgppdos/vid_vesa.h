
// vid_vesa.h : VESA extra modes
//

#include "../doomdef.h"
#include "../screen.h"



#define MODE_SUPPORTED_IN_HW    0x0001
#define COLOR_MODE              0x0008
#define GRAPHICS_MODE           0x0010
#define VGA_INCOMPATIBLE        0x0020
#define LINEAR_FRAME_BUFFER     0x0080
#define LINEAR_MODE             0x4000

#define MAX_VESA_MODES          30  // we'll just take the first 30 if there


// VESA information block structure
typedef struct vbeinfoblock_s
{
    unsigned char  VESASignature[4]   __attribute__ ((packed));
    unsigned short VESAVersion        __attribute__ ((packed));
    unsigned long  OemStringPtr       __attribute__ ((packed));
    byte    Capabilities[4];
    unsigned long  VideoModePtr       __attribute__ ((packed));
    unsigned short TotalMemory        __attribute__ ((packed));
    byte    OemSoftwareRev[2];
    byte    OemVendorNamePtr[4];
    byte    OemProductNamePtr[4];
    byte    OemProductRevPtr[4];
    byte    Reserved[222];
    byte    OemData[256];
} vbeinfoblock_t;


// VESA information for a specific mode
typedef struct vesamodeinfo_s
{
   unsigned short ModeAttributes       __attribute__ ((packed));
   unsigned char  WinAAttributes       __attribute__ ((packed));
   unsigned char  WinBAttributes       __attribute__ ((packed));
   unsigned short WinGranularity       __attribute__ ((packed));
   unsigned short WinSize              __attribute__ ((packed));
   unsigned short WinASegment          __attribute__ ((packed));
   unsigned short WinBSegment          __attribute__ ((packed));
   unsigned long  WinFuncPtr           __attribute__ ((packed));
   unsigned short BytesPerScanLine     __attribute__ ((packed));
   unsigned short XResolution          __attribute__ ((packed));
   unsigned short YResolution          __attribute__ ((packed));
   unsigned char  XCharSize            __attribute__ ((packed));
   unsigned char  YCharSize            __attribute__ ((packed));
   unsigned char  NumberOfPlanes       __attribute__ ((packed));
   unsigned char  BitsPerPixel         __attribute__ ((packed));
   unsigned char  NumberOfBanks        __attribute__ ((packed));
   unsigned char  MemoryModel          __attribute__ ((packed));
   unsigned char  BankSize             __attribute__ ((packed));
   unsigned char  NumberOfImagePages   __attribute__ ((packed));
   unsigned char  Reserved_page        __attribute__ ((packed));
   unsigned char  RedMaskSize          __attribute__ ((packed));
   unsigned char  RedMaskPos           __attribute__ ((packed));
   unsigned char  GreenMaskSize        __attribute__ ((packed));
   unsigned char  GreenMaskPos         __attribute__ ((packed));
   unsigned char  BlueMaskSize         __attribute__ ((packed));
   unsigned char  BlueMaskPos          __attribute__ ((packed));
   unsigned char  ReservedMaskSize     __attribute__ ((packed));
   unsigned char  ReservedMaskPos      __attribute__ ((packed));
   unsigned char  DirectColorModeInfo  __attribute__ ((packed));

   /* VBE 2.0 extensions */
   unsigned long  PhysBasePtr          __attribute__ ((packed));
   unsigned long  OffScreenMemOffset   __attribute__ ((packed));
   unsigned short OffScreenMemSize     __attribute__ ((packed));

   /* VBE 3.0 extensions */
   unsigned short LinBytesPerScanLine  __attribute__ ((packed));
   unsigned char  BnkNumberOfPages     __attribute__ ((packed));
   unsigned char  LinNumberOfPages     __attribute__ ((packed));
   unsigned char  LinRedMaskSize       __attribute__ ((packed));
   unsigned char  LinRedFieldPos       __attribute__ ((packed));
   unsigned char  LinGreenMaskSize     __attribute__ ((packed));
   unsigned char  LinGreenFieldPos     __attribute__ ((packed));
   unsigned char  LinBlueMaskSize      __attribute__ ((packed));
   unsigned char  LinBlueFieldPos      __attribute__ ((packed));
   unsigned char  LinRsvdMaskSize      __attribute__ ((packed));
   unsigned char  LinRsvdFieldPos      __attribute__ ((packed));
   unsigned long  MaxPixelClock        __attribute__ ((packed));

   unsigned char  Reserved[190]        __attribute__ ((packed));
} vesamodeinfo_t;


// setup standard VGA + VESA modes list, activate the default video mode.
void VID_Init (void);
// setup a video mode, this is to be called from the menu
int  VID_SetMode (int modenum);
