;; tmap.nas : assembler optimised rendering code for software mode
;;            draw floor spans, and wall columns.

[BITS 32]

;; structures, must match the C structures!
;;INCLUDE asm_defs.inc

;; externs
;; columns
[EXTERN _dc_x]
[EXTERN _dc_yl]
[EXTERN _dc_yh]
[EXTERN _ylookup]
[EXTERN _columnofs]
[EXTERN _dc_source]
[EXTERN _dc_texturemid]
[EXTERN _dc_iscale]
[EXTERN _centery]
[EXTERN _dc_colormap]
[EXTERN _dc_transmap]
[EXTERN _colormaps]

;; spans
[EXTERN _ds_x1]
[EXTERN _ds_x2]
[EXTERN _ds_y]
[EXTERN _ds_xfrac]
[EXTERN _ds_yfrac]
[EXTERN _ds_xstep]
[EXTERN _ds_ystep]
[EXTERN _ds_source]
[EXTERN _ds_colormap]

;; polygon edge rasterizer
[EXTERN _prastertab]


;;----------------------------------------------------------------------
;;
;; R_DrawColumn
;;
;; New  optimised version 10-01-1998 by D.Fabrice and P.Boris
;; TO DO: optimise it much farther... should take at most 3 cycles/pix
;;      once it's fixed, add code to patch the offsets so that it
;;      works in every screen width.
;;
;;----------------------------------------------------------------------

[SECTION .data]

;;.align        4
loopcount       dd      0
pixelcount      dd      0
tystep          dd      0

rowbytes        dd      0
doublerowbytes  dd      0

[SECTION .text]

;----------------------------------------------------------------------------
;fixed_t FixedMul (fixed_t a, fixed_t b)
;----------------------------------------------------------------------------
[GLOBAL _FixedMul]
;       align   16
_FixedMul:
        mov     eax,[esp+4]
        imul    dword [esp+8]
        shrd    eax,edx,16
        ret

;----------------------------------------------------------------------------
;fixed_t FixedDiv2 (fixed_t a, fixed_t b);
;----------------------------------------------------------------------------
[GLOBAL _FixedDiv2]
;       align   16
_FixedDiv2:
        mov     eax,[esp+4]
        mov     edx,eax                 ;; these two instructions allow the next
        sar     edx,31                  ;; two to pair, on the Pentium processor.
        shld    edx,eax,16
        sal     eax,16
        idiv    dword [esp+8]
        ret

;----------------------------------------------------------------------------
; void  ASM_PatchRowBytes (int rowbytes);
;----------------------------------------------------------------------------
[GLOBAL _ASM_PatchRowBytes]
;       align   16
_ASM_PatchRowBytes:
        mov     eax,[esp+4]
        mov     [rowbytes],eax
        add     eax,eax
        mov     [doublerowbytes],eax
        ret


;----------------------------------------------------------------------------
; 8bpp column drawer
;----------------------------------------------------------------------------

[GLOBAL _R_DrawColumn_8]
;       align   16
_R_DrawColumn_8:
        push    ebp                     ;; preserve caller's stack frame pointer
        push    esi                     ;; preserve register variables
        push    edi
        push    ebx
;;
;; dest = ylookup[_dc_yl] + columnofs[_dc_x];
;;
        mov     ebp,[_dc_yl]
        mov     ebx,ebp
        mov     edi,[_ylookup+ebx*4]
        mov     ebx,[_dc_x]
        add     edi,[_columnofs+ebx*4]  ;; edi = dest
;;
;; pixelcount = yh - yl + 1
;;
        mov     eax,[_dc_yh]
        inc     eax
        sub     eax,ebp                 ;; pixel count
        mov     [pixelcount],eax        ;; save for final pixel
        jle     near vdone                   ;; nothing to scale
;;
;; frac = dc_texturemid - (centery-dc_yl)*fracstep;
;;
        mov     ecx,[_dc_iscale]        ;; fracstep
        mov     eax,[_centery]
        sub     eax,ebp
        imul    eax,ecx
        mov     edx,[_dc_texturemid]
        sub     edx,eax
        mov     ebx,edx
        shr     ebx,16                  ;; frac int.
        and     ebx,0x7f
        shl     edx,16                  ;; y frac up

        mov     ebp,ecx
        shl     ebp,16                  ;; fracstep f. up
        shr     ecx,16                  ;; fracstep i. ->cl
        and     cl,0x7f

        mov     esi,[_dc_source]
;;
;; lets rock :) !
;;
        mov     eax,[pixelcount]
        mov     dh,al
        shr     eax,2
        mov     ch,al                   ;; quad count
        mov     eax,[_dc_colormap]
        test    dh,0x3
        je      near .v4quadloop
;;
;;  do un-even pixel
;;
        test    dh,0x1
        je      .two_uneven

        mov     al,[esi+ebx]            ;; prep un-even loops
        add     edx,ebp                 ;; ypos f += ystep f
        adc     bl,cl                   ;; ypos i += ystep i
        mov     dl,[eax]                ;; colormap texel
        and     bl,0x7f                 ;; mask 0-127 texture index
        mov     [edi],dl                ;; output pixel
        add     edi,[rowbytes]
