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
 * @file led_strip.c
 *
 * RMT-based ESP-IDF driver for WS2812B/SK6812/APA106/SM16703 LED strips
 *
 * Copyright (c) 2020 Ruslan V. Uss <unclerus@gmail.com>
 *
 * MIT Licensed as described in the file LICENSE
 */
#include "led_strip.h"
#include <esp_log.h>
#include <esp_attr.h>
#include <stdlib.h>
#include <esp32s3/rom/ets_sys.h>

#ifndef RMT_DEFAULT_CONFIG_TX
#define RMT_DEFAULT_CONFIG_TX(gpio, channel_id)      \
    {                                                \
        .rmt_mode = RMT_MODE_TX,                     \
        .channel = channel_id,                       \
        .gpio_num = gpio,                            \
        .clk_div = 80,                               \
        .mem_block_num = 1,                          \
        .tx_config = {                               \
            .carrier_freq_hz = 38000,                \
            .carrier_level = RMT_CARRIER_LEVEL_HIGH, \
            .idle_level = RMT_IDLE_LEVEL_LOW,        \
            .carrier_duty_percent = 33,              \
            .carrier_en = false,                     \
            .loop_en = false,                        \
            .idle_output_en = true,                  \
        }                                            \
    }
#endif

static const char *TAG = "led_strip";

#define LED_STRIP_RMT_CLK_DIV 2

#define CHECK(x) do { esp_err_t __; if ((__ = x) != ESP_OK) return __; } while (0)
#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)

#define COLOR_SIZE(strip) (3 + ((strip)->is_rgbw != 0))

#define LED_STRIP_FLUSH_TIMEOUT 1000
#define LED_STRIP_PAUSE_LENGTH 50

static void IRAM_ATTR _rmt_adapter(const void *src, rmt_item32_t *dest, size_t src_size,
                                   size_t wanted_num, size_t *translated_size, size_t *item_num,
                                   const rmt_item32_t *bit0, const rmt_item32_t *bit1)
{
    if (!src || !dest)
    {
        *translated_size = 0;
        *item_num = 0;
        return;
    }
    size_t size = 0;
    size_t num = 0;
    uint8_t *psrc = (uint8_t *)src;
    rmt_item32_t *pdest = dest;
    while (size < src_size && num < wanted_num)
    {
        uint8_t b = *psrc;
        for (int i = 0; i < 8; i++)
        {
            // MSB first
            pdest->val = b & (1 << (7 - i)) ? bit1->val : bit0->val;
            num++;
            pdest++;
        }
        size++;
        psrc++;
    }
    *translated_size = size;
    *item_num = num;
}

typedef struct {
    rmt_item32_t bit0, bit1;
} led_rmt_t;

static led_rmt_t rmt_items[LED_STRIP_TYPE_MAX] = { 0 };

static void IRAM_ATTR ws2812_rmt_adapter(const void *src, rmt_item32_t *dest, size_t src_size,
        size_t wanted_num, size_t *translated_size, size_t *item_num)
{
    _rmt_adapter(src, dest, src_size, wanted_num, translated_size, item_num, &rmt_items[LED_STRIP_WS2812].bit0, &rmt_items[LED_STRIP_WS2812].bit1);
}

static void IRAM_ATTR sk6812_rmt_adapter(const void *src, rmt_item32_t *dest, size_t src_size,
        size_t wanted_num, size_t *translated_size, size_t *item_num)
{
    _rmt_adapter(src, dest, src_size, wanted_num, translated_size, item_num, &rmt_items[LED_STRIP_SK6812].bit0, &rmt_items[LED_STRIP_SK6812].bit1);
}

static void IRAM_ATTR apa106_rmt_adapter(const void *src, rmt_item32_t *dest, size_t src_size,
        size_t wanted_num, size_t *translated_size, size_t *item_num)
{
    _rmt_adapter(src, dest, src_size, wanted_num, translated_size, item_num, &rmt_items[LED_STRIP_APA106].bit0, &rmt_items[LED_STRIP_APA106].bit1);
}

static void IRAM_ATTR sm16703_rmt_adapter(const void *src, rmt_item32_t *dest, size_t src_size,
                                         size_t wanted_num, size_t *translated_size, size_t *item_num)
{
    _rmt_adapter(src, dest, src_size, wanted_num, translated_size, item_num, &rmt_items[LED_STRIP_SM16703].bit0, &rmt_items[LED_STRIP_SM16703].bit1);
}

