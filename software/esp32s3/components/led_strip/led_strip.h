/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Ruslan V. Uss <unclerus@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file led_strip.h
 * @defgroup led_strip led_strip
 * @{
 *
 * RMT-based ESP-IDF driver for WS2812B/SK6812/APA106/SM16703 LED strips
 *
 * Copyright (c) 2020 Ruslan V. Uss <unclerus@gmail.com>
 *
 * MIT Licensed as described in the file LICENSE
 */
#ifndef __LED_STRIP_H__
#define __LED_STRIP_H__

#include <driver/gpio.h>
#include <esp_err.h>
#include <driver/rmt.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * LED type
 */
typedef enum
{
    LED_STRIP_WS2812 = 0,
    LED_STRIP_SK6812,
    LED_STRIP_APA106,
    LED_STRIP_SM16703,

    LED_STRIP_TYPE_MAX
} led_strip_type_t;

/**
 * LED strip descriptor
 */
typedef struct
{
    led_strip_type_t type; ///< LED type
    bool is_rgbw;          ///< true for RGBW strips
    size_t length;         ///< Number of LEDs in strip
    gpio_num_t gpio;       ///< Data GPIO pin
    rmt_channel_t channel; ///< RMT channel
    uint8_t *buf;
} led_strip_t;
 
/**
 * @brief Setup library
 *
 * This method must be called before any other led_strip methods
 */
void led_start(led_strip_t *strip);

/**
 * @brief Send strip buffer to LEDs
 *
 * @param strip Descriptor of LED strip
 * @return `ESP_OK` on success
 */
esp_err_t led_strip_flush(led_strip_t *strip);

/**
 * @brief Check if associated RMT channel is busy
 *
 * @param strip Descriptor of LED strip
 * @return true if RMT peripherals is busy
 */

/**
 * @brief Set color of single LED in strip
 *
 * This function does not actually change colors of the LEDs.
 * Call ::led_strip_flush() to send buffer to the LEDs.
 *
 * @param strip Descriptor of LED strip
 * @param num LED number, 0..strip length - 1
 * @param color RGB color
 * @return `ESP_OK` on success
 */
esp_err_t led_strip_set_pixel(led_strip_t *strip, size_t num, size_t r, size_t g, size_t b, size_t w);


#ifdef __cplusplus
}
#endif

/**@}*/

#endif /* __LED_STRIP_H__ */