;;
;;  do two non-quad-aligned pixels
;;
.two_uneven:
        test    dh,0x2
        je      .f3

        mov     al,[esi+ebx]            ;; fetch source texel      
        add     edx,ebp                 ;; ypos f += ystep f      
        adc     bl,cl                   ;; ypos i += ystep i       
        mov     dl,[eax]                ;; colormap texel         
        and     bl,0x7f                 ;; mask 0-127 texture index
        mov     [edi],dl                ;; output pixel           
        mov     al,[esi+ebx]                                       
        add     edx,ebp                 ;; fetch source texel      
        adc     bl,cl                   ;; ypos f += ystep f      
        mov     dl,[eax]                ;; ypos i += ystep i       
        and     bl,0x7f                 ;; colormap texel         
        add     edi,[rowbytes]          ;; mask 0-127 texture index
        mov     [edi],dl                                           
        add     edi,[rowbytes]          ;; output pixel           
;;
;;  test if there was at least 4 pixels
;;
.f3:
        test    ch,0xff                 ;; test quad count
        je      near vdone
;;
;; ebp : ystep frac. upper 24 bits
;; edx : y     frac. upper 24 bits
;; ebx : y     i.    lower 7 bits,  masked for index
;; ecx : ch = counter, cl = y step i.
;; eax : colormap aligned 256
;; esi : source texture column
;; edi : dest screen
;;
.v4quadloop:
        mov     dh,0x7f                 ;; prep mask
vquadloop:
        mov     al,[esi+ebx]            ;; prep loop                 
        add     edx,ebp                 ;; ypos f += ystep f       
        adc     bl,cl                   ;; ypos i += ystep i        
        mov     dl,[eax]                ;; colormap texel          
        mov     [edi],dl                ;; output pixel             
        and     bl,0x7f                 ;; mask 0-127 texture index

        mov     al,[esi+ebx]            ;; fetch source texel
        add     edx,ebp
        adc     bl,cl
        add     edi,[rowbytes]
        mov     dl,[eax]
        and     bl,0x7f
        mov     [edi],dl
        
        mov     al,[esi+ebx]            ;; fetch source texel
        add     edx,ebp
        adc     bl,cl
        add     edi,[rowbytes]
        mov     dl,[eax]
        and     bl,0x7f
        mov     [edi],dl
        
        mov     al,[esi+ebx]            ;; fetch source texel
        add     edx,ebp
        adc     bl,cl
        add     edi,[rowbytes]
        mov     dl,[eax]
        and     bl,0x7f
        mov     [edi],dl
        
        add     edi,[rowbytes]

        dec     ch
        jne     vquadloop

vdone:
        pop     ebx                     ;; restore register variables
        pop     edi
        pop     esi
        pop     ebp                     ;; restore caller's stack frame pointer
        ret

;;----------------------------------------------------------------------
;;13-02-98:
;;      R_DrawSkyColumn : same as R_DrawColumn but:
;;
;;      - wrap around 256 instead of 127.
;;      this is needed because we have a higher texture for mouselook,
;;      we need at least 200 lines for the sky.
;;
;;      NOTE: the sky should never wrap, so it could use a faster method.
;;            for the moment, we'll still use a wrapping method...
;;
;;      IT S JUST A QUICK CUT N PASTE, WAS NOT OPTIMISED AS IT SHOULD BE !!!
;;
;;----------------------------------------------------------------------

[GLOBAL _R_DrawSkyColumn_8]
;       align   16
_R_DrawSkyColumn_8:
        push    ebp
        push    esi
        push    edi
        push    ebx
;;
;; dest = ylookup[dc_yl] + columnofs[dc_x];
;;
        mov     ebp,[_dc_yl]
        mov     ebx,ebp
        mov     edi,[_ylookup+ebx*4]
        mov     ebx,[_dc_x]
        add     edi,[_columnofs+ebx*4]   ;; edi = dest
;;
;; pixelcount = yh - yl + 1
;;
        mov     eax,[_dc_yh]
        inc     eax
        sub     eax,ebp                 ;; pixel count         
        mov     [pixelcount],eax        ;; save for final pixel
        jle     near    .vskydone       ;; nothing to scale 
;;
;; frac = dc_texturemid - (centery-dc_yl)*fracstep;
;;
        mov     ecx,[_dc_iscale]        ;; fracstep
        mov     eax,[_centery]
        sub     eax,ebp
        imul    eax,ecx
        mov     edx,[_dc_texturemid]
        sub     edx,eax
        mov     ebx,edx
        shr     ebx,16                  ;; frac int.
        and     ebx,0xff
        shl     edx,16                  ;; y frac up
        mov     ebp,ecx
        shl     ebp,16                  ;; fracstep f. up
        shr     ecx,16                  ;; fracstep i. ->cl
        mov     esi,[_dc_source]
;;
;; lets rock :) !
;;
        mov     eax,[pixelcount]
        mov     dh,al
        shr     eax,0x2
        mov     ch,al                   ;; quad count
        mov     eax,[_dc_colormap]
        test    dh,0x3
        je      .v4skyquadloop
