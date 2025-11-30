// Platform-specific configuration for TensorFlow Lite Micro on LiteX
// This file can be used to override default TFLM settings

#ifndef LITEX_TFLM_CONFIG_H_
#define LITEX_TFLM_CONFIG_H_

// Memory configuration
#define TFLM_TENSOR_ARENA_SIZE 4096

// Enable debug logging
#define TF_LITE_STRIP_ERROR_STRINGS 0

// Platform features
#define LITEX_HAS_TIMER 1
#define LITEX_HAS_UART 1

// Performance optimization
#define TFLM_OPTIMIZE_FOR_SIZE 1

#endif  // LITEX_TFLM_CONFIG_H_
