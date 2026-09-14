#
# Makefile for highwire
#
TARGET = highwire.prg
DISTDIR = dist

# compiler settings

CROSS = m68k-atari-mint-
CC = $(CROSS)gcc -g #-DDEBUG
AS = $(CC) -c
LD = $(CC) 
CP = cp
RM = rm -f

# The toolkit container has no zip, and neither it nor the CI runner has
# unix2dos -- which is why the docs shipped with Unix line endings for so long.
# So use what the machine actually has.  perl is on all of them and its form is
# idempotent, leaving an already converted file untouched; python3's zipfile
# writes the same entries with the same CRCs as zip -r, and takes its arguments
# in the same order, so either can fill $(ZIP).
TODOS = perl -pi -e 's/\r?\n/\r\n/'
ZIP := $(shell command -v zip >/dev/null 2>&1 && echo 'zip -r' || echo 'python3 -m zipfile -c')

CPU = 68000
#CPU = 68030
#CPU = 68040
#CPU = 68020-60
#CPU = 5475

DEFS = -DUSE_OVL -DUSE_INET -DLIBPNG -DLIBGIF -DLIBJPG
OPTFLAGS = -funsigned-char \
       -fomit-frame-pointer -O2 -fstrength-reduce 

ifeq ($(CPU),5475)
	OPTS = $(CPU:%=-mcpu=%) $(OPTFLAGS)
else
	OPTS = $(CPU:%=-m%) $(OPTFLAGS)
endif

# -m68020 and up select the m68020-60 multilib, which was built for a hardware
# FPU.  Its startup code stops with "This program requires a 68881 or higher
# arithmetic coprocessor" on a machine that hasn't got one, and a 68030 has no
# FPU of its own -- a Falcon or TT only has one if a 68882 was fitted, and most
# Falcons never were.  So the 030 build links the plain 68000 libraries instead.
# HighWire does no floating point arithmetic of its own, so nothing is lost
# except speed: the C library is then 68000 code and calls software multiply and
# divide helpers, which costs about 20% on page rendering (measured under Hatari
# on a Falcon and a TT, both with a 68882 fitted).
# -msoft-float alone will not do it, as there is no soft-float 68020 multilib
# for it to select, and libgcc has to come from the same set or libpng's gamma
# code still reaches a hardware __fixunsdfsi.
#
#   FPU=1  hardware floating point libraries (faster, needs an FPU)
#   FPU=0  soft ones -- for a 68LC040 or 68EC060, which have no FPU either
#
# Only the link is affected, so the two 030 builds share their object files and
# the second one is just a relink.  dist ships both: highwire.030 runs on any
# 030, highwire.03F is the faster one for a TT or a Falcon with a 68882.
#
ifeq ($(CPU),68030)
FPU ?= 0
else
FPU ?= 1
endif

ifneq ($(CPU),5475)
ifeq ($(FPU),0)
SOFTFLOAT := -L$(dir $(shell $(CC) -m68000 -print-file-name=libc.a)) \
             -L$(dir $(shell $(CC) -m68000 -print-libgcc-file-name))
endif
endif

DISABLED_WARNINGS = -Wno-deprecated-declarations
WARN = \
	-Wall \
	-Wmissing-prototypes \
	-Wshadow \
	-Wpointer-arith \
	-Wcast-qual \
	$(DISABLED_WARNINGS) \
	-Werror 


INCLUDE = 

# The toolkit image ships without the image libraries.  A sysroot unpacked
# into .crosslibs/ supplies them; take the multilib that matches the link.
CROSSLIBS := $(wildcard .crosslibs/usr/m68k-atari-mint/sys-root/usr)
ifneq ($(CROSSLIBS),)
ifeq ($(FPU),0)
MULTIDIR := .
else
MULTIDIR := $(shell $(CC) $(OPTS) -print-multi-directory)
endif
INCLUDE += -I$(CROSSLIBS)/include -L$(CROSSLIBS)/lib/$(MULTIDIR)
endif

hash = \#
CHECKGIF := $(shell if echo -e "$(hash)include <gif_lib.h> \\nconst char *version = GIF_LIB_VERSION" | $(CC) $(INCLUDE) -E - | grep GIF_LIB_VERSION >/dev/null; then echo -lgif; else echo -lungif; fi)

CFLAGS = $(INCLUDE) $(WARN) $(OPTS) $(DEFS)
ASFLAGS = $(OPTS)
LDFLAGS = -s
LIBS = $(SOFTFLOAT) -lgem -lcflib -liio $(CHECKGIF) -ljpeg -lpng -lz -lm \
       #-lsocket

ifeq ($(CPU),5475)
        OBJDIR = obj.$(CPU)
else
	OBJDIR = obj$(CPU:68%=.%)
endif

# Dependency files record which OBJDIR they belong to, so they have to live
# per CPU target too.  Shared, they would silently stop applying as soon as
# you built for a second CPU, and a header change would then be missed.
DEPDIR = $(OBJDIR)/.deps

# The sources live in src/, so make looks for them there.  The file lists below
# stay as bare names, which is also what the Pure C project files in src/ want.
VPATH = src

# The default build links straight into dist/, ready to copy across.
all: $(DISTDIR)/$(TARGET)

#
# C source files
#
# Makefile is a prerequisite so that changing the flags, the CPU mapping or the
# dependency layout rebuilds rather than quietly reusing stale objects.
$(OBJDIR)/%.o: %.c Makefile
	@echo "$(CC) $(CFLAGS) -c $< -o $@"; \
	$(CC) -Wp,-MMD,$(DEPDIR)/$*.P_ $(CFLAGS) -c $< -o $@
	@sed "1s,^[^:]*:,$@:," \
	     $(DEPDIR)/$*.P_ > $(DEPDIR)/$*.P
	@rm -f $(DEPDIR)/$*.P_