;;
;;  do un-even pixel
;;
        test    dh,0x1
        je      .f2
        mov     al,[esi+ebx]            ;; prep un-even loops 
        add     edx,ebp                 ;; ypos f += ystep f
        adc     bl,cl                   ;; ypos i += ystep i 
        mov     dl,[eax]                ;; colormap texel   
        mov     [edi],dl                ;; output pixel     
        add     edi,[rowbytes]
;;
;;  do two non-quad-aligned pixels
;;
.f2:    test    dh,0x2
        je      .f3

        mov     al,[esi+ebx]            ;; fetch source texel
        add     edx,ebp                 ;; ypos f += ystep f
        adc     bl,cl                   ;; ypos i += ystep i 
        mov     dl,[eax]                ;; colormap texel   
        mov     [edi],dl                ;; output pixel     

        mov     al,[esi+ebx]            ;; fetch source texel                     
        add     edx,ebp                 ;; ypos f += ystep f 
        adc     bl,cl                   ;; ypos i += ystep i 
        mov     dl,[eax]                ;; colormap texel    
        add     edi,[rowbytes]                               
        mov     [edi],dl                ;; output pixel      

        add     edi,[rowbytes]          
;;
;;  test if there was at least 4 pixels
;;
.f3:    test    ch,0xff                 ;; test quad count
        je      .vskydone
;;
;; ebp : ystep frac. upper 24 bits
;; edx : y     frac. upper 24 bits
;; ebx : y     i.    lower 7 bits,  masked for index
;; ecx : ch = counter, cl = y step i.
;; eax : colormap aligned 256
;; esi : source texture column
;; edi : dest screen
;;
.v4skyquadloop:
.vskyquadloop:
        mov     al,[esi+ebx]            ;; prep loop          
        add     edx,ebp                 ;; ypos f += ystep f
        mov     dl,[eax]                ;; colormap texel   
        adc     bl,cl                   ;; ypos i += ystep i 
        mov     [edi],dl                ;; output pixel      

        mov     al,[esi+ebx]            ;; fetch source texel
        add     edx,ebp
        adc     bl,cl
        add     edi,[rowbytes]
        mov     dl,[eax]
        mov     [edi],dl

        mov     al,[esi+ebx]            ;; fetch source texel
        add     edx,ebp
        adc     bl,cl
        add     edi,[rowbytes]
        mov     dl,[eax]
        mov     [edi],dl

        mov     al,[esi+ebx]            ;; fetch source texel
        add     edx,ebp
        adc     bl,cl
        add     edi,[rowbytes]
        mov     dl,[eax]
        mov     [edi],dl

        add     edi,[rowbytes]

        dec     ch
        jne     .vskyquadloop
.vskydone:      
        pop     ebx
        pop     edi
        pop     esi
        pop     ebp
        ret


;;----------------------------------------------------------------------
;; R_DrawTransColumn
;;
;; Vertical column texture drawer, with transparency. Replaces Doom2's
;; 'fuzz' effect, which was not so beautiful.
;; Transparency is always impressive in some way, don't know why...
;;----------------------------------------------------------------------

[GLOBAL _R_DrawTranslucentColumn_8]
_R_DrawTranslucentColumn_8:
        push    ebp                     ;; preserve caller's stack frame pointer
        push    esi                     ;; preserve register variables
        push    edi
        push    ebx
;;
;; dest = ylookup[dc_yl] + columnofs[dc_x];
;;
        mov     ebp,[_dc_yl]
        mov     ebx,ebp
        mov     edi,[_ylookup+ebx*4]
        mov     ebx,[_dc_x]
        add     edi,[_columnofs+ebx*4]   ;; edi = dest
;;
;; pixelcount = yh - yl + 1
;;
        mov     eax,[_dc_yh]
        inc     eax
        sub     eax,ebp                 ;; pixel count         
        mov     [pixelcount],eax        ;; save for final pixel
        jle     near    .vtdone         ;; nothing to scale 
;;
;; frac = dc_texturemid - (centery-dc_yl)*fracstep;
;;
        mov     ecx,[_dc_iscale]        ;; fracstep
        mov     eax,[_centery]
        sub     eax,ebp
        imul    eax,ecx
        mov     edx,[_dc_texturemid]
        sub     edx,eax
        mov     ebx,edx

        shr     ebx,16                  ;; frac int.
        and     ebx,0x7f
        shl     edx,16                  ;; y frac up
        mov     ebp,ecx
        shl     ebp,16                  ;; fracstep f. up
        shr     ecx,16                  ;; fracstep i. ->cl
        and     cl,0x7f
        mov     esi,[_dc_source]
;;
;; lets rock :) !
;;
        mov     eax,[pixelcount]
        mov     dh,al
        shr     eax,0x2
        mov     ch,al                   ;; quad count
        mov     eax,[_dc_transmap]
        test    dh,0x3
        je      .vt4quadloop
;;
;;  do un-even pixel
;;
        test    dh,0x1
        je      .f2

        mov     ah,[esi+ebx]            ;; fetch texel : colormap number
        add     edx,ebp
        adc     bl,cl
        mov     al,[edi]                ;; fetch dest  : index into colormap
        and     bl,0x7f
        mov     dl,[eax]
        mov     [edi],dl
        add     edi,[rowbytes]
