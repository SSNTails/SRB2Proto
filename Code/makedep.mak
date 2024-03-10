#############################################################################
#
# $Id:$
#     GNU Make makefile for PC DOS/win95 version
#                                      for Doom LEGACY (bpereira@ulb.ac.be)
#
# $Log:$
#
#     -DUSEASM -> use assembly routines instead of C ones, where possible.
#     -DPC_DOS -> use dos specific code (eg:textmode stuff)... win95 ok.
#

# gcc or g++
CC=gcc

#if use PGCC
PGCC=1

WFLAGS=-Wall # -W -Wno-unused -Wno-sign-compare

#indicate platform and what interface use with
# posible value for now : DJGPPDOS
#                         LINUX      (not implemented)
DJGPPDOS=1
#LINUX=1

#determine the interface directory (where you put all i_*.c)
ifdef DJGPPDOS
       INTERFACE=DJGPPDOS
else
       ifdef LINUX
             INTERFACE=LINUX
       endif
endif

OPTS=-DPC_DOS -DUSEASM -MMD depend.mak

CFLAGS = -g $(OPTS)

DEBUGLIBS=-lalleg -lbcd -lsocke
LIBS=-lalleg -lbcd -lsocke -s

SFLAGS=

# name of the exefile
EXENAME=DOOM3

# subdirectory for objects
O=OBJS

# not too sophisticated dependency
OBJS=   \
                $(O)/dstrings.o         \
                $(O)/i_sound.o          \
                $(O)/i_cdmus.o          \
                $(O)/i_video.o          \
                $(O)/i_net.o            \
                $(O)/i_tcp.o            \
                $(O)/i_system.o         \
                $(O)/tables.o           \
                $(O)/f_finale.o         \
                $(O)/f_wipe.o           \
                $(O)/d_main.o           \
                $(O)/d_net.o            \
                $(O)/d_items.o          \
                $(O)/g_game.o           \
                $(O)/m_menu.o           \
                $(O)/m_misc.o           \
                $(O)/m_argv.o           \
                $(O)/m_bbox.o           \
                $(O)/m_fixed.o          \
                $(O)/m_swap.o           \
                $(O)/m_cheat.o          \
                $(O)/m_random.o         \
                $(O)/am_map.o           \
                $(O)/p_ceilng.o         \
                $(O)/p_doors.o          \
                $(O)/p_enemy.o          \
                $(O)/p_floor.o          \
                $(O)/p_inter.o          \
                $(O)/p_lights.o         \
                $(O)/p_map.o            \
                $(O)/p_maputl.o         \
                $(O)/p_plats.o          \
                $(O)/p_pspr.o           \
                $(O)/p_setup.o          \
                $(O)/p_sight.o          \
                $(O)/p_spec.o           \
                $(O)/p_switch.o         \
                $(O)/p_mobj.o           \
                $(O)/p_telept.o         \
                $(O)/p_tick.o           \
                $(O)/p_saveg.o          \
                $(O)/p_user.o           \
                $(O)/r_bsp.o            \
                $(O)/r_data.o           \
                $(O)/r_draw.o           \
                $(O)/r_main.o           \
                $(O)/r_plane.o          \
                $(O)/r_segs.o           \
                $(O)/r_sky.o            \
                $(O)/r_things.o         \
                $(O)/w_wad.o            \
                $(O)/wi_stuff.o         \
                $(O)/v_video.o          \
                $(O)/st_lib.o           \
                $(O)/st_stuff.o         \
                $(O)/hu_stuff.o         \
                $(O)/s_sound.o          \
                $(O)/z_zone.o           \
                $(O)/info.o             \
                $(O)/sounds.o   \
                $(O)/tmap.o     \
                $(O)/p_fab.o    \
                $(O)/dehacked.o \
                $(O)/qmus2mid.o \
                $(O)/vid_copy.o \
                $(O)/vid_vesa.o \
                $(O)/g_input.o  \
                $(O)/screen.o   \
                $(O)/console.o  \
                $(O)/command.o  \
                $(O)/d_netcmd.o \
                $(O)/d_clisrv.o \
                $(O)/r_splats.o \
                $(O)/g_state.o  \
                $(O)/d_netfil.o



all:     $(O)/$(EXENAME)

clean:
ifdef LINUX
        rm -f *.o *~ *.flc
        rm -f $(O)/*
else
        del $(O)\*.* >NUL
endif

# executable

$(O)/$(EXENAME):        $(OBJS) $(O)/i_main.o
        @Echo done


# in all other case
$(O)/%.o : %.c
        $(CC) $(CFLAGS) $(LDFLAGS) $(WFLAGS) -c $<

$(O)/%.o : %.S
        $(CC) $(OPTS) $(SFLAGS) -x assembler-with-cpp -c $<

$(O)/%.o : DJGPPDOS/%.S
        $(CC) $(OPTS) $(SFLAGS) -x assembler-with-cpp -c $<

$(O)/%.o : DJGPPDOS/%.c
        $(CC) $(CFLAGS) $(LDFLAGS) $(WFLAGS) -c $<

#############################################################
#
#############################################################