#
# files
#
CFILES = \
	Logging.c \
	schedule.c \
	mime.c \
	ovl_sys.c \
	inet.c \
	http.c \
	cache.c \
	Location.c \
	cookie.c \
	DomBox.c \
	O_Struct.c \
	fontbase.c \
	W_Struct.c \
	raster.c \
	image.c \
	img-dcdr.c \
	Paragrph.c \
	list.c \
	Form.c \
	Table.c \
	Frame.c \
	color.c \
	encoding.c \
	scanner.c \
	parser.c \
	p_about.c \
	p_dir.c \
	render.c \
	Containr.c \
	Loader.c \
	Redraws.c \
	clipbrd.c \
	Window.c \
	formwind.c \
	fntsetup.c \
	dl_mngr.c \
	Widget.c \
	hwWind.c \
	av_prot.c \
	dragdrop.c \
	olga.c \
	config.c \
	bookmark.c \
	Variable.c \
	Nice_VDI.c \
	romvdi.c \
	keyinput.c \
	Mouse_R.c \
	AEI.c \
	HighWire.c \
	strtools.c \
#	mem-diag.c

HDR = hwWind.h Loader.h Containr.h Table.h Location.h Logging.h Form.h

SFILES = 

OBJS = $(SFILES:%.s=$(OBJDIR)/%.o) $(CFILES:%.c=$(OBJDIR)/%.o)
OBJS_MAGIC := $(shell mkdir ./$(OBJDIR) > /dev/null 2>&1 || :)

DEPENDENCIES = $(addprefix ./$(DEPDIR)/, $(patsubst %.c,%.P,$(CFILES)))


$(TARGET) $(DISTDIR)/$(TARGET): $(OBJS)
	mkdir -p $(@D)
	$(LD) -o $@ -Wl,-stack,128k -Wl,--mprg-flags=0x17 $(CFLAGS) $(LDFLAGS) $(OBJS) $(LIBS)

000: ; $(MAKE) CPU=68000
030: ; $(MAKE) CPU=68030
03f: ; $(MAKE) CPU=68030 FPU=1
040: ; $(MAKE) CPU=68040
060: ; $(MAKE) CPU=68020-60
v4e: ; $(MAKE) CPU=5475

clean:
	rm -Rf *.bak */*.bak */*/*.bak *[%~] */*[%~] */*/*[%~]
	rm -Rf obj.* */obj.* */*/obj.* .deps */.deps */*/.deps *.o */*/*.o
	rm -Rf *.app *.[gt]tp *.prg modules/mintnet.ovl

distclean: clean


#
# distribution/snapshot archive
#
dist::
	$(MAKE) clean
	$(MAKE) CPU=68000 $(TARGET)
	mkdir -p $(DISTDIR)
	mv $(TARGET) $(DISTDIR)/highwire.000
	$(MAKE) CPU=68030 $(TARGET)
	mv $(TARGET) $(DISTDIR)/highwire.030
	$(MAKE) CPU=68030 FPU=1 $(TARGET)
	mv $(TARGET) $(DISTDIR)/highwire.03F
	$(MAKE) CPU=68040 $(TARGET)
	mv $(TARGET) $(DISTDIR)/highwire.040
	$(MAKE) CPU=68020-60 $(TARGET)
	mv $(TARGET) $(DISTDIR)/highwire.060
	$(MAKE) CPU=5475 $(TARGET)
	mv $(TARGET) $(DISTDIR)/highwire.v4e
	cp -a deskicon.rsc highwire.rsc $(DISTDIR)
	mkdir -p $(DISTDIR)/doc
	cp -a doc/HIGHWIRE.DOC doc/hotkeys.txt $(DISTDIR)/doc
	cp -a Change.Log $(DISTDIR)
	mkdir -p $(DISTDIR)/html
	cp -pvr html/. $(DISTDIR)/html
	mkdir -p $(DISTDIR)/modules
	$(MAKE) -C modules/network.src clean
	$(MAKE) -C modules/network.src CPU=5475
	cp -a modules/mintnet.ovl $(DISTDIR)/modules/mintnet.v4e
	$(MAKE) -C modules/network.src CPU=68000
	cp -a modules/mintnet.ovl $(DISTDIR)/modules
	cp -a modules/README.TXT modules/iconnect.ovl modules/magicnet.ovl modules/stik2.ovl modules/sting.ovl $(DISTDIR)/modules
	mkdir -p $(DISTDIR)/example.cfg
	cp -a example.cfg/highwire.cfg $(DISTDIR)/example.cfg
#	Ready to run on the machine most people have, without renaming anything
#	first: the 68000 build as HIGHWIRE.PRG, and STinG as the network module.
#	Both are copies, so the other builds and stacks are still there to swap in.
	cp -a $(DISTDIR)/highwire.000 $(DISTDIR)/highwire.prg
	cp -a modules/sting.ovl $(DISTDIR)/modules/network.ovl
	$(TODOS) $(DISTDIR)/doc/HIGHWIRE.DOC $(DISTDIR)/doc/hotkeys.txt $(DISTDIR)/modules/README.TXT $(DISTDIR)/Change.Log $(DISTDIR)/example.cfg/highwire.cfg
	(cwd=`pwd`; cd $(DISTDIR); $(ZIP) "$$cwd"/hw`date +%y%m%d`.zip .)

#
# dependencies
#
DEPS_MAGIC := $(shell mkdir -p ./$(DEPDIR) > /dev/null 2>&1 || :)

-include $(DEPENDENCIES)
