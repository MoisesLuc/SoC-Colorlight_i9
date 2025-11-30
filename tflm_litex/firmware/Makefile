# TFLM firmware for LiteX/VexRiscv
# Uses pre-built TFLM library

# Directories
BUILD_DIR = ../build/colorlight_i5
OUT = build
TFLM = tflm

# LiteX configuration
include $(BUILD_DIR)/software/include/generated/variables.mak
include $(SOC_DIRECTORY)/software/common.mak

# Override toolchain to use local RISC-V toolchain
TOOLCHAIN_PATH = ../toolchain/xpack-riscv-none-elf-gcc-13.2.0-1/bin
CC := $(TOOLCHAIN_PATH)/riscv-none-elf-gcc
CX := $(TOOLCHAIN_PATH)/riscv-none-elf-g++
AR := $(TOOLCHAIN_PATH)/riscv-none-elf-ar
OBJCOPY := $(TOOLCHAIN_PATH)/riscv-none-elf-objcopy

# TFLM defines (must match library build)
TFLM_DEFINES = -DTF_LITE_STATIC_MEMORY -DTF_LITE_DISABLE_X86_NEON
TFLM_DEFINES += -DGEMMLOWP_ALLOW_SLOW_SCALAR_FALLBACK
TFLM_DEFINES += -DTF_LITE_USE_GLOBAL_CMATH_FUNCTIONS
TFLM_DEFINES += -DTF_LITE_USE_GLOBAL_MAX -DTF_LITE_USE_GLOBAL_MIN

# TFLM includes
TFLM_INCLUDES = -I$(TFLM) -I$(TFLM)/third_party/flatbuffers/include
TFLM_INCLUDES += -I$(TFLM)/third_party/gemmlowp -I$(TFLM)/third_party/ruy

# Override CPUFLAGS for RV32IM (instead of picorv32 from variables.mak)
CPUFLAGS_OVERRIDE = -march=rv32im_zicsr -mabi=ilp32

# Application flags (uses LiteX CPUFLAGS)
APP_FLAGS = $(CPUFLAGS_OVERRIDE) -Os -g -std=c++17
APP_FLAGS += -fno-rtti -fno-exceptions -fno-threadsafe-statics
APP_FLAGS += -ffunction-sections -fdata-sections -Wall
APP_FLAGS += -I. -I$(BUILDINC_DIRECTORY) -I$(BUILDINC_DIRECTORY)/../libc
APP_FLAGS += -I$(SOC_DIRECTORY)/software/include -I$(SOC_DIRECTORY)/software/libbase
APP_FLAGS += -I$(CPU_DIRECTORY) -Iplatform
APP_FLAGS += $(TFLM_INCLUDES) $(TFLM_DEFINES)

# Sources
PLATFORM_SOURCES = platform/litex_debug_log.cc platform/litex_micro_time.cc platform/litex_system_setup.cc
APP_SOURCES = main.cc models/hello_world_int8_model_data.cc

# Objects
PLATFORM_OBJECTS = $(patsubst %.cc,$(OUT)/%.o,$(PLATFORM_SOURCES))
APP_OBJECTS = $(patsubst %.cc,$(OUT)/%.o,$(APP_SOURCES))
OBJECTS = $(OUT)/crt0.o $(APP_OBJECTS) $(PLATFORM_OBJECTS)

# Pre-built TFLM library
TFLM_LIB = $(TFLM)/build/libtflm.a

# Build
all: $(OUT)/main.bin

$(OUT)/main.bin: $(OUT)/main.elf
	$(OBJCOPY) -O binary $< $@
	@echo "Firmware ready: $@"

$(OUT)/main.elf: $(OBJECTS) $(TFLM_LIB)
	$(CC) $(LDFLAGS) -T linker.ld -N \
		-Wl,--start-group $^ \
		$(PACKAGES:%=-L$(BUILD_DIR)/software/%) \
		$(LIBS:lib%=-l%) -lgcc \
		-Wl,--end-group \
		-Wl,--gc-sections -Wl,--allow-multiple-definition \
		-Wl,-Map,$@.map -o $@
	@echo "Firmware size:"
	@$(TRIPLE)-size $@

$(OUT)/platform/%.o: platform/%.cc
	@mkdir -p $(dir $@)
	$(CX) $(APP_FLAGS) -c $< -o $@

$(OUT)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CX) $(APP_FLAGS) -c $< -o $@

$(OUT)/crt0.o: $(CPU_DIRECTORY)/crt0.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OUT)

.PHONY: all clean
