#
# Makefile for highwire
#
TARGET = ../mintnet.ovl
BUILDDIR = ../../build/network

# compiler settings
CROSS = m68k-atari-mint-
CC = $(CROSS)gcc -g #-DDEBUG
AS = $(CC) -c
LD = $(CC) 
CP = cp
RM = rm -f

CPU = 68000
#CPU = 68030
#CPU = 68040
#CPU = 68020-60
#CPU = 5475

DEFS = -DUSE_INET

OPTFLAGS = -funsigned-char \
       -fomit-frame-pointer -O2 -fstrength-reduce 

ifeq ($(CPU),5475)
	OPTS = $(CPU:%=-mcpu=%) $(OPTFLAGS)
else
	OPTS = $(CPU:%=-m%) $(OPTFLAGS)
endif

WARN = \
	-Wall \
	-Wmissing-prototypes \
	-Wshadow \
	-Wpointer-arith \
	-Wcast-qual

INCLUDE =

CFLAGS = $(INCLUDE) $(WARN) $(OPTS) $(DEFS)
ASFLAGS = $(OPTS)
LDFLAGS = -s
LIBS = -liio

ifeq ($(CPU),5475)
        OBJDIR = $(BUILDDIR)/obj.$(CPU)
else
	OBJDIR = $(BUILDDIR)/obj$(CPU:68%=.%)
endif
DEPDIR = $(BUILDDIR)/.deps

all: $(TARGET)

#
# C source files
#
$(OBJDIR)/%.o: %.c
	@echo "$(CC) $(CFLAGS) -c $< -o $@"; \
	$(CC) -Wp,-MMD,$(DEPDIR)/$(<:.c=.P_) $(CFLAGS) -c $< -o $@
	@cat $(DEPDIR)/$(<:.c=.P_) \
	    | sed "s,^\(.*\)\.o:,$(OBJDIR)/\1.o:," > $(DEPDIR)/$(<:.c=.P)
	@rm -f $(DEPDIR)/$(<:.c=.P_)

#
# files
#
CFILES = network.c

HDR = 

SFILES = 

OBJS = $(SFILES:%.s=$(OBJDIR)/%.o) $(CFILES:%.c=$(OBJDIR)/%.o)
OBJS_MAGIC := $(shell mkdir -p $(OBJDIR) > /dev/null 2>&1 || :)

DEPENDENCIES = $(addprefix $(DEPDIR)/, $(patsubst %.c,%.P,$(CFILES)))


$(TARGET): $(OBJS)
	$(LD) -o $@ $(CFLAGS) -Wl,-stack,8k $(LDFLAGS) $(OBJS) $(LIBS)

000: ; $(MAKE) CPU=68000
030: ; $(MAKE) CPU=68030
040: ; $(MAKE) CPU=68040
060: ; $(MAKE) CPU=68020-60
v4e: ; $(MAKE) CPU=5475

# The other stacks.  Each wrapper names its flavour and includes network.c,
# so they share every rule above; only iconnect still needs Pure C, its
# bundled headers fighting the ones the cross compiler brings.
#
# sting and stik2 talk to the stack purely through its trap interface and
# need no sockets, so they link against libcmini and come out at a twentieth
# of the size mintlib gives them.  mintnet and magicnet need mintlib's
# socket layer and stay on it.  libcmini comes with the toolkit container.
CMINI = /freemint/libcmini/lib

sting:    ; $(MAKE) TARGET=../sting.ovl    CFILES=sting.c    INCLUDE=-Iinclude \
                    LDFLAGS="-s -nostdlib $(CMINI)/crt0.o" \
                    LIBS="-L$(CMINI) -lcmini -lgcc -lcmini"
stik2:    ; $(MAKE) TARGET=../stik2.ovl    CFILES=stik2.c    INCLUDE=-Iinclude \
                    LDFLAGS="-s -nostdlib $(CMINI)/crt0.o" \
                    LIBS="-L$(CMINI) -lcmini -lgcc -lcmini"
magicnet: ; $(MAKE) TARGET=../magicnet.ovl CFILES=magicnet.c INCLUDE=-Iinclude

clean:
	$(RM) $(OBJS)

distclean:
	$(MAKE) clean;
	$(RM) *~


#
# adjust file names
#
$(CFILES) $(HDR) Makefile: ; mv `echo $@ | tr A-Z a-z` $@
case: Makefile $(HDR) $(CFILES)


#
# dependencies
#
DEPS_MAGIC := $(shell mkdir -p $(DEPDIR) > /dev/null 2>&1 || :)

-include $(DEPENDENCIES)
