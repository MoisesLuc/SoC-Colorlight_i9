/*
 * TensorFlow Lite Micro - Hello World Example
 * Adapted for LiteX SoC on FPGA
 * 
 * This firmware runs a simple sine wave prediction model using TensorFlow Lite Micro
 * on a RISC-V soft-core processor in an FPGA using the LiteX SoC framework.
 */

// LiteX includes (C headers)
extern "C" {
#include <irq.h>
#include <uart.h>
#include <console.h>
#include <generated/csr.h>
}

// Undefine min/max macros from LiteX to avoid conflicts with C++ std::min/max
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// TensorFlow Lite Micro includes
#include "tensorflow/lite/core/c/common.h"
#include "models/hello_world_int8_model_data.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {
using HelloWorldOpResolver = tflite::MicroMutableOpResolver<1>;

TfLiteStatus RegisterOps(HelloWorldOpResolver& op_resolver) {
  TF_LITE_ENSURE_STATUS(op_resolver.AddFullyConnected());
  return kTfLiteOk;
}
}  // namespace

// Global variables for TensorFlow Lite Micro
namespace {
  const tflite::Model* model = nullptr;
  tflite::MicroInterpreter* interpreter = nullptr;
  TfLiteTensor* input = nullptr;
  TfLiteTensor* output = nullptr;
  
  constexpr int kTensorArenaSize = 4096;
  uint8_t tensor_arena[kTensorArenaSize] __attribute__((aligned(16)));
}

// Console helper functions
static char *readstr(void)
{
	char c[2];
	static char s[64];
	static int ptr = 0;

	if(readchar_nonblock()) {
		c[0] = readchar();
		c[1] = 0;
		switch(c[0]) {
			case 0x7f:
			case 0x08:
				if(ptr > 0) {
					ptr--;
					putsnonl("\x08 \x08");
				}
				break;
			case 0x07:
				break;
			case '\r':
			case '\n':
				s[ptr] = 0x00;
				putsnonl("\n");
				ptr = 0;
				return s;
			default:
				if(ptr >= (sizeof(s) - 1))
					break;
				putsnonl(c);
				s[ptr] = c[0];
				ptr++;
				break;
		}
	}

	return NULL;
}

static char *get_token(char **str)
{
	char *c, *d;

	c = (char *)strchr(*str, ' ');
	if(c == NULL) {
		d = *str;
		*str = *str+strlen(*str);
		return d;
	}
	*c = 0;
	d = *str;
	*str = c+1;
	return d;
}

// Helper function to print float (picolibc doesn't support %f)
static void print_float(float value, int decimals)
{
	int integer_part = (int)value;
	float frac = value - integer_part;
	
	if (value < 0 && integer_part == 0) {
		printf("-");
	}
	
	printf("%d.", integer_part);
	
	if (frac < 0) frac = -frac;
	
	for (int i = 0; i < decimals; i++) {
		frac *= 10;
		int digit = (int)frac;
		printf("%d", digit);
		frac -= digit;
	}
}

static void prompt(void)
{
	printf("TFLM>");
}

// TensorFlow Lite Micro initialization
static bool tflm_init(void)
{
	printf("Initializing TensorFlow Lite Micro...\n");
	
	// Load the model
	model = tflite::GetModel(g_hello_world_int8_model_data);
	if (model->version() != TFLITE_SCHEMA_VERSION) {
		printf("Model schema version mismatch!\n");
		printf("Expected: %d, Got: %d\n", TFLITE_SCHEMA_VERSION, model->version());
		return false;
	}
	printf("Model loaded (version %d)\n", model->version());
	
	// Set up the operations resolver
	static HelloWorldOpResolver op_resolver;
	if (RegisterOps(op_resolver) != kTfLiteOk) {
		printf("Failed to register operations!\n");
		return false;
	}
	printf("Operations registered\n");
	
	// Create the interpreter
	static tflite::MicroInterpreter static_interpreter(
		model, op_resolver, tensor_arena, kTensorArenaSize);
	interpreter = &static_interpreter;
	
	// Allocate tensors
	TfLiteStatus allocate_status = interpreter->AllocateTensors();
	if (allocate_status != kTfLiteOk) {
		printf("AllocateTensors() failed!\n");
		return false;
	}
	printf("Tensors allocated\n");
	
	// Get input and output tensors
	input = interpreter->input(0);
	output = interpreter->output(0);
	
	printf("Input: %d bytes, type %d\n", input->bytes, input->type);
	printf("Output: %d bytes, type %d\n", output->bytes, output->type);
	printf("Arena used: %d / %d bytes\n", 
		interpreter->arena_used_bytes(), kTensorArenaSize);
	
	return true;
}

