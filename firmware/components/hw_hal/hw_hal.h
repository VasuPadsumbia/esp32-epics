/**
 * @file hw_hal.h
 * @brief Hardware Abstraction Layer for GPIO peripherals.
 *
 * Provides a clean interface for GPIO control decoupled from ESP-IDF specifics.
 * This allows unit testing of application logic without real hardware.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PIN_ROLE_UNUSED = 0,
    PIN_ROLE_GPIO_IN,
    PIN_ROLE_GPIO_OUT,
    PIN_ROLE_PWM,
    PIN_ROLE_ADC,
    PIN_ROLE_DAC,
    PIN_ROLE_I2C,
    PIN_ROLE_UART
} pin_role_t;

/** @brief Initialize a pin for a specific role. */
void hw_hal_pin_cfg(uint8_t pin, pin_role_t role);

/** @brief Restore all pin roles from NVS. */
void hw_hal_init_all(void);

/** @brief Initialise a GPIO pin as a push-pull output. */
void hw_hal_gpio_init_output(uint8_t pin);

/** @brief Initialise a GPIO pin as an input (with pull-up). */
void hw_hal_gpio_init_input(uint8_t pin);

/** @brief Set a GPIO output level. @param level true = HIGH, false = LOW */

void hw_hal_gpio_set(uint8_t pin, bool level);

/** @brief Read the current logical level of a GPIO pin. */
bool hw_hal_gpio_get(uint8_t pin);

pin_role_t hw_hal_get_pin_role(uint8_t pin);
uint32_t hw_hal_get_pin_value(uint8_t pin);

/* --- PWM (LEDC) --- */

/** @brief Initialise a PWM channel on a pin. */
void hw_hal_pwm_init(uint8_t pin, uint8_t channel, uint32_t freq_hz);

/** @brief Set PWM duty cycle (0-1023 for 10-bit). */
void hw_hal_pwm_set_duty(uint8_t pin, uint32_t duty);

/** @brief Set PWM frequency. */
void hw_hal_pwm_set_freq(uint8_t pin, uint32_t freq_hz);

/* --- Analog Input (ADC) --- */

/** @brief Initialise a pin for ADC reading. */
void hw_hal_adc_init(uint8_t pin);

/** @brief Read raw ADC value (0-4095). */
uint32_t hw_hal_adc_read(uint8_t pin);

/** @brief Read voltage in millivolts (requires calibration). */
uint32_t hw_hal_adc_read_mv(uint8_t pin);

/* --- DAC --- */

/**
 * @brief DAC support (only Pins 25, 26)
 */
void hw_hal_dac_init(uint8_t pin);
void hw_hal_dac_set_voltage(uint8_t pin, uint8_t value);

/* --- Communication Protocols --- */

/** @brief Initialise I2C Master (0). */
void hw_hal_i2c_init(uint8_t sda_pin, uint8_t scl_pin, uint32_t clk_speed);

/** @brief Initialise Secondary UART (1). */
void hw_hal_uart2_init(uint8_t tx_pin, uint8_t rx_pin, uint32_t baud);

#ifdef __cplusplus
}
#endif
