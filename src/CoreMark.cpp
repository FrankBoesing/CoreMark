// CoreMark Benchmark for Arduino compatible boards
//   original CoreMark code: https://github.com/eembc/coremark
#include <Arduino.h>
#include <stdarg.h>

// A way to call the C-only coremark function from Arduino's C++ environment
extern "C" int coremark_main(void);


void setup()
{
	Serial.begin(9600);
	while (!Serial) ; // wait for Arduino Serial Monitor
	delay(500);

	Serial.println("CoreMark Performance Benchmark");
	Serial.println();
	Serial.println("CoreMark measures how quickly your processor can manage linked");
	Serial.println("lists, compute matrix multiply, and execute state machine code.");
	Serial.println();
	Serial.println("Iterations/Sec is the main benchmark result, higher numbers are better");
	Serial.println("Running.... (usually requires 12 to 20 seconds)");
	Serial.println();


	delay(250);
	coremark_main(); // Run the benchmark  :-)
}

void loop()
{
}

/* --------------------------------------------------------------------------
 *  Dual-core support:
 *   - RP2040: Arduino-Pico loop1()-Modell
 *   - ESP32 : FreeRTOS Core-1 Loop (RP2040-analog)
 * -------------------------------------------------------------------------- */

#include "coremark.h"

#if MULTITHREAD > 1

/* ==========================================================================
 *  RP2040 IMPLEMENTATION  (UNVERÄNDERT)
 * ========================================================================== */
#if defined(ARDUINO_ARCH_RP2040)

volatile bool core1_go   = false;
volatile bool core1_done = false;

void* (*core1_func)(void*) = NULL;
void* core1_func_arg = NULL;

void setup1() {
#ifdef LED_BUILTIN
	pinMode(LED_BUILTIN, OUTPUT);
#endif
}

void loop1() {
	if (core1_go && !core1_done) {
		if (core1_func != NULL) {
#ifdef LED_BUILTIN
			digitalWrite(LED_BUILTIN, HIGH);
#endif
			core1_func(core1_func_arg);
			core1_func = NULL;
		}
		core1_done = true;
		core1_go   = false;
	}
}

ee_u8 core_start_parallel(core_results *res) {
	if (core1_func == NULL) {
		core1_func     = (void* (*)(void*))iterate;
		core1_func_arg = res;
		core1_done     = false;
		core1_go       = true;
	}
	return 0;
}

ee_u8 core_stop_parallel(core_results *res) {
	iterate(res);
	while (!core1_done) {
		/* busy wait */
	}
	return 0;
}

#endif /* ARDUINO_ARCH_RP2040 */

/* ==========================================================================
 *  ESP32 IMPLEMENTATION  (RP2040-ANALOG)
 * ========================================================================== */
#if defined(ARDUINO_ARCH_ESP32)

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static volatile bool core1_go   = false;
static volatile bool core1_done = false;

static void* (*core1_func)(void*) = NULL;
static void* core1_func_arg = NULL;

static TaskHandle_t core1_task = NULL;

/* --------------------------------------------------------------------------
 *  Core 1 permanent loop (equivalent to RP2040 loop1)
 * -------------------------------------------------------------------------- */
static void core1_loop_task(void *arg) {
	(void)arg;

	for (;;) {
		if (core1_go && !core1_done) {
			if (core1_func != NULL) {
#ifdef LED_BUILTIN
				digitalWrite(LED_BUILTIN, HIGH);
#endif
				core1_func(core1_func_arg);
				core1_func = NULL;
			}
			core1_done = true;
			core1_go   = false;
		}
		taskYIELD(); /* avoid watchdog, minimal OS influence */
	}
}

/* --------------------------------------------------------------------------
 *  Must be called once before CoreMark starts
 * -------------------------------------------------------------------------- */
void core_parallel_init(void) {
	if (core1_task == NULL) {
		xTaskCreatePinnedToCore(
			core1_loop_task,
			"core1_loop",
			4096, /* stack size – Annahme */
			NULL,
			configMAX_PRIORITIES - 1,
			&core1_task,
			1 /* Core 1 */
		);
	}
}

uint8_t core_start_parallel(core_results *res) {
	if (core1_func == NULL) {
		core1_func     = (void* (*)(void*))iterate;
		core1_func_arg = res;
		core1_done     = false;
		core1_go       = true;
	}
	return 0;
}

uint8_t core_stop_parallel(core_results *res) {
	/* Core 0 work */
	iterate(res);

	/* wait for core 1 */
	while (!core1_done) {
		taskYIELD();
	}
	return 0;
}

#endif /* ARDUINO_ARCH_ESP32 */

#endif /* MULTITHREAD > 1 */


// CoreMark calls this function to print results.
extern "C" int ee_printf(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	for (; *format; format++) {
		if (*format == '%') {
			bool islong = false;
			format++;
			if (*format == '%') { Serial.print(*format); continue; }
			if (*format == '-') format++; // ignore size
			while (*format >= '0' && *format <= '9') format++; // ignore size
			if (*format == 'l') { islong = true; format++; }
			if (*format == '\0') break;
			if (*format == 's') {
				Serial.print((char *)va_arg(args, int));
			} else if (*format == 'f') {
				Serial.print(va_arg(args, double));
			} else if (*format == 'd') {
				if (islong) Serial.print(va_arg(args, long));
				else Serial.print(va_arg(args, int));
			} else if (*format == 'u') {
				if (islong) Serial.print(va_arg(args, unsigned long));
				else Serial.print(va_arg(args, unsigned int));
			} else if (*format == 'x') {
				if (islong) Serial.print(va_arg(args, unsigned long), HEX);
				else Serial.print(va_arg(args, unsigned int), HEX);
			} else if (*format == 'c' ) {
				Serial.print(va_arg(args, int));
			}
		} else {
			if (*format == '\n') Serial.print('\r');
			Serial.print(*format);
		}
	}
	va_end(args);
	return 1;
}

// CoreMark calls this function to measure elapsed time
extern "C" uint32_t Arduino_millis(void)
{
	return millis();
}