// Run inference with given input value
static void tflm_infer(float input_val)
{
	if (!interpreter) {
		printf("Model not initialized!\n");
		return;
	}
	
	// Quantize input
	float input_scale = input->params.scale;
	int input_zero_point = input->params.zero_point;
	int8_t quantized_input = (int8_t)(input_val / input_scale + input_zero_point);
	
	// Set input
	input->data.int8[0] = quantized_input;
	
	// Run inference
	TfLiteStatus invoke_status = interpreter->Invoke();
	if (invoke_status != kTfLiteOk) {
		printf("Invoke failed!\n");
		return;
	}
	
	// Dequantize output
	float output_scale = output->params.scale;
	int output_zero_point = output->params.zero_point;
	float output_val = (output->data.int8[0] - output_zero_point) * output_scale;
	
	// Calculate expected value
	float expected = sin(input_val);
	float error = fabs(expected - output_val);
	
	printf("Input: ");
	print_float(input_val, 2);
	printf(" (q=%d)\n", quantized_input);
	
	printf("Output: ");
	print_float(output_val, 4);
	printf(" (q=%d)\n", output->data.int8[0]);
	
	printf("Expected: ");
	print_float(expected, 4);
	printf(" (sin(");
	print_float(input_val, 2);
	printf("))\n");
	
	printf("Error: ");
	print_float(error, 4);
	printf("\n");
}

// Run a test suite
static void tflm_test(void)
{
	printf("\n=== Running TensorFlow Lite Micro Test Suite ===\n\n");
	
	const float test_values[] = {0.0f, 0.77f, 1.57f, 2.3f, 3.14f, 4.71f, 6.28f};
	const int num_tests = sizeof(test_values) / sizeof(test_values[0]);
	
	for (int i = 0; i < num_tests; i++) {
		printf("--- Test %d/%d ---\n", i+1, num_tests);
		tflm_infer(test_values[i]);
		printf("\n");
	}
	
	printf("=== All tests completed ===\n\n");
}

// Continuous inference loop - simulates LED wave effect in console
static void tflm_continuous(void)
{
	if (!interpreter) {
		printf("Model not initialized. Run 'init' first.\n");
		return;
	}
	
	printf("\n=== Continuous Inference Mode ===\n");
	printf("Simulating sine wave (like LED brightness control)\n");
	printf("Press Ctrl+C or any key to stop\n\n");
	
	float x = 0.0f;
	const float kXrange = 2.0f * 3.14159265f; // 2*PI
	int inference_count = 0;
	
	while (1) {
		// Check if user wants to stop
		if (readchar_nonblock()) {
			readchar(); // consume the character
			break;
		}
		
		// Quantize input
		float input_scale = input->params.scale;
		int input_zero_point = input->params.zero_point;
		int8_t quantized_input = (int8_t)(x / input_scale + input_zero_point);
		
		// Set input and run inference
		input->data.int8[0] = quantized_input;
		TfLiteStatus invoke_status = interpreter->Invoke();
		
		if (invoke_status != kTfLiteOk) {
			printf("Invoke failed!\n");
			break;
		}
		
		// Dequantize output
		float output_scale = output->params.scale;
		int output_zero_point = output->params.zero_point;
		float y_val = (output->data.int8[0] - output_zero_point) * output_scale;
		
		// Convert output to "brightness" (0-100%)
		// Output is in range [-1, 1], convert to [0, 1]
		float brightness = (y_val + 1.0f) / 2.0f;
		if (brightness < 0.0f) brightness = 0.0f;
		if (brightness > 1.0f) brightness = 1.0f;
		
		int brightness_percent = (int)(brightness * 100.0f);
		
		// Print wave visualization
		printf("x=");
		print_float(x, 2);
		printf(" y=");
		print_float(y_val, 3);
		printf(" [");
		
		// Draw brightness bar
		int bar_length = brightness_percent / 2; // 50 chars max
		for (int i = 0; i < 50; i++) {
			if (i < bar_length) {
				printf("=");
			} else {
				printf(" ");
			}
		}
		printf("] %d%%\n", brightness_percent);
		
		// Increment x and wrap around
		x += 0.1f;
		if (x >= kXrange) {
			x = 0.0f;
			inference_count++;
			printf("--- Cycle %d completed ---\n", inference_count);
		}
		
		// Small delay to make it visible
		for (volatile int i = 0; i < 100000; i++);
	}
	
	printf("\n=== Stopped continuous mode ===\n");
	printf("Total cycles: %d\n\n", inference_count);
}