;;
;;  do two non-quad-aligned pixels
;;
.f2:    test    dh,0x2
        je      .f3
        
        mov     ah,[esi+ebx]            ;; fetch texel : colormap number
        add     edx,ebp
        adc     bl,cl
        mov     al,[edi]                ;; fetch dest  : index into colormap
        and     bl,0x7f
        mov     dl,[eax]
        mov     [edi],dl
        add     edi,[rowbytes]
        
        mov     ah,[esi+ebx]            ;; fetch texel : colormap number
        add     edx,ebp
        adc     bl,cl
        mov     al,[edi]                ;; fetch dest  : index into colormap
        and     bl,0x7f
        mov     dl,[eax]
        mov     [edi],dl
        add     edi,[rowbytes]
;;
;;  test if there was at least 4 pixels
;;
.f3:    test    ch,0xff                 ;; test quad count
        je near .vtdone

;;
;; ebp : ystep frac. upper 24 bits
;; edx : y     frac. upper 24 bits
;; ebx : y     i.    lower 7 bits,  masked for index
;; ecx : ch = counter, cl = y step i.
;; eax : colormap aligned 256
;; esi : source texture column
;; edi : dest screen
;;
.vt4quadloop:
        mov     dh,0x7f                 ;; prep mask
        mov     ah,[esi+ebx]            ;; fetch texel : colormap number
        mov     [tystep],ebp
        add     edi,[rowbytes]
        mov     al,[edi]                ;; fetch dest  : index into colormap
        sub     edi,[rowbytes]
        mov     ebp,edi
        sub     edi,[rowbytes]
        jmp short .inloop

.vtquadloop:
        add     edx,[tystep]
        adc     bl,cl
        and     bl,dh
        add     ebp,[doublerowbytes]
        mov     dl,[eax]
        mov     ah,[esi+ebx]            ;; fetch texel : colormap number
        mov     [edi],dl
        mov     al,[ebp]                ;; fetch dest   : index into colormap
.inloop:
        add     edx,[tystep]
        adc     bl,cl
        and     bl,dh
        add     edi,[doublerowbytes]
        mov     dl,[eax]
        mov     ah,[esi+ebx]            ;; fetch texel : colormap number
        mov     [ebp+0x0],dl
        mov     al,[edi]                ;; fetch dest   : index into colormap

        add     edx,[tystep]
        adc     bl,cl
        and     bl,dh
        add     ebp,[doublerowbytes]
        mov     dl,[eax]
        mov     ah,[esi+ebx]            ;; fetch texel : colormap number
        mov     [edi],dl
        mov     al,[ebp]                ;; fetch dest   : index into colormap
        
        add     edx,[tystep]
        adc     bl,cl
        and     bl,dh
        add     edi,[doublerowbytes]
        mov     dl,[eax]
        mov     ah,[esi+ebx]            ;; fetch texel : colormap number
        mov     [ebp],dl
        mov     al,[edi]                ;; fetch dest   : index into colormap
        dec     ch
        jne     .vtquadloop
.vtdone:
        pop     ebx
        pop     edi
        pop     esi
        pop     ebp
        ret


;;----------------------------------------------------------------------
;; R_DrawShadeColumn
;;
;;   for smoke..etc.. test.
;;----------------------------------------------------------------------
[GLOBAL _R_DrawShadeColumn_8]
_R_DrawShadeColumn_8:
        push    ebp                     ;; preserve caller's stack frame pointer
        push    esi                     ;; preserve register variables
        push    edi
        push    ebx

;;
;; dest = ylookup[dc_yl] + columnofs[dc_x];
;;
        mov     ebp,[_dc_yl]
        mov     ebx,ebp
        mov     edi,[_ylookup+ebx*4]
        mov     ebx,[_dc_x]
        add     edi,[_columnofs+ebx*4]  ;; edi = dest
;;
;; pixelcount = yh - yl + 1
;;
        mov     eax,[_dc_yh]
        inc     eax
        sub     eax,ebp                 ;; pixel count
        mov     [pixelcount],eax       ;; save for final pixel
        jle near .shdone                ;; nothing to scale
;;
;; frac = dc_texturemid - (centery-dc_yl)*fracstep;
;;
        mov     ecx,[_dc_iscale]        ;; fracstep
        mov     eax,[_centery]
        sub     eax,ebp
        imul    eax,ecx
        mov     edx,[_dc_texturemid]
        sub     edx,eax
        mov     ebx,edx
        shr     ebx,16                  ;; frac int.
        and     ebx,byte +0x7f
        shl     edx,16                  ;; y frac up

        mov     ebp,ecx
        shl     ebp,16                  ;; fracstep f. up
        shr     ecx,16                  ;; fracstep i. ->cl
        and     cl,0x7f

        mov     esi,[_dc_source]
;;
;; lets rock :) !
;;
        mov     eax,[pixelcount]
        mov     dh,al
        shr     eax,2
        mov     ch,al                   ;; quad count
        mov     eax,[_colormaps]
        test    dh,3
        je      .sh4quadloop
