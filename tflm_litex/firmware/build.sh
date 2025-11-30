#!/bin/bash
# Build script for TensorFlow Lite Micro on LiteX

set -e

echo "=========================================="
echo "Building TensorFlow Lite Micro on LiteX"
echo "=========================================="
echo ""

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Check if build directory exists
if [ ! -d "../build/colorlight_i5" ]; then
    echo "ERROR: LiteX build not found at ../build/colorlight_i5"
    echo "Please build the LiteX SoC first using colorlight_i5.py"
    exit 1
fi

# Build TFLM library if needed
if [ ! -f "tflm/build/libtflm.a" ]; then
    echo "Building TensorFlow Lite Micro library..."
    cd tflm
    make -j$(nproc)
    cd ..
    echo "TFLM library built successfully"
    echo ""
fi

# Build firmware
echo "Building firmware..."
make clean
make -j$(nproc)

if [ -f "build/main.bin" ]; then
    echo ""
    echo "=========================================="
    echo "Build successful!"
    echo "=========================================="
    echo "Firmware: build/main.bin"
    echo "ELF file: build/main.elf"
    echo ""
    echo "To upload to FPGA:"
    echo "  litex_term /dev/ttyUSBx --kernel=build/main.bin"
    echo ""
else
    echo ""
    echo "ERROR: Build failed!"
    exit 1
fi
