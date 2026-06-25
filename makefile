TARGET := output/sbemu.exe
CC := i586-pc-msdosdjgpp-gcc
AS := i586-pc-msdosdjgpp-as
CXX := i586-pc-msdosdjgpp-g++
DEBUG ?= 0

VERSION ?= $(shell git describe --tags)

ALSA_DEFINES := -D__KERNEL__ -D__DISABLE_EXPORTS -DCONFIG_X86_32 \
				-DCONFIG_FLATMEM -DCONFIG_ARCH_HAS_PTE_SPECIAL -DCONFIG_HAVE_ARCH_THREAD_STRUCT_WHITELIST \
				-DCONFIG_ARCH_HAS_MEM_ENCRYPT -DCONFIG_THREAD_INFO_IN_TASK \
				-DCONFIG_KVFREE_RCU_BATCHED -DCONFIG_PAGE_SHIFT=12 -DCONFIG_HAVE_MOD_ARCH_SPECIFIC \
				-DCONFIG_HAVE_ARCH_WITHIN_STACK_FRAMES -DCONFIG_ARCH_HAS_CACHE_LINE_SIZE \
				-DCONFIG_TINY_SRCU -DCONFIG_TINY_RCU -DCONFIG_X86_L1_CACHE_SHIFT=6 \
				-DCONFIG_ARCH_WANT_BATCHED_UNMAP_TLB_FLUSH \
				-DKBUILD_MODNAME="\"\"" -DKBUILD_MODFILE="\"\"" \
				-DCONFIG_OF \
				-DCONFIG_IRQ_DOMAIN \
				-DCONFIG_SERIAL_DEV_BUS \
				-DCONFIG_REGMAP \
				-DCONFIG_ACPI \
				-DCONFIG_PCI \
				-DCONFIG_I2C \
				-DCONFIG_GPIOLIB_LEGACY \
				-DCONFIG_GPIOLIB \
				-DCONFIG_SND_PROC_FS \
				-DCONFIG_SND_JACK \
				-DCONFIG_SOUNDWIRE \
				-DCONFIG_SND_SEQUENCER \
				-DCONFIG_SND_SEQ_UMP \
				-DCONFIG_SND_SEQUENCER_OSS \
				-DCONFIG_SND_HDA_INPUT_BEEP \
				-DCONFIG_SND_HDA_INPUT_BEEP_MODE=1 \
				-DCONFIG_SND_HDA_HWDEP \
				-DCONFIG_SND_HDA_PREALLOC_SIZE=64 \
				-DCONFIG_SND_HDA_COMPONENT \
				-DCONFIG_SND_INTEL_NHLT \
				-DCONFIG_SND_INTEL_DSP_CONFIG \
				-DCONFIG_SND_HDA_I915 \
				-DCONFIG_SND_CS46XX_NEW_DSP \
				-DCONFIG_OLPC \
				-DCONFIG_SND_SOC_ACPI_AMD_SDCA_QUIRKS \
				-DCONFIG_SND_ATMEL_SOC_DMA \
				-DCONFIG_SND_ATMEL_SOC_PDC \
				-DCONFIG_SND_SOC_AC97_BUS \
				-DCONFIG_INPUT \
				-DCONFIG_SND_SOC_MT6359_ACCDET \
				-DCONFIG_SND_SOC_RT5575_SPI \
				-DCONFIG_SND_SOC_RT5677_SPI \
				-DCONFIG_SND_SOC_WCD_MBHC \
				-DCONFIG_SND_DESIGNWARE_PCM \
				-DCONFIG_SND_SOC_TOPOLOGY \
				-DCONFIG_SND_SOC_QCOM_OFFLOAD_UTILS \
				-DCONFIG_SND_SOC_SDCA_FDL \
				-DCONFIG_SND_SOC_SDCA \
				-DCONFIG_SND_SOC_SDCA_HID \
				-DCONFIG_SND_SOC_COMPRESS \
				-DCONFIG_SND_SOC_SOF_COMPRESS \
				-DCONFIG_SND_SOF_SOF_HDA_SDW_BPT \
				-DCONFIG_SND_SOC_SOF_HDA_PROBES \
				-DCONFIG_SND_SOC_TI_EDMA_PCM \
				-DCONFIG_SND_SOC_TI_SDMA_PCM \
				-DCONFIG_SND_SOC_TI_UDMA_PCM \
				-DCONFIG_AC97_BUS_NEW=1 \
				-DCONFIG_PAGE_OFFSET=0x400000