;;
;;  do un-even pixel
;;
        test    dh,0x1
        je      .f2

        mov     ah,[esi+ebx]            ;; fetch texel : colormap number
        add     edx,ebp
        adc     bl,cl
        mov     al,[edi]                ;; fetch dest  : index into colormap
        and     bl,0x7f
        mov     dl,[eax]
        mov     [edi],dl
        add     edi,[rowbytes]
;;
;;  do two non-quad-aligned pixels
;;
.f2:
        test    dh,0x2
        je      .f3

        mov     ah,[esi+ebx]            ;; fetch texel : colormap number
        add     edx,ebp
        adc     bl,cl
        mov     al,[edi]                ;; fetch dest  : index into colormap
        and     bl,0x7f
        mov     dl,[eax]
        mov     [edi],dl
        add     edi,[rowbytes]
        
        mov     ah,[esi+ebx]            ;; fetch texel : colormap number
        add     edx,ebp
        adc     bl,cl
        mov     al,[edi]                ;; fetch dest  : index into colormap
        and     bl,0x7f
        mov     dl,[eax]
        mov     [edi],dl
        add     edi,[rowbytes]
;;
;;  test if there was at least 4 pixels
;;
.f3:
        test    ch,0xff                 ;; test quad count
        je near .shdone

;;
;; ebp : ystep frac. upper 24 bits
;; edx : y     frac. upper 24 bits
;; ebx : y     i.    lower 7 bits,  masked for index
;; ecx : ch = counter, cl = y step i.
;; eax : colormap aligned 256
;; esi : source texture column
;; edi : dest screen
;;
.sh4quadloop:
        mov     dh,0x7f                 ;; prep mask
        mov     ah,[esi+ebx]            ;; fetch texel : colormap number
        mov     [tystep],ebp
        add     edi,[rowbytes]
        mov     al,[edi]                ;; fetch dest  : index into colormap
        sub     edi,[rowbytes]
        mov     ebp,edi
        sub     edi,[rowbytes]
        jmp short .shinloop

;;    .align  4
.shquadloop:
        add     edx,[tystep]
        adc     bl,cl
        and     bl,dh
        add     ebp,[doublerowbytes]
        mov     dl,[eax]
        mov     ah,[esi+ebx]            ;; fetch texel : colormap number
        mov     [edi],dl
        mov     al,[ebp]                ;; fetch dest : index into colormap
.shinloop:
        add     edx,[tystep]
        adc     bl,cl
        and     bl,dh
        add     edi,[doublerowbytes]
        mov     dl,[eax]
        mov     ah,[esi+ebx]            ;; fetch texel : colormap number
        mov     [ebp],dl
        mov     al,[edi]                ;; fetch dest : index into colormap

        add     edx,[tystep]
        adc     bl,cl
        and     bl,dh
        add     ebp,[doublerowbytes]
        mov     dl,[eax]
        mov     ah,[esi+ebx]            ;; fetch texel : colormap number
        mov     [edi],dl
        mov     al,[ebp]                ;; fetch dest : index into colormap

        add     edx,[tystep]
        adc     bl,cl
        and     bl,dh
        add     edi,[doublerowbytes]
        mov     dl,[eax]
        mov     ah,[esi+ebx]            ;; fetch texel : colormap number
        mov     [ebp],dl
        mov     al,[edi]                ;; fetch dest : index into colormap

        dec     ch
        jne     .shquadloop

.shdone:
        pop     ebx                     ;; restore register variables
        pop     edi
        pop     esi
        pop     ebp                     ;; restore caller's stack frame pointer
        ret



;;----------------------------------------------------------------------
;;
;;      R_DrawSpan
;;
;;      Horizontal texture mapping
;;
;;----------------------------------------------------------------------


[SECTION .data]

obelix          dd      0
etaussi         dd      0

[SECTION .text]

[GLOBAL _R_DrawSpan_8]
_R_DrawSpan_8:
        push    ebp                     ;; preserve caller's stack frame pointer
        push    esi                     ;; preserve register variables
        push    edi
        push    ebx
;;
;; find loop count
;;
        mov     eax,[_ds_x2]
        inc     eax
        sub     eax,[_ds_x1]            ;; pixel count
        mov     [pixelcount],eax        ;; save for final pixel
        js near .hdone                  ;; nothing to scale
        shr     eax,0x1                 ;; double pixel count
        mov     [loopcount],eax
;;
;; build composite position
;;
        mov     ebp,[_ds_xfrac]
        shl     ebp,10
        and     ebp,0xffff0000
        mov     eax,[_ds_yfrac]
        shr     eax,6
        and     eax,0xffff
        mov     edi,[_ds_y]
        or      ebp,eax
        
        mov     esi,[_ds_source]
;;
;; calculate screen dest
;;
        mov     edi,[_ylookup+edi*4]
        mov     eax,[_ds_x1]
        add     edi,[_columnofs+eax*4]
