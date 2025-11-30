/* Copyright 2023 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

/*
 * LiteX platform-specific timing for TensorFlow Lite Micro
 */

#include "tensorflow/lite/micro/micro_time.h"

extern "C" {
#include <generated/csr.h>
}

namespace tflite {

// Simple tick counter using LiteX timer if available
// Otherwise returns 0
uint32_t ticks_per_second() {
#ifdef CSR_TIMER0_BASE
  return CONFIG_CLOCK_FREQUENCY;
#else
  return 0;
#endif
}

uint32_t GetCurrentTimeTicks() {
#ifdef CSR_TIMER0_BASE
  // LiteX timer counts down, so we invert it
  // This is a simplified implementation
  static uint32_t overflow_count = 0;
  static uint32_t last_value = 0;
  
  uint32_t current_value = timer0_value_read();
  
  if (current_value > last_value) {
    overflow_count++;
  }
  last_value = current_value;
  
  return (uint32_t)((overflow_count << 16) | (current_value & 0xFFFF));
#else
  return 0;
#endif
}

}  // namespace tflite