ALSA_INCLUDES:= -I. -I./alsa_wrapper/include -I./alsa_wrapper/include/generated \
				-I./alsa -I./alsa/include -I./alsa/include/uapi -I./alsa/arch/x86/include -I./alsa/arch/x86/include/uapi \
				-I./alsa/sound/hda/common -I./alsa/sound/hda/controllers -I./alsa/sound/hda/core
				

ALSA_CFLAGS:= -Wno-address-of-packed-member -fplan9-extensions

INCLUDES := -I./sbemu $(ALSA_INCLUDES)
DEFINES := $(ALSA_DEFINES) -DSBEMU -DDEBUG=$(DEBUG) -DMAIN_SBEMU_VER=\"$(VERSION)\"
CFLAGS := -fcommon -march=i386 -ffast-math -O2 -flto $(ALSA_CFLAGS) $(INCLUDES) $(DEFINES)
LDFLAGS := -lstdc++ -lm -Wno-attributes

ifeq ($(DEBUG),0)
LDFLAGS += -s
CFLAGS += -DNDEBUG
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

ALSA_SRC := $(shell find ./alsa -type f -name "*.c")
ALSA_SRC := $(shell find . -type f -name "*.c")

#some .c files are included in another .c file, thus excluded in build.
ALSA_SRC_EXCLUDE := ./alsa/sound/aoa/core/gpio-pmf.c \
					./alsa/sound/aoa/soundbus/i2sbus/control.c \
					./alsa/sound/aoa/soundbus/i2sbus/core.c \
					./alsa/sound/aoa/soundbus/i2sbus/pcm.c \
					./alsa/sound/aoa/codecs/tas.c \
					./alsa/sound/aoa/codecs/onyx.c \
					./alsa/sound/core/control_compat.c \
					./alsa/sound/core/hwdep_compat.c \
					./alsa/sound/core/rawmidi_compat.c \
					./alsa/sound/core/seq/seq_compat.c \
					./alsa/sound/core/timer_compat.c \
					./alsa/sound/core/pcm_compat.c \
					./alsa/sound/core/pcm_drm_eld.c \
					./alsa/sound/core/pcm_timer.c \
					./alsa/sound/core/isadma.c \
					./alsa/sound/core/info_oss.c \
					./alsa/sound/core/sound_oss.c \
					./alsa/sound/core/sound_kunit.c \
					./alsa/sound/hda/codecs/side-codecs/cirrus_scodec_test.c \
					./alsa/sound/pci/ac97/ac97_patch.c \
					./alsa/sound/pci/au88x0/au88x0_core.c \
					./alsa/sound/pci/au88x0/au88x0_pcm.c \
					./alsa/sound/pci/au88x0/au88x0_mixer.c \
					./alsa/sound/pci/au88x0/au88x0_mpu401.c \
					./alsa/sound/pci/au88x0/au88x0_game.c \
					./alsa/sound/pci/au88x0/au88x0_eq.c \
					./alsa/sound/pci/au88x0/au88x0_a3d.c \
					./alsa/sound/pci/au88x0/au88x0_xtalk.c \
					./alsa/sound/pci/au88x0/au88x0.c \
					./alsa/sound/pci/au88x0/au88x0_eqdata.c \
					./alsa/sound/pci/au88x0/au88x0_synth.c \
					./alsa/sound/pci/au88x0/au88x0_a3ddata.c \
					./alsa/sound/pci/echoaudio/darla20_dsp.c \
					./alsa/sound/pci/echoaudio/darla24_dsp.c \
					./alsa/sound/pci/echoaudio/echo3g_dsp.c \
					./alsa/sound/pci/echoaudio/echoaudio_3g.c \
					./alsa/sound/pci/echoaudio/echoaudio_dsp.c \
					./alsa/sound/pci/echoaudio/echoaudio.c \
					./alsa/sound/pci/echoaudio/gina20_dsp.c \
					./alsa/sound/pci/echoaudio/gina24_dsp.c \
					./alsa/sound/pci/echoaudio/echoaudio_gml.c \
					./alsa/sound/pci/echoaudio/midi.c \
					./alsa/sound/pci/echoaudio/indigodj_dsp.c \
					./alsa/sound/pci/echoaudio/indigodjx_dsp.c \
					./alsa/sound/pci/echoaudio/indigoio_dsp.c \
					./alsa/sound/pci/echoaudio/indigoiox_dsp.c \
					./alsa/sound/pci/echoaudio/indigo_express_dsp.c \
					./alsa/sound/pci/echoaudio/indigo_dsp.c \
					./alsa/sound/pci/echoaudio/layla20_dsp.c \
					./alsa/sound/pci/echoaudio/layla24_dsp.c \
					./alsa/sound/pci/echoaudio/mia_dsp.c \
					./alsa/sound/pci/echoaudio/mona_dsp.c \
					./alsa/sound/pci/lola/lola_proc.c \
					./alsa/sound/pci/nm256/nm256_coef.c \
					./alsa/sound/soc/codecs/cs-amp-lib-test.c \
					./alsa/sound/soc/codecs/cs35l56-test.c \
					./alsa/drivers/soundwire/debugfs.c \
					./alsa/sound/soc/codecs/rt1320-sdw.c \
					./alsa/sound/soc/codecs/wcd937x-sdw.c \
					./alsa/sound/soc/codecs/wcd938x-sdw.c \
					./alsa/sound/soc/codecs/wcd939x-sdw.c \
					./alsa/sound/soc/codecs/wm_adsp_fw_find_test.c \
					./alsa/sound/soc/intel/avs/debugfs.c \
					./alsa/sound/soc/intel/avs/probes.c \
					./alsa/sound/soc/soc-card-test.c \
					./alsa/sound/soc/soc-ops-test.c \
					./alsa/sound/soc/soc-topology-test.c \
					./alsa/sound/soc/soc-usb.c \
					./alsa/sound/soc/sof/sof-client.c \
					./alsa/sound/soc/ti/davinci-evm.c \
					./alsa/sound/soc/ti/n810.c \
					./alsa/sound/soc/ti/omap3pandora.c \
					./alsa/sound/soc/ti/osk5912.c \
					./alsa/sound/soc/ti/rx51.c \
					./alsa/sound/soc/ux500/mop500.c \
					./alsa/sound/soc/ux500/ux500_msp_dai.c \
					./alsa/sound/soc/codecs/cs35l56-shared-test.c \
					./alsa/sound/soc/codecs/cs42l43-sdw.c \
					./alsa/sound/soc/codecs/pm4125-sdw.c
					

ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/core/oss -type f -name "*.c")
ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/core/seq/oss -type f -name "*.c")
ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/firewire -type f -name "*.c")
ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/hda/codecs/helpers -type f -name "*.c") #included/inlined
ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/isa -type f -name "*.c")
ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/mips -type f -name "*.c")
ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/oss -type f -name "*.c")
ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/parisc -type f -name "*.c")
ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/ppc -type f -name "*.c")
ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/sh -type f -name "*.c")
ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/soc/au1x -type f -name "*.c")
ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/soc/fsl -type f -name "*.c")
ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/soc/pxa -type f -name "*.c")
ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/soc/renesas -type f -name "*.c") #CONFIG_CPU_SUBTYPE_SH7760/CONFIG_CPU_SUBTYPE_SH7780
ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/soc/sprd -type f -name "*.c") #CONFIG_SND_SOC_SPRD_MCDT
ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/soc/spear -type f -name "*.c")
ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/sparc -type f -name "*.c")
ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/usb -type f -name "*.c")
ALSA_SRC_EXCLUDE += $(shell find ./alsa/sound/virtio -type f -name "*.c")