;;
;; build composite step
;;
        mov     ebx,[_ds_xstep]
        shl     ebx,10
        and     ebx,0xffff0000
        mov     eax,[_ds_ystep]
        shr     eax,6
        and     eax,0xffff
        or      ebx,eax
        
        mov     [obelix],ebx
        mov     [etaussi],esi

;; %eax      aligned colormap
;; %ebx      aligned colormap
;; %ecx,%edx  scratch
;; %esi      virtual source
;; %edi      moving destination pointer
;; %ebp      frac

        mov     eax,[_ds_colormap]
        mov     ecx,ebp
        add     ebp,ebx                 ;; advance frac pointer
        shr     cx,10
        rol     ecx,6
        and     ecx,4095                ;; finish calculation for third pixel
        mov     edx,ebp
        shr     dx,10
        rol     edx,6
        add     ebp,ebx                 ;; advance frac pointer
        and     edx,4095                ;; finish calculation for fourth pixel
        mov     ebx,eax
        mov     al,[esi+ecx]            ;; get first pixel
        mov     bl,[esi+edx]            ;; get second pixel

        test dword [pixelcount],0xfffffffe
   
        mov     dl,[eax]                ;; color translate first pixel

;;      movw    $0xf0f0,%dx             ;;see visplanes start

        je      .hchecklast

        mov     dh,[ebx]                ;; color translate second pixel
        mov     esi,[loopcount]
.hdoubleloop:   
        mov     ecx,ebp
        shr     cx,10
        rol     ecx,6
        add     ebp,[obelix]            ;; advance frac pointer              
        mov     [edi],dx                ;; write first pixel                 
        and     ecx,4095                ;; finish calculation for third pixel
        mov     edx,ebp
        shr     dx,10
        rol     edx,6
     add     ecx,[etaussi]
        and     edx,4095                ;; finish calculation for fourth pixel
        mov     al,[ecx]                ;; get third pixel
        add     ebp,[obelix]            ;; advance frac pointer
     add     edx,[etaussi]      
        mov     bl,[edx]                ;; get fourth pixel
        mov     dl,[eax]                ;; color translate third pixel
        add     edi,byte +0x2           ;; advance to third pixel destination
        dec     esi                     ;; done with loop?
        mov     dh,[ebx]                ;; color translate fourth pixel
        jne     .hdoubleloop
;; check for final pixel
.hchecklast:
        test dword [pixelcount],0x1
        je      .hdone
        mov     [edi],dl                ;; write final pixel
.hdone: pop     ebx                     ;; restore register variables
        pop     edi
        pop     esi
        pop     ebp                     ;; restore caller's stack frame pointer
        ret



;; ========================================================================
;;  Rasterization des segments d'un polyg“ne textur‚ de maniŠre LINEAIRE.
;;  Il s'agit donc d'interpoler les coordonn‚es aux bords de la texture en
;;  mˆme temps que les abscisses minx/maxx pour chaque ligne.
;;  L'argument 'dir' indique quels bords de la texture sont interpolés:
;;    0 : segments associ‚s aux bord SUPERIEUR et INFERIEUR ( TY constant )
;;    1 : segments associ‚s aux bord GAUCHE    et DROITE    ( TX constant )
;; ========================================================================
;;
;;  void   rasterize_segment_tex( LONG x1, LONG y1, LONG x2, LONG y2, LONG tv1, LONG tv2, LONG tc, LONG dir );
;;                                   ARG1     ARG2     ARG3     ARG4      ARG5      ARG6     ARG7       ARG8
;;
;;  Pour dir = 0, (tv1,tv2) = (tX1,tX2), tc = tY, en effet TY est constant.
;;
;;  Pour dir = 1, (tv1,tv2) = (tY1,tY2), tc = tX, en effet TX est constant.
;;
;;
;;  Uses:  extern struct rastery *_rastertab;
;;

[SECTION .text]

MINX            EQU    0
MAXX            EQU    4
TX1             EQU    8
TY1             EQU    12
TX2             EQU    16
TY2             EQU    20
RASTERY_SIZEOF  EQU    24

[GLOBAL _rasterize_segment_tex]
_rasterize_segment_tex:
        push    ebp
        mov     ebp,esp

        sub     esp,byte +0x8           ;; alloue les variables locales

        push    ebx
        push    esi
        push    edi
        o16 mov ax,es
        push    eax

;;        #define DX       [ebp-4]
;;        #define TD       [ebp-8]

        mov     eax,[ebp+0xc]           ;; y1
        mov     ebx,[ebp+0x14]          ;; y2
        cmp     ebx,eax
        je near .L_finished             ;; special (y1==y2) segment horizontal, exit!

        jg near .L_rasterize_right

