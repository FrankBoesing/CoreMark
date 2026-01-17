/*
Copyright 2018 Embedded Microprocessor Benchmark Consortium (EEMBC)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Original Author: Shay Gal-on
*/
#include "coremark.h"
#include "core_portme.h"

extern uint32_t Arduino_millis();

#if VALIDATION_RUN
	volatile ee_s32 seed1_volatile=0x3415;
	volatile ee_s32 seed2_volatile=0x3415;
	volatile ee_s32 seed3_volatile=0x66;
#endif
#if PERFORMANCE_RUN
	volatile ee_s32 seed1_volatile=0x0;
	volatile ee_s32 seed2_volatile=0x0;
	volatile ee_s32 seed3_volatile=0x66;
#endif
#if PROFILE_RUN
	volatile ee_s32 seed1_volatile=0x8;
	volatile ee_s32 seed2_volatile=0x8;
	volatile ee_s32 seed3_volatile=0x8;
#endif
	volatile ee_s32 seed4_volatile=ITERATIONS;
	volatile ee_s32 seed5_volatile=0;
/* Porting : Timing functions
	How to capture time and convert to seconds must be ported to whatever is supported by the platform.
	e.g. Read value from on board RTC, read value from cpu clock cycles performance counter etc.
	Sample implementation for standard time.h and windows.h definitions included.
*/
CORETIMETYPE barebones_clock() {
	return Arduino_millis();
}
/* Define : TIMER_RES_DIVIDER
	Divider to trade off timer resolution and total time that can be measured.

	Use lower values to increase resolution, but make sure that overflow does not occur.
	If there are issues with the return value overflowing, increase this value.
	*/
#define CLOCKS_PER_SEC 1000.0
#define TIMER_RES_DIVIDER 1

#define GETMYTIME(_t) (*_t=barebones_clock())
#define MYTIMEDIFF(fin,ini) ((fin)-(ini))
#define TIMER_RES_DIVIDER 1
#define SAMPLE_TIME_IMPLEMENTATION 1
#define EE_TICKS_PER_SEC (CLOCKS_PER_SEC / TIMER_RES_DIVIDER)

/** Define Host specific (POSIX), or target specific global time variables. */
static CORETIMETYPE start_time_val, stop_time_val;

/* Function : start_time
	This function will be called right before starting the timed portion of the benchmark.

	Implementation may be capturing a system timer (as implemented in the example code)
	or zeroing some system parameters - e.g. setting the cpu clocks cycles to 0.
*/
void start_time(void) {
	GETMYTIME(&start_time_val );
}
/* Function : stop_time
	This function will be called right after ending the timed portion of the benchmark.

	Implementation may be capturing a system timer (as implemented in the example code)
	or other system parameters - e.g. reading the current value of cpu cycles counter.
*/
void stop_time(void) {
	GETMYTIME(&stop_time_val );
}
/* Function : get_time
	Return an abstract "ticks" number that signifies time on the system.

	Actual value returned may be cpu cycles, milliseconds or any other value,
	as long as it can be converted to seconds by <time_in_secs>.
	This methodology is taken to accomodate any hardware or simulated platform.
	The sample implementation returns millisecs by default,
	and the resolution is controlled by <TIMER_RES_DIVIDER>
*/
CORE_TICKS get_time(void) {
	CORE_TICKS elapsed=(CORE_TICKS)(MYTIMEDIFF(stop_time_val, start_time_val));
	return elapsed;
}
/* Function : time_in_secs
	Convert the value returned by get_time to seconds.

	The <secs_ret> type is used to accomodate systems with no support for floating point.
	Default implementation implemented by the EE_TICKS_PER_SEC macro above.
*/
secs_ret time_in_secs(CORE_TICKS ticks) {
	secs_ret retval=((secs_ret)ticks) / (secs_ret)EE_TICKS_PER_SEC;
	return retval;
}

ee_u32 default_num_contexts=MULTITHREAD;