ALSA_SRC := $(filter-out $(ALSA_SRC_EXCLUDE), $(ALSA_SRC))

ALSA_WRAPPER_SRC := 

SBEMU_SRC := sbemu/dbopl.cpp \
	     sbemu/opl3emu.cpp \
	     sbemu/pic.c \
	     sbemu/sbemu.c \
	     sbemu/iotrap.c \
	     sbemu/vdma.c \
	     sbemu/serial.c \
	     sbemu/dpmi/xms.c \
	     sbemu/dpmi/dpmi.c \
	     sbemu/dpmi/dbgutil.c \
	     sbemu/dpmi/dpmi_dj2.c \
	     sbemu/dpmi/dpmi_tsr.c \
	     sbemu/dpmi/djgpp/gormcb.c \
		 sbemu/dpmi/djgpp/gopint.c \
	     main.c \
	     vdpmi.c \
	     utility.c \
		 vmpu.c

LINUX_DRIVERS_SRC := $(ALSA_SRC) $(ALSA_WRAPPER_SRC) 
SRC := $(LINUX_DRIVERS_SRC) $(SBEMU_SRC)
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
# delete LTO -save-temps files
	$(SILENTCMD)$(RM) output/sbemu.ltrans*

distclean: clean
	$(SILENTMSG) "DISTCLEAN\n"
	$(SILENTCMD)$(RM) $(TARGET)