static void help(void)
{
	puts("TensorFlow Lite Micro on LiteX - Available commands:");
	puts("help                - Show this help");
	puts("reboot              - Reboot CPU");
	puts("led                 - Toggle LED");
	puts("init                - Initialize TensorFlow Lite Micro");
	puts("test                - Run inference test suite");
	puts("infer <value>       - Run inference on single value");
	puts("run                 - Run continuous inference (sine wave)");
	puts("info                - Show model information");
}

static void reboot(void)
{
	ctrl_reset_write(1);
}

static void toggle_led(void)
{
	int i;
	printf("Toggling LED...\n");
	i = leds_out_read();
	leds_out_write(!i);
}

static void show_info(void)
{
	if (!interpreter) {
		printf("Model not initialized. Run 'init' first.\n");
		return;
	}
	
	printf("\n=== TensorFlow Lite Micro Model Info ===\n");
	printf("Model size: %u bytes\n", g_hello_world_int8_model_data_size);
	printf("Schema version: %d\n", model->version());
	printf("Tensor arena size: %d bytes\n", kTensorArenaSize);
	printf("Arena used: %d bytes\n", interpreter->arena_used_bytes());
	printf("Inputs: %d\n", interpreter->inputs_size());
	printf("Outputs: %d\n", interpreter->outputs_size());
	
	if (input) {
		printf("\nInput tensor:\n");
		printf("  Type: %d (int8)\n", input->type);
		printf("  Bytes: %d\n", input->bytes);
		printf("  Scale: ");
		print_float(input->params.scale, 6);
		printf("\n");
		printf("  Zero point: %d\n", input->params.zero_point);
	}
	
	if (output) {
		printf("\nOutput tensor:\n");
		printf("  Type: %d (int8)\n", output->type);
		printf("  Bytes: %d\n", output->bytes);
		printf("  Scale: ");
		print_float(output->params.scale, 6);
		printf("\n");
		printf("  Zero point: %d\n", output->params.zero_point);
	}
	printf("\n");
}

static void console_service(void)
{
	char *str;
	char *token;

	str = readstr();
	if(str == NULL) return;
	
	token = get_token(&str);
	
	if(strcmp(token, "help") == 0) {
		help();
	}
	else if(strcmp(token, "reboot") == 0) {
		reboot();
	}
	else if(strcmp(token, "led") == 0) {
		toggle_led();
	}
	else if(strcmp(token, "init") == 0) {
		if (tflm_init()) {
			printf("TensorFlow Lite Micro initialized successfully!\n");
		} else {
			printf("Failed to initialize TensorFlow Lite Micro!\n");
		}
	}
	else if(strcmp(token, "test") == 0) {
		tflm_test();
	}
	else if(strcmp(token, "run") == 0) {
		tflm_continuous();
	}
	else if(strcmp(token, "infer") == 0) {
		token = get_token(&str);
		if (strlen(token) > 0) {
			float val = atof(token);
			tflm_infer(val);
		} else {
			printf("Usage: infer <value>\n");
		}
	}
	else if(strcmp(token, "info") == 0) {
		show_info();
	}
	else if(strlen(token) > 0) {
		printf("Unknown command: %s\n", token);
	}
	
	prompt();
}

// Implementation of DebugLog for TFLM (already declared in debug_log.h)
// MicroPrintf is defined in platform/litex_debug_log.cc

int main(void)
{
#ifdef CONFIG_CPU_HAS_INTERRUPT
	irq_setmask(0);
	irq_setie(1);
#endif
	uart_init();

	puts("\n========================================");
	puts("TensorFlow Lite Micro on LiteX");
	puts("Built: " __DATE__ " " __TIME__);
	puts("========================================\n");
	
	printf("System ready. Type 'init' to initialize TensorFlow Lite Micro.\n");
	printf("Type 'help' for available commands.\n\n");
	
	help();
	prompt();

	while(1) {
		console_service();
	}

	return 0;
}