typedef enum {
    ORDER_GRB,
    ORDER_RGB,
} color_order_t;

typedef struct {
    uint32_t t0h, t0l, t1h, t1l;
    color_order_t order;
    sample_to_rmt_t adapter;
} led_params_t;

static const led_params_t led_params[] = {
    [LED_STRIP_WS2812]  = { .t0h = 400, .t0l = 1000, .t1h = 1000, .t1l = 400, .order = ORDER_GRB, .adapter = ws2812_rmt_adapter },
    [LED_STRIP_SK6812]  = { .t0h = 300, .t0l = 900,  .t1h = 600,  .t1l = 600, .order = ORDER_GRB, .adapter = sk6812_rmt_adapter },
    [LED_STRIP_APA106]  = { .t0h = 350, .t0l = 1360, .t1h = 1360, .t1l = 350, .order = ORDER_RGB, .adapter = apa106_rmt_adapter },
    [LED_STRIP_SM16703] = { .t0h = 300, .t0l = 900,  .t1h = 1360, .t1l = 350, .order = ORDER_RGB, .adapter = sm16703_rmt_adapter },
};

///////////////////////////////////////////////////////////////////////////////

void led_strip_install()
{
    float ratio = (float)APB_CLK_FREQ / LED_STRIP_RMT_CLK_DIV / 1e09f;

    for (size_t i = 0; i < LED_STRIP_TYPE_MAX; i++)
    {
        // 0 bit
        rmt_items[i].bit0.duration0 = (uint32_t)(ratio * led_params[i].t0h);
        rmt_items[i].bit0.level0 = 1;
        rmt_items[i].bit0.duration1 = (uint32_t)(ratio * led_params[i].t0l);
        rmt_items[i].bit0.level1 = 0;
        // 1 bit
        rmt_items[i].bit1.duration0 = (uint32_t)(ratio * led_params[i].t1h);
        rmt_items[i].bit1.level0 = 1;
        rmt_items[i].bit1.duration1 = (uint32_t)(ratio * led_params[i].t1l);
        rmt_items[i].bit1.level1 = 0;
    }
}

esp_err_t led_strip_init(led_strip_t *strip)
{
    CHECK_ARG(strip && strip->length > 0 && strip->type < LED_STRIP_TYPE_MAX);

    strip->buf = calloc(strip->length, COLOR_SIZE(strip));
    if (!strip->buf)
    {
        ESP_LOGE(TAG, "Not enough memory");
        return ESP_ERR_NO_MEM;
    }

    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(strip->gpio, strip->channel);
    config.clk_div = LED_STRIP_RMT_CLK_DIV;

    CHECK(rmt_config(&config));
    CHECK(rmt_driver_install(config.channel, 0, 0));

    CHECK(rmt_translator_init(config.channel, led_params[strip->type].adapter));

    return ESP_OK;
}

void led_start(led_strip_t *strip)
{
    led_strip_install();
    ESP_ERROR_CHECK(led_strip_init(strip));
}

esp_err_t led_strip_flush(led_strip_t *strip)
{
    CHECK_ARG(strip && strip->buf);

    CHECK(rmt_wait_tx_done(strip->channel, pdMS_TO_TICKS(LED_STRIP_FLUSH_TIMEOUT)));
    ets_delay_us(LED_STRIP_PAUSE_LENGTH);
    return rmt_write_sample(strip->channel, strip->buf,
                            strip->length * COLOR_SIZE(strip), false);
}

esp_err_t led_strip_set_pixel(led_strip_t *strip, size_t num, size_t r, size_t g,  size_t b, size_t w)
{
    CHECK_ARG(strip && strip->buf && num <= strip->length);
    size_t idx = num * COLOR_SIZE(strip);
    switch (led_params[strip->type].order)
    {
        case ORDER_GRB:
            strip->buf[idx] = g;
            strip->buf[idx + 1] = r;
            strip->buf[idx + 2] = b;
            if (strip->is_rgbw)
                strip->buf[idx + 3] = w;
            break;
        case ORDER_RGB:
            strip->buf[idx] = r;
            strip->buf[idx + 1] = g;
            strip->buf[idx + 2] = b;
            if (strip->is_rgbw)
                strip->buf[idx + 3] = w;
            break;
    }
    return ESP_OK;
}
