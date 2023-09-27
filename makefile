TARGET := output/sbemu.exe
CC := i586-pc-msdosdjgpp-gcc
CXX := i586-pc-msdosdjgpp-g++
DEBUG ?= 0

VERSION ?= $(shell git describe --tags)

INCLUDES := -I./mpxplay -I./sbemu
DEFINES := -D__DOS__ -DSBEMU -DDEBUG=$(DEBUG) -DMAIN_SBEMU_VER=\"$(VERSION)\"
CFLAGS := -fcommon -march=i386 -Os $(INCLUDES) $(DEFINES)
LDFLAGS := -lstdc++ -lm

ifeq ($(DEBUG),0)
LDFLAGS += -s
endif

ifeq ($(V),1)
SILENTCMD :=
SILENTMSG := @true
else
SILENTCMD := @
SILENTMSG := @printf
endif

VPATH += .
VPATH += sbemu
VPATH += sbemu/dpmi

all: $(TARGET)

CARDS_SRC := mpxplay/au_cards/ac97_def.c \
	     mpxplay/au_cards/au_cards.c \
	     mpxplay/au_cards/dmairq.c \
	     mpxplay/au_cards/pcibios.c \
	     mpxplay/au_cards/sc_e1371.c \
	     mpxplay/au_cards/sc_ich.c \
	     mpxplay/au_cards/sc_inthd.c \
	     mpxplay/au_cards/sc_sbl24.c \
	     mpxplay/au_cards/sc_sbliv.c \
	     mpxplay/au_cards/sc_via82.c \

MIXER_SRC := mpxplay/au_mixer/cv_bits.c \
	     mpxplay/au_mixer/cv_chan.c \
	     mpxplay/au_mixer/cv_freq.c \

NEWFUNC_SRC := mpxplay/newfunc/fpu.c \
	       mpxplay/newfunc/memory.c \
	       mpxplay/newfunc/nf_dpmi.c \
	       mpxplay/newfunc/string.c \
	       mpxplay/newfunc/threads.c \
	       mpxplay/newfunc/time.c \
	       mpxplay/newfunc/timer.c \

SBEMU_SRC := sbemu/dbopl.cpp \
	     sbemu/opl3emu.cpp \
	     sbemu/pic.c \
	     sbemu/sbemu.c \
	     sbemu/untrapio.c \
	     sbemu/vdma.c \
	     sbemu/virq.c \
	     sbemu/dpmi/xms.c \
	     sbemu/dpmi/dpmi.c \
	     sbemu/dpmi/dbgutil.c \
	     sbemu/dpmi/dpmi_dj2.c \
	     sbemu/dpmi/dpmi_tsr.c \
	     sbemu/dpmi/gormcb.c \
	     main.c \
	     qemm.c \
	     test.c \
	     utility.c \
	     hdpmipt.c \

SRC := $(CARDS_SRC) $(MIXER_SRC) $(NEWFUNC_SRC) $(SBEMU_SRC)
OBJS := $(patsubst %.cpp,output/%.o,$(patsubst %.c,output/%.o,$(SRC)))

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(SILENTMSG) "LINK\t$@\n"
	$(SILENTCMD)$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

output/%.o: %.c
	@mkdir -p $(dir $@)
	$(SILENTMSG) "CC\t$@\n"
	$(SILENTCMD)$(CC) $(CFLAGS) -c $< -o $@

output/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(SILENTCMD)$(SILENTMSG) "CXX\t$@\n"
	$(SILENTCMD)$(CXX) $(CFLAGS) -c $< -o $@

clean:
	$(SILENTMSG) "CLEAN\n"
	$(SILENTCMD)$(RM) $(OBJS)

distclean: clean
	$(SILENTMSG) "DISTCLEAN\n"
	$(SILENTCMD)$(RM) $(TARGET)
