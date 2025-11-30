OUT = build
TFLM = .

# Toolchain path
TOOLCHAIN_PATH = ../../toolchain/xpack-riscv-none-elf-gcc-13.2.0-1/bin

# Tools
CXX = $(TOOLCHAIN_PATH)/riscv-none-elf-g++
AR = $(TOOLCHAIN_PATH)/riscv-none-elf-ar

# TFLM defines
TFLM_DEFINES = -DTF_LITE_STATIC_MEMORY -DTF_LITE_DISABLE_X86_NEON
TFLM_DEFINES += -DGEMMLOWP_ALLOW_SLOW_SCALAR_FALLBACK
TFLM_DEFINES += -DTF_LITE_USE_GLOBAL_CMATH_FUNCTIONS
TFLM_DEFINES += -DTF_LITE_USE_GLOBAL_MAX -DTF_LITE_USE_GLOBAL_MIN

# TFLM includes
TFLM_INCLUDES = -I$(TFLM) -I$(TFLM)/third_party/flatbuffers/include
TFLM_INCLUDES += -I$(TFLM)/third_party/gemmlowp -I$(TFLM)/third_party/ruy

# TFLM compiler flags
TFLM_FLAGS = -march=rv32im_zicsr -mabi=ilp32 -O2 -g -std=c++17
TFLM_FLAGS += -fno-rtti -fno-exceptions -fno-threadsafe-statics
TFLM_FLAGS += -ffunction-sections -fdata-sections -Wall -Wno-unused-parameter
TFLM_FLAGS += $(TFLM_INCLUDES) $(TFLM_DEFINES)

# Find all TFLM sources
TFLM_SOURCES = $(shell find tensorflow/lite/micro -name "*.cc" \
                 -not -name "*test*.cc" -not -name "*mock*.cc" \
                 -not -path "*/examples/*" -not -path "*/benchmarks/*" \
                 -not -path "*/tools/*" -not -path "*/testing/*")
TFLM_SOURCES += $(shell find tensorflow/lite/core -name "*.cc" -not -name "*test*.cc")
TFLM_SOURCES += $(shell find tensorflow/lite/kernels -name "*.cc" \
                  -not -name "*test*.cc" -not -path "*/test/*")
TFLM_SOURCES += $(shell find tensorflow/compiler/mlir/lite -name "*.cc" -not -name "*test*.cc")
TFLM_SOURCES += $(shell find third_party -name "*.cc" -not -name "*test*.cc" -not -path "*/test/*")

TFLM_OBJECTS = $(patsubst %.cc,$(OUT)/%.o,$(TFLM_SOURCES))

# Build library
all: $(OUT)/libtflm.a

$(OUT)/libtflm.a: $(TFLM_OBJECTS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(OUT)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) $(TFLM_FLAGS) -c $< -o $@

clean:
	rm -rf $(OUT)

.PHONY: all clean
