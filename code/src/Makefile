# POLY, a synth firmware for the CHOMPI
#
# Build with ARM GCC 10.3-2021.10 (the same toolchain as TAPE and WAVE):
#   PATH=/path/to/gcc-arm-none-eabi-10.3-2021.10/bin:$PATH make
# Output: build/CHOMPI.bin. Install it as /FIRMWARE/04_POLY.bin on a
# multi-firmware launcher card (or send it over USB MIDI from the launcher).

TARGET = CHOMPI

DEBUG = 0
OPT = -O3

# Runs from SRAM, loaded there by the CHOMPI bootloader / launcher.
APP_TYPE = BOOT_SRAM
LDSCRIPT = chompi_sram.lds

# FreeRTOS coreJSON (options.json)
C_SOURCES = ../libs/coreJSON/source/core_json.c
C_INCLUDES += -I../libs/coreJSON/source/include

CPP_SOURCES = chompi_main.cpp \
	encoder.cpp

USE_FATFS = 1

# DIAG=1 logs UI events to /POLY/diag.txt (debug builds only)
DIAG ?= 0
C_DEFS += -DDIAG=$(DIAG)

LIBDAISY_DIR = ../libs/libDaisy
DAISYSP_DIR = ../libs/DaisySP

SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
