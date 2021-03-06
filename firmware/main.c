/*
 * ShiftX2 firmware
 *
 * Copyright (C) 2016 Autosport Labs
 *
 * This file is part of the Race Capture firmware suite
 *
 * This is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details. You should
 * have received a copy of the GNU General Public License along with
 * this code. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ch.h"
#include "hal.h"
#include "settings.h"
#include "logging.h"
#include "system.h"
#include "system_serial.h"
#include "system_CAN.h"
#include "system_LED.h"
#include "system_ADC.h"
#include "system_button.h"

#define CAN_THREAD_STACK 512
#define LED_THREAD_STACK 256
#define LED_FLASH_THREAD_STACK 256
#define STARTUP_DEMO_THREAD_STACK 256
#define MAIN_THREAD_SLEEP_NORMAL_MS 10000
#define MAIN_THREAD_SLEEP_FINE_MS   1000
#define MAIN_THREAD_CHECK_INTERVAL_MS 100
#define WATCHDOG_TIMEOUT 11000
#define WATCHDOG_ENABLED true

/*
 * CAN receiver thread.
 */
static THD_WORKING_AREA(can_rx_wa, CAN_THREAD_STACK);
static THD_FUNCTION(can_rx, arg)
{
        (void)arg;
        chRegSetThreadName("CAN_worker");
        can_worker();
}

static THD_WORKING_AREA(led_work_wa, LED_THREAD_STACK);
static THD_FUNCTION(led_work, arg)
{
        (void)arg;
        chRegSetThreadName("LED_worker");
        led_worker();
}

static THD_WORKING_AREA(led_flash_work_wa, LED_FLASH_THREAD_STACK);
static THD_FUNCTION(led_flash_work, arg)
{
        (void)arg;
        chRegSetThreadName("LED_flas_worker");
        led_flash_worker();
}

static THD_WORKING_AREA(startup_demo_work_wa, STARTUP_DEMO_THREAD_STACK);
static THD_FUNCTION(startup_demo_work, arg)
{
        (void)arg;
        chRegSetThreadName("LED_flas_worker");
        startup_demo_worker();
}

static const WDGConfig wdgcfg = {
        STM32_IWDG_PR_64,
        STM32_IWDG_RL(1000),
        STM32_IWDG_WIN_DISABLED
};

/* Watchdog configuration and initialization
 */
static void _start_watchdog(void)
{
        if (! WATCHDOG_ENABLED)
                return;

        wdgStart(&WDGD1, &wdgcfg);
}

int main(void)
{
        /*
         * System initializations.
         * - HAL initialization, this also initializes the configured device drivers
         *   and performs the board-specific initializations.
         * - Kernel initialization, the main() function becomes a thread and the
         *   RTOS is active.
         */

        /* ChibiOS initialization */
        halInit();
        chSysInit();
        _start_watchdog();

        /* Application specific initialization */
        system_can_init();
        system_adc_init();
        system_serial_init();

        /*
         * Creates the processing threads.
         */
        chThdCreateStatic(can_rx_wa, sizeof(can_rx_wa), NORMALPRIO, can_rx, NULL);
        chThdCreateStatic(led_work_wa, sizeof(led_work_wa), NORMALPRIO, led_work, NULL);
        chThdCreateStatic(led_flash_work_wa, sizeof(led_flash_work_wa), NORMALPRIO, led_flash_work, NULL);
        chThdCreateStatic(startup_demo_work_wa, sizeof(startup_demo_work_wa), NORMALPRIO, startup_demo_work, NULL);

        uint32_t stats_check = 0;
        while (true) {
                chThdSleepMilliseconds(MAIN_THREAD_CHECK_INTERVAL_MS);
                stats_check += MAIN_THREAD_CHECK_INTERVAL_MS;
                if (stats_check > MAIN_THREAD_SLEEP_NORMAL_MS) {
                        broadcast_stats();
                        stats_check = 0;
                }
                button_check_broadcast_state();
                if (WATCHDOG_ENABLED)
                        wdgReset(&WDGD1);
                check_system_state();
        }
        return 0;
}