/* Function : portable_init
	Target specific initialization code
	Test for some common mistakes.
*/
void portable_init(core_portable *p, int *argc, char *argv[])
{
	// Serial.begin(9600);
	// #error "Call board initialization routines in portable init (if needed), in particular initialize UART!\n"
	if (sizeof(ee_ptr_int) != sizeof(ee_u8 *)) {
		ee_printf("ERROR! Please define ee_ptr_int to a type that holds a pointer!\n");
	}
	if (sizeof(ee_u32) != 4) {
		ee_printf("ERROR! Please define ee_u32 to a 32b unsigned type!\n");
	}
	p->portable_id=1;
}
/* Function : portable_fini
	Target specific final code
*/
void portable_fini(core_portable *p)
{
	p->portable_id=0;
}

#if (MEM_METHOD==MEM_MALLOC)
#include <stdlib.h>
void *portable_malloc(ee_size_t size) {
	return malloc(size);
}
void portable_free(void *p) {
	free(p);
}
#endif

#if (MULTITHREAD > 1) && (USE_PICO > 0)
/* RP2040/RP2350 dual-core parallel execution */
extern void start_on_core1(void* (*func)(void*), void* arg);
extern void wait_for_core1(void);

static core_results *core0_work = NULL;

ee_u8 core_start_parallel(core_results *res) {
	if (core0_work == NULL) {
		/* First call - save for core 0 to run later */
		core0_work = res;
	} else {
		/* Second call - start core 1 now */
		start_on_core1((void* (*)(void*))iterate, res);
	}
	return 0;
}

ee_u8 core_stop_parallel(core_results *res) {
	if (res == core0_work) {
		/* Run core 0's work on this core */
		iterate(res);
		core0_work = NULL;
	} else {
		/* Wait for core 1 to finish */
		wait_for_core1();
	}
	return 0;
}
#elif (MULTITHREAD > 1) && (USE_FREERTOS > 0)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_task_wdt.h>

/* Semaphore-based synchronization to ensure tasks start together and report when done */
static TaskHandle_t core_tasks[2] = {NULL, NULL};
static SemaphoreHandle_t start_sem = NULL;
static SemaphoreHandle_t done_sem = NULL;
static int created_count = 0;

/* Task wrapper for cores */
static void core_task_fn(void *arg) {
	core_results *res = (core_results *)arg;

	/* wait until main gives the start semaphore */
	xSemaphoreTake(start_sem, portMAX_DELAY);

	iterate(res);

	/* signal completion to main */
	xSemaphoreGive(done_sem);

	vTaskDelete(NULL);
}

ee_u8 core_start_parallel(core_results *res) {
	/* create semaphores once */
	if (start_sem == NULL) {
		start_sem = xSemaphoreCreateCounting(MULTITHREAD, 0);
	}
	if (done_sem == NULL) {
		done_sem = xSemaphoreCreateCounting(MULTITHREAD, 0);
	}

	/* create a task for this context and pin to the next core core index is created_count (0 or 1) */
	int idx = created_count;
	if (idx < MULTITHREAD) {
		char name[16];
		if (idx == 0) snprintf(name, sizeof(name), "core0_iter");
		else snprintf(name, sizeof(name), "core1_iter");
		xTaskCreatePinnedToCore(
			core_task_fn,
			name,
			4096,
			res,
			1,
			&core_tasks[idx],
			idx /* pin to core idx */
		);
		created_count++;
	}

	/* if we've created all expected contexts, release the start semaphore for each */
	esp_task_wdt_deinit();
	if (created_count == (int)default_num_contexts) {
		for (int i = 0; i < created_count; i++) {
			xSemaphoreGive(start_sem);
		}
		/* reset for next run */
		created_count = 0;
	}

	return 0;
}

ee_u8 core_stop_parallel(core_results *res) {
	/* wait until one of the tasks reports completion */
	xSemaphoreTake(done_sem, portMAX_DELAY);
	return 0;
}
#endif
