// m_bbox.c : bounding boxes

#include "doomtype.h"
#include "m_bbox.h"

// faB: getting sick of windows includes errors,
//     I'm supposed to clean that up later.. sure
#ifdef __WIN32__
#define MAXINT    ((int)0x7fffffff)
#define MININT    ((int)0x80000000)
#endif


void M_ClearBox (fixed_t *box)
{
    box[BOXTOP] = box[BOXRIGHT] = MININT;
    box[BOXBOTTOM] = box[BOXLEFT] = MAXINT;
}

void M_AddToBox ( fixed_t*      box,
                  fixed_t       x,
                  fixed_t       y )

{
    if (x<box[BOXLEFT])
        box[BOXLEFT] = x;
    else if (x>box[BOXRIGHT])
        box[BOXRIGHT] = x;
    if (y<box[BOXBOTTOM])
        box[BOXBOTTOM] = y;
    else if (y>box[BOXTOP])
        box[BOXTOP] = y;
}