;;rasterize_left:       ;; on rasterize un segment … la GAUCHE du polyg“ne

        mov     ecx,eax
        sub     ecx,ebx
        inc     ecx                     ;; y1-y2+1

        mov     eax,RASTERY_SIZEOF
        mul     ebx                     ;; * y2
        mov     esi,[_prastertab]
        add     esi,eax                 ;; point into rastertab[y2]

        mov     eax,[ebp+0x8]           ;; ARG1
        sub     eax,[ebp+0x10]          ;; ARG3
        shl     eax,0x10                ;;     ((x1-x2)<<PRE) ...
        cdq
        idiv    ecx                     ;; dx =     ...        / (y1-y2+1)
        mov     [ebp-0x4],eax           ;; DX
        
        mov     eax,[ebp+0x18]          ;; ARG5
        sub     eax,[ebp+0x1c]          ;; ARG6
        shl     eax,0x10
        cdq
        idiv    ecx                     ;;      tdx =((tx1-tx2)<<PRE) / (y1-y2+1)
        mov     [ebp-0x8],eax           ;; idem tdy =((ty1-ty2)<<PRE) / (y1-y2+1)

        mov     eax,[ebp+0x10]          ;; ARG3
        shl     eax,0x10                ;; x = x2<<PRE
        
        mov     ebx,[ebp+0x1c]          ;; ARG6
        shl     ebx,0x10                ;; tx = tx2<<PRE    d0
                                        ;; ty = ty2<<PRE    d1
        mov     edx,[ebp+0x20]          ;; ARG7
        shl     edx,0x10                ;; ty = ty<<PRE     d0
                                        ;; tx = tx<<PRE     d1
        push    ebp
        mov     edi,[ebp-0x4]           ;; DX
        cmp     dword [ebp+0x24],byte +0x0      ;; ARG8   direction ?

        mov     ebp,[ebp-0x8]           ;; TD
        je      .L_rleft_h_loop
;;
;; TY varie, TX est constant
;;
.L_rleft_v_loop:   
        mov     [esi+MINX],eax           ;; rastertab[y].minx = x
          add     ebx,ebp
        mov     [esi+TX1],edx           ;;             .tx1  = tx
          add     eax,edi
        mov     [esi+TY1],ebx           ;;             .ty1  = ty

        ;;addl    DX, %eax        // x     += dx
        ;;addl    TD, %ebx        // ty    += tdy

        add     esi,RASTERY_SIZEOF      ;; next raster line into rastertab[]
        dec     ecx
        jne     .L_rleft_v_loop
        pop     ebp
        jmp     .L_finished
;;
;; TX varie, TY est constant
;;
.L_rleft_h_loop:
        mov     [esi+MINX],eax           ;; rastertab[y].minx = x 
          add     eax,edi                                        
        mov     [esi+TX1],ebx           ;;             .tx1  = tx
          add     ebx,ebp                                        
        mov     [esi+TY1],edx           ;;             .ty1  = ty
        
        ;;addl    DX, %eax        // x     += dx
        ;;addl    TD, %ebx        // tx    += tdx

        add     esi,RASTERY_SIZEOF      ;; next raster line into rastertab[]
        dec     ecx
        jne     .L_rleft_h_loop
        pop     ebp
        jmp     .L_finished
;;
;; on rasterize un segment … la DROITE du polyg“ne
;;
.L_rasterize_right:
        mov     ecx,ebx
        sub     ecx,eax
        inc     ecx                     ;; y2-y1+1

        mov     ebx,RASTERY_SIZEOF
        mul     ebx                     ;;   * y1
        mov     esi,[_prastertab]
        add     esi,eax                 ;;  point into rastertab[y1]

        mov     eax,[ebp+0x10]          ;; ARG3
        sub     eax,[ebp+0x8]           ;; ARG1
        shl     eax,0x10                ;; ((x2-x1)<<PRE) ...
        cdq
        idiv    ecx                     ;;  dx =     ...        / (y2-y1+1)
        mov     [ebp-0x4],eax           ;; DX

        mov     eax,[ebp+0x1c]          ;; ARG6
        sub     eax,[ebp+0x18]          ;; ARG5
        shl     eax,0x10
        cdq
        idiv    ecx                     ;;       tdx =((tx2-tx1)<<PRE) / (y2-y1+1)
        mov     [ebp-0x8],eax           ;;  idem tdy =((ty2-ty1)<<PRE) / (y2-y1+1)

        mov     eax,[ebp+0x8]           ;; ARG1
        shl     eax,0x10                ;; x  = x1<<PRE

        mov     ebx,[ebp+0x18]          ;; ARG5
        shl     ebx,0x10                ;; tx = tx1<<PRE    d0
                                        ;; ty = ty1<<PRE    d1
        mov     edx,[ebp+0x20]          ;; ARG7
        shl     edx,0x10                ;; ty = ty<<PRE     d0
                                        ;; tx = tx<<PRE     d1
        push    ebp
        mov     edi,[ebp-0x4]           ;; DX
        
        cmp     dword [ebp+0x24], 0     ;; direction ?

         mov     ebp,[ebp-0x8]          ;; TD
        je      .L_rright_h_loop
;;
;; TY varie, TX est constant
;;
.L_rright_v_loop:

        mov     [esi+MAXX],eax           ;; rastertab[y].maxx = x
          add     ebx,ebp
        mov     [esi+TX2],edx          ;;             .tx2  = tx
          add     eax,edi
        mov     [esi+TY2],ebx          ;;             .ty2  = ty
        
        ;;addl    DX, %eax        // x     += dx
        ;;addl    TD, %ebx        // ty    += tdy

        add     esi,RASTERY_SIZEOF
        dec     ecx
        jne     .L_rright_v_loop

        pop     ebp

        jmp     short .L_finished
