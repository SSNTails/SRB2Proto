//
// vid_copy.S : code for updating the linear frame buffer screen.
//

#include "asm_defs.inc"           // structures, must match the C structures!

// DJGPPv2 is as fast as this one, but then someone may compile with a less
// good version of DJGPP than mine, so this little asm will do the trick!

#define srcptr          4+16
#define destptr         8+16
#define width           12+16
#define height          16+16
#define srcrowbytes     20+16
#define destrowbytes    24+16

// VID_BlitLinearScreen( src, dest, width, height, srcwidth, destwidth );
//         width is given as BYTES

.globl C(VID_BlitLinearScreen)
C(VID_BlitLinearScreen):
    pushl   %ebp                // preserve caller's stack frame
    pushl   %edi
    pushl   %esi                // preserve register variables
    pushl   %ebx

    cld
    movl    srcptr(%esp),%esi
    movl    destptr(%esp),%edi
    movl    width(%esp),%ebx
    movl    srcrowbytes(%esp),%eax
    subl    %ebx,%eax
    movl    destrowbytes(%esp),%edx
    subl    %ebx,%edx
    shrl    $2,%ebx
    movl    height(%esp),%ebp
LLRowLoop:
    movl    %ebx,%ecx
    rep/movsl   (%esi),(%edi)
    addl    %eax,%esi
    addl    %edx,%edi
    decl    %ebp
    jnz     LLRowLoop

    popl    %ebx                // restore register variables
    popl    %esi
    popl    %edi
    popl    %ebp                // restore the caller's stack frame

    ret
