// hwr_defs.h : hardware driver API definitions

#ifndef _HWR_DEFS_
#define _HWR_DEFS_

#define DEGREE (.01745328f)
#define PI     (3.1415926535f)

// SetState : possible values for IdState
enum hwdsetstate {
    HWD_SET_DEPTHMASK = 1,
    HWD_SET_COLORMASK,
    HWD_SET_CULLMODE,
    HWD_SET_CONSTANTCOLOR,
    HWD_SET_COLORSOURCE,
    HWD_SET_ALPHABLEND,
    HWD_SET_TEXTURECLAMP,
    HWD_SET_TEXTURECOMBINE,
    HWD_SET_TEXTUREFILTERMODE,
    HWD_SET_MIPMAPMODE,
    HWD_SET_ALPHATESTFUNC,
    HWD_SET_ALPHATESTREFVALUE,
    HWD_SET_ALPHASOURCE,
    HWD_ENABLE,
    HWD_DISABLE,
    HWD_SET_FOG_TABLE,
    HWD_SET_FOG_COLOR,
    HWD_SET_FOG_MODE,
    HWD_SET_CHROMAKEY_MODE,
    HWD_SET_CHROMAKEY_VALUE,
};
typedef enum hwdsetstate hwdstate_t;

// HWD_SET_COLORSOURCE
enum hwdcolorsource {
    HWD_COLORSOURCE_CONSTANT = 0,
    HWD_COLORSOURCE_ITERATED,
    HWD_COLORSOURCE_TEXTURE,
    HWD_COLORSOURCE_CONSTANTALPHA_SCALE_TEXTURE,
};

// HWD_SET_ALPHASOURCE
enum hwdalphasource {
    HWD_ALPHASOURCE_CONSTANT = 0,
    HWD_ALPHASOURCE_TEXTURE,
};

enum hwdalphablend {
    HWD_ALPHABLEND_NONE = 0,
    HWD_ALPHABLEND_TRANSLUCENT,
};

enum hwdtextureclamp {
    HWD_TEXTURE_CLAMP_XY = 0,
    HWD_TEXTURE_WRAP_XY,
};

enum hwdtexturecombine {
    HWD_TEXTURECOMBINE_NORMAL = 0,
};

enum hwdtexturefiltermode {
    HWD_SET_TEXTUREFILTER_POINTSAMPLED = 0,
    HWD_SET_TEXTUREFILTER_BILINEAR,
};

enum hwdmipmapmode {
    HWD_MIPMAP_DISABLE = 0,
};

enum hwdenabledisable {
    HWD_SHAMELESS_PLUG = 1,
};

enum hwdfogmode {
    HWD_FOG_DISABLE = 0,
    HWD_FOG_ENABLE,
};

enum hwdchromakeymode {
    HWD_CHROMAKEY_ENABLE = 1
};


// 3rd parameter for ClearBuffer : depth buffer
enum hwdcleardepth {
    HWD_CLEARDEPTH_MIN = 0,     //indexes gr_wrange
    HWD_CLEARDEPTH_MAX = 1
};
typedef enum hwdcleardepth hwdcleardepth_t;

// GetState : possible values for IdState
enum hwdgetstate {
    HWD_GET_FOGTABLESIZE = 1,
};
typedef enum hwdgetstate hwdgetstate_t;


#endif //_HWR_DEFS_