;;
;; TX varie, TY est constant
;;
.L_rright_h_loop:
        mov     [esi+MAXX],eax           ;; rastertab[y].maxx = x
          add     eax,edi
        mov     [esi+TX2],ebx          ;;             .tx2  = tx
          add     ebx,ebp
        mov     [esi+TY2],edx          ;;             .ty2  = ty

        ;;addl    DX, %eax        // x     += dx
        ;;addl    TD, %ebx        // tx    += tdx

        add     esi,RASTERY_SIZEOF
        dec     ecx
        jne     .L_rright_h_loop

        pop     ebp

.L_finished:
        pop     eax
        o16 mov es,ax
        pop     edi
        pop     esi
        pop     ebx

        mov     esp,ebp
        pop     ebp
        ret


;;; this version can draw 64x64 tiles, but they would have to be arranged 4 per row,
;; so that the stride from one line to the next is 256
;;
;; .data
;;xstep         dd      0
;;ystep         dd      0
;;texwidth      dd      64              ;; texture width
;; .text
;; this code is kept in case we add high-detail floor textures for example (256x256)
;       align   16
;_R_DrawSpan_8:
;       push ebp                        ;; preserve caller's stack frame pointer
;       push esi                        ;; preserve register variables
;       push edi
;       push ebx
;;
;; find loop count
;;
;       mov eax,[_ds_x2]
;       inc eax
;       sub eax,[_ds_x1]                ;; pixel count         
;       mov [pixelcount],eax            ;; save for final pixel
;       js near .hdone                  ;; nothing to scale    
;;
;; calculate screen dest
;;
;       mov edi,[_ds_y]
;       mov edi,[_ylookup+edi*4]
;       mov eax,[_ds_x1]
;       add edi,[_columnofs+eax*4]
;;
;; prepare registers for inner loop
;;
;       xor eax,eax
;       mov edx,[_ds_xfrac]
;       ror edx,16
;       mov al,dl
;       mov ecx,[_ds_yfrac]
;       ror ecx,16
;       mov ah,cl
;       
;       mov ebx,[_ds_xstep]
;       ror ebx,16
;       mov ch,bl
;       and ebx,0xffff0000
;       mov [xstep],ebx
;       mov ebx,[_ds_ystep]
;       ror ebx,16
;       mov dh,bl
;       and ebx,0xffff0000
;       mov [ystep],ebx
;
;       mov esi,[_ds_source]
;
;;; %eax      Yi,Xi in %ah,%al
;;; %ebx      aligned colormap
;;; %ecx      Yfrac upper, dXi in %ch, %cl is counter (upto 1024pels, =4x256)
;;; %edx      Xfrac upper, dYi in %dh, %dl receives mapped pixels from (ebx)
;;;  ystep    dYfrac, add to %ecx, low word is 0
;;;  xstep    dXfrac, add to %edx, low word is 0
;;; %ebp      temporary register serves as offset like %eax
;;; %esi      virtual source
;;; %edi      moving destination pointer
;
;       mov ebx,[pixelcount]
;       shr ebx,0x2                     ;; 4 pixels per loop
;       test bl,0xff
;       je near .hchecklast
;       mov cl,bl
;       
;       mov ebx,[_dc_colormap]
;;;
;;; prepare loop with first pixel
;;;
;       add ecx,[ystep]                 ;;pr‚a1
;       adc ah,dh
;       add edx,[xstep]
;       adc al,ch
;       and eax,0x3f3f
;       mov bl,[esi+eax]                ;;pr‚b1
;       mov dl,[ebx]                    ;;pr‚c1
;
;       add ecx,[ystep]                 ;;a2
;        adc ah,dh
;
;.hdoubleloop:
;       mov [edi+1],dl
;        add edx,[xstep]
;       adc al,ch
;        add edi,byte +0x2
;       mov ebp,eax
;        add ecx,[ystep]
;       adc ah,dh
;        and ebp,0x3f3f
;       add edx,[xstep]
;        mov bl,[esi+ebp]
;       adc al,ch
;        mov dl,[ebx]
;       and eax,0x3f3f
;        mov [edi],dl
;       mov bl,[esi+eax]
;        add ecx,[ystep]
;       adc ah,dh
;        add edx,[xstep]
;       adc al,ch
;        mov dl,[ebx]
;       mov ebp,eax
;        mov [edi+1],dl
;       and ebp,0x3f3f
;        add ecx,[ystep]
;       adc ah,dh
;        mov bl,[esi+ebp]
;       add edi,byte +0x2
;        add edx,[xstep]
;       adc al,ch
;        mov dl,[ebx]
;       and eax,0x3f3f
;        mov [edi],dl
;       mov bl,[esi+eax]
;        add ecx,[ystep]
;       adc ah,dh
;        mov dl,[ebx]
;       dec cl
;        jne near .hdoubleloop
;;; check for final pixel
;.hchecklast:
;;; to do
;.hdone:
;       pop ebx
;       pop edi
;       pop esi
;       pop ebp
;       ret
