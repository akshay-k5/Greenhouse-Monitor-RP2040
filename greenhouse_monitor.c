/*
 * Greenhouse Monitoring System
 * Raspberry Pi Pico (RP2040)
 *
 * Features:
 *  - Reads temperature from the onboard RP2040 sensor (ADC channel 4).
 *  - Controls a "warmer LED" (PWM) based on temperature:
 *        temp < 25 C          -> 100% intensity
 *        25 C <= temp < 27 C  ->  50% intensity
 *        temp >= 27 C         ->  25% intensity
 *  - Interactive UART / USB-CDC menu:
 *        1) Select control mode (Automatic / Manual)
 *        2) Show current temperature in C and F
 *        3) Decrease warmer LED intensity (manual only)
 *        4) Increase warmer LED intensity (manual only)
 */

#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"

/* ----------------------- Configuration ----------------------- */
#define LED_PIN          15       // PWM pin for warmer LED
#define PWM_WRAP         1000     // PWM period (0..1000)
#define TEMP_LOW_C       25.0f    // Below this -> 100%
#define TEMP_HIGH_C      27.0f    // At/above this -> 25%
#define MANUAL_STEP      25       // % step for manual adjustment

/* ------------------------- State ----------------------------- */
typedef enum { MODE_AUTO, MODE_MANUAL } control_mode_t;
static control_mode_t current_mode = MODE_MANUAL;
static int led_intensity = 100;   // 0..100 %

/* ------------------------ LED / PWM -------------------------- */
static void led_init(void) {
    gpio_set_function(LED_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(LED_PIN);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, PWM_WRAP);
    pwm_init(slice, &cfg, true);
}

static void led_set_intensity(int percent) {
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;
    led_intensity = percent;

    uint slice = pwm_gpio_to_slice_num(LED_PIN);
    uint chan  = pwm_gpio_to_channel(LED_PIN);
    uint level = (uint)((percent * PWM_WRAP) / 100);
    pwm_set_chan_level(slice, chan, level);
}

/* ---------------------- Temperature -------------------------- */
/* Onboard sensor: ADC channel 4.
 * Conversion from RP2040 datasheet:
 *     T = 27 - (V - 0.706) / 0.001721
 */
static float read_temp_celsius(void) {
    adc_select_input(4);
    uint32_t sum = 0;
    const int N = 8;                  // average 8 samples for stability
    for (int i = 0; i < N; i++) {
        sum += adc_read();
        sleep_us(200);
    }
    float raw     = (float)sum / N;
    float voltage = raw * 3.3f / 4096.0f;
    float temp_c  = 27.0f - (voltage - 0.706f) / 0.001721f;
    return temp_c;
}

static float celsius_to_fahrenheit(float c) {
    return c * 9.0f / 5.0f + 32.0f;
}

/* ------------------- Automatic logic ------------------------- */
static int auto_intensity_for_temp(float t) {
    if (t < TEMP_LOW_C)  return 100;
    if (t < TEMP_HIGH_C) return  50;
    return 25;
}

/* ---------------------- Input helper ------------------------- */
static char read_choice(void) {
    int ch;
    do {
        ch = getchar_timeout_us(100000);   // 100 ms timeout
    } while (ch == PICO_ERROR_TIMEOUT);

    sleep_ms(30);                          // let CR/LF arrive
    while (getchar_timeout_us(1000) != PICO_ERROR_TIMEOUT) { /* drain */ }
    return (char)ch;
}

/* -------------------------- Menu ----------------------------- */
static void print_menu(void) {
    printf("\n======== Greenhouse Monitoring System ========\n");
    printf(" Mode       : %s\n",
           current_mode == MODE_AUTO ? "AUTOMATIC" : "MANUAL");
    printf(" LED Warmth : %d%%\n", led_intensity);
    printf("----------------------------------------------\n");
    printf(" 1) Change Control Mode (Auto / Manual)\n");
    printf(" 2) Show Current Temperature\n");
    printf(" 3) Decrease Warmer-LED Intensity\n");
    printf(" 4) Increase Warmer-LED Intensity\n");
    printf("----------------------------------------------\n");
    printf(" Enter choice: ");
}

/* --------------------- Automatic mode ------------------------ */
static void run_auto_mode(void) {
    printf("\n>> AUTOMATIC mode active.");
    printf("\n>> Press any key to return to the main menu.\n\n");

    while (true) {
        float t = read_temp_celsius();
        int target = auto_intensity_for_temp(t);
        if (target != led_intensity) {
            led_set_intensity(target);
        }

        printf("Temp: %6.2f C  |  %6.2f F  |  Warmer LED: %3d%%\n",
               t, celsius_to_fahrenheit(t), led_intensity);

        int ch = getchar_timeout_us(0);
        if (ch != PICO_ERROR_TIMEOUT) {
            sleep_ms(30);
            while (getchar_timeout_us(1000) != PICO_ERROR_TIMEOUT) { /* drain */ }
            printf("\n>> Exiting AUTOMATIC mode.\n");
            return;
        }
        sleep_ms(1000);
    }
}

/* --------------------- Mode sub-menu ------------------------- */
static void choose_mode(void) {
    printf("\n--- Choose Control Mode ---\n");
    printf("  A) Automatic\n");
    printf("  M) Manual\n");
    printf("  Choice: ");
    char c = read_choice();

    if (c == 'a' || c == 'A') {
        current_mode = MODE_AUTO;
        printf("\n>> AUTOMATIC selected.\n");
        run_auto_mode();
    } else if (c == 'm' || c == 'M') {
        current_mode = MODE_MANUAL;
        printf("\n>> MANUAL selected. Use options 3 / 4 to adjust the LED.\n");
    } else {
        printf("\n>> Invalid choice.\n");
    }
}

/* --------------------------- Main ---------------------------- */
int main(void) {
    stdio_init_all();
    sleep_ms(3000);                        // let USB CDC enumerate

    adc_init();
    adc_set_temp_sensor_enabled(true);

    led_init();
    led_set_intensity(100);                // start warm

    printf("\n\n========================================\n");
    printf(" Greenhouse Monitoring System - Boot OK\n");
    printf("========================================\n");
    printf(" Warmer LED on GPIO%d | Sensor: onboard ADC4\n", LED_PIN);

    while (true) {
        print_menu();
        char choice = read_choice();

        switch (choice) {
            case '1':
                choose_mode();
                break;

            case '2': {
                float t = read_temp_celsius();
                printf("\n>> Current Temperature:\n");
                printf("     %.2f C\n", t);
                printf("     %.2f F\n", celsius_to_fahrenheit(t));
                break;
            }

            case '3':
                if (current_mode == MODE_MANUAL) {
                    led_set_intensity(led_intensity - MANUAL_STEP);
                    printf("\n>> LED decreased to %d%%\n", led_intensity);
                } else {
                    printf("\n>> Only available in MANUAL mode. Use option 1.\n");
                }
                break;

            case '4':
                if (current_mode == MODE_MANUAL) {
                    led_set_intensity(led_intensity + MANUAL_STEP);
                    printf("\n>> LED increased to %d%%\n", led_intensity);
                } else {
                    printf("\n>> Only available in MANUAL mode. Use option 1.\n");
                }
                break;

            default:
                printf("\n>> Invalid choice: '%c'\n", choice);
                break;
        }
    }

    return 0;
}
