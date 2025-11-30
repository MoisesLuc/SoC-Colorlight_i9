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
 * LiteX platform-specific debug logging for TensorFlow Lite Micro
 */

#include "tensorflow/lite/micro/debug_log.h"
#include "tensorflow/lite/micro/micro_log.h"

#include <cstdarg>
#include <cstdio>

extern "C" {
#include <uart.h>
#include <console.h>
}

// Implementation of DebugLog (called by TFLM internally)
extern "C" void DebugLog(const char* format, va_list args) {
  // Use LiteX's vprintf which outputs to UART
  vprintf(format, args);
}

// Implementation of MicroPrintf (public API for logging)
void MicroPrintf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
}
