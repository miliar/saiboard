#include <wifi.h>
#include "esp_log.h"
#include "lwip/sockets.h"
#include <led_strip.h>
#include "esp_adc/adc_oneshot.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include "driver/touch_pad.h"

#define PORT 3333
#define KEEPALIVE_IDLE 5
#define KEEPALIVE_INTERVAL 5
#define KEEPALIVE_COUNT 3
#define EXAMPLE_ADC1_CHAN0 ADC_CHANNEL_0 // Pin 1
#define EXAMPLE_ADC1_CHAN1 ADC_CHANNEL_1 // Pin 2
#define EXAMPLE_ADC1_CHAN2 ADC_CHANNEL_2 // Pin 3
#define EXAMPLE_ADC1_CHAN3 ADC_CHANNEL_3 // Pin 4
#define EXAMPLE_ADC1_CHAN4 ADC_CHANNEL_4 // Pin 5
#define EXAMPLE_ADC1_CHAN5 ADC_CHANNEL_5 // Pin 6
#define EXAMPLE_ADC1_CHAN6 ADC_CHANNEL_6 // Pin 7
#define TOUCH_PIN TOUCH_PAD_NUM8
#define A0_AT_12 GPIO_NUM_41
#define A1_AT_11 GPIO_NUM_40
#define A2_AT_10 GPIO_NUM_39
#define A3_AT_9 GPIO_NUM_38
#define E_AT_6 GPIO_NUM_37
#define S1_AT_2 GPIO_NUM_16
#define S0_AT_3 GPIO_NUM_15
#define LED_PIN_AT_5 GPIO_NUM_17 // PIN 17

static const char *TAG = "TCP/IP socket server";
static adc_oneshot_unit_handle_t adc1_handle;
static led_strip_t strip = {
    .type = LED_STRIP_SK6812,
    .length = 361,
    .gpio = LED_PIN_AT_5,
    .buf = NULL,
    .is_rgbw = true,
};

void _sensor_endpoint(cJSON *answer)
{
    ESP_LOGI(TAG, "Sensors");
    cJSON *hall = cJSON_AddArrayToObject(answer, "hall");
    static uint8_t demux[19][4] = {
        {0, 0, 0, 1}, // 0
        {1, 0, 0, 1}, // 1
        {0, 1, 0, 1}, // 2
        {0, 0, 0, 0}, // 3
        {1, 0, 0, 0}, // 4
        {0, 1, 0, 0}, // 5
        {1, 1, 0, 0}, // 6
        {0, 0, 1, 0}, // 7
        {1, 0, 1, 0}, // 8
        {0, 1, 1, 0}, // 9
        {1, 1, 1, 0}, // 10
        {1, 1, 1, 0}, // 11
        {0, 1, 1, 0}, // 12
        {1, 0, 1, 0}, // 13
        {0, 0, 1, 0}, // 14
        {1, 1, 0, 0}, // 15
        {0, 1, 0, 0}, // 16
        {1, 0, 0, 0}, // 17
        {0, 0, 0, 0}  // 18
    };
    for (int8_t r = 18; r >= 0; r--)
    {
        cJSON *row = cJSON_CreateArray();
        gpio_set_level(E_AT_6, (r > 10) ? 1 : 0);
        gpio_set_level(A0_AT_12, demux[r][0]);
        gpio_set_level(A1_AT_11, demux[r][1]);
        gpio_set_level(A2_AT_10, demux[r][2]);
        gpio_set_level(A3_AT_9, demux[r][3]);
        gpio_set_level(S0_AT_3, 0);
        gpio_set_level(S1_AT_2, 0);
        vTaskDelay(1 / portTICK_PERIOD_MS);
        int raw[19];
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN0, &raw[8]));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN1, &raw[9]));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN2, &raw[10]));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN3, &raw[0]));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN4, &raw[1]));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN5, &raw[11]));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN6, &raw[12]));
        gpio_set_level(S0_AT_3, 1);
        gpio_set_level(S1_AT_2, 0);
        vTaskDelay(1 / portTICK_PERIOD_MS);
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN3, &raw[2]));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN4, &raw[3]));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN5, &raw[13]));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN6, &raw[14]));
        gpio_set_level(S0_AT_3, 0);
        gpio_set_level(S1_AT_2, 1);
        vTaskDelay(1 / portTICK_PERIOD_MS);
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN3, &raw[4]));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN4, &raw[5]));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN5, &raw[15]));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN6, &raw[16]));
        gpio_set_level(S0_AT_3, 1);
        gpio_set_level(S1_AT_2, 1);
        vTaskDelay(1 / portTICK_PERIOD_MS);
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN3, &raw[6]));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN4, &raw[7]));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN5, &raw[17]));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN6, &raw[18]));

        for (uint8_t c = 0; c < 19; c++)
        {
            cJSON *c_raw = cJSON_CreateNumber(raw[c]);
            cJSON_AddItemToArray(row, c_raw);
        }
        cJSON_AddItemToArray(hall, row);
    }
    uint32_t touch_value;
    touch_pad_read_raw_data(TOUCH_PIN, &touch_value);
    cJSON_AddNumberToObject(answer, "touch",touch_value);
}

int _row_col_to_nr(int row, int col)
{
    int nr;
    if (col < 8)
    {
        nr = row * 8 + (7 - col) * (row % 2) + col * !(row % 2);
    }
    else if (col > 10)
    {
        nr = row * 8 + 208 + (19 - col) * (row % 2) + (col - 10) * !(row % 2);
    }
    else
    {
        nr = 151 + (18 - row) * 3 + (11 - col) * (row % 2) + (col - 7) * !(row % 2);
    }
    return nr;
}

void _led_endpoint(cJSON *answer, const cJSON *leds)
{
    ESP_LOGI(TAG, "Leds");
    const cJSON *led = NULL;
    cJSON_ArrayForEach(led, leds)
    {
        cJSON *row = cJSON_GetArrayItem(led, 0);
        cJSON *col = cJSON_GetArrayItem(led, 1);
        cJSON *red = cJSON_GetArrayItem(led, 2);
        cJSON *green = cJSON_GetArrayItem(led, 3);
        cJSON *blue = cJSON_GetArrayItem(led, 4);
        cJSON *white = cJSON_GetArrayItem(led, 5);

        if (cJSON_IsNumber(red) && cJSON_IsNumber(green) && cJSON_IsNumber(blue) && cJSON_IsNumber(white))
        {
            ESP_LOGI(TAG, "pos: (%d,%d) r: %d g: %d b: %d, w: %d", row->valueint, col->valueint, red->valueint, green->valueint, blue->valueint, white->valueint);
            led_strip_set_pixel(&strip, _row_col_to_nr(row->valueint, col->valueint), red->valueint, green->valueint, blue->valueint, white->valueint);
        }
    }
    ESP_ERROR_CHECK(led_strip_flush(&strip));
    cJSON_AddStringToObject(answer, "status", "OK");
}

int _process_message(char *message, size_t size)
{
    cJSON *root = cJSON_Parse(message);
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    cJSON *answer = cJSON_CreateObject();
    if (cJSON_IsString(name) && (name->valuestring != NULL) && strcmp(name->valuestring, "led") == 0)
    {
        const cJSON *leds = cJSON_GetObjectItemCaseSensitive(root, "leds");
        _led_endpoint(answer, leds);
    }
    else
    {
        _sensor_endpoint(answer);
    }
    memset(message, 0, size);
    char *answer_string = cJSON_PrintUnformatted(answer);
    ESP_LOGI(TAG, "%s", answer_string);
    strncpy(message, answer_string, size);
    free(answer_string);
    cJSON_Delete(answer);
    cJSON_Delete(root);
    return strlen(message) + 1;
}

static void _do_retransmit(const int sock)
{
    int len;
    char rx_buffer[10000]; // should be enough for max json send/recv

    do
    {
        memset(rx_buffer, 0, sizeof(rx_buffer)); // Clear the buffer.
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0)
        {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        }
        else if (len == 0)
        {
            ESP_LOGW(TAG, "Connection closed");
        }
        else
        {
            while (rx_buffer[len - 1] != '}')
            {
                len += recv(sock, rx_buffer + len, sizeof(rx_buffer) - len, 0);
            }
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);
            len = _process_message(rx_buffer, sizeof(rx_buffer));

            // send() can return less bytes than supplied length.
            // Walk-around for robust implementation.
            int to_write = len;
            while (to_write > 0)
            {
                int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
                if (written < 0)
                {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                }
                to_write -= written;
            }
        }
    } while (len > 0);
}

static void tcp_server_task(void *pvParameters)
{
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;

    if (addr_family == AF_INET)
    {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;
    }

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0)
    {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);
    if (err != 0)
    {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1)
    {

        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Set tcp keepalive option
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
        // Convert ip address to string
        if (source_addr.ss_family == PF_INET)
        {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }
        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        _do_retransmit(sock);
        ESP_LOGI(TAG, "Shutdown socket");
        shutdown(sock, 0);
        close(sock);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

void init_analog_read(void)
{
    //-------------ADC1 Init---------------//
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN0, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN1, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN2, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN3, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN4, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN5, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN6, &config));
}

void init_touch_sensor(void)
{
    touch_pad_init();
    touch_pad_config(TOUCH_PIN);
    //touch_pad_set_voltage(TOUCH_HVOLT_MAX,  TOUCH_LVOLT_MAX, TOUCH_HVOLT_ATTEN_MAX);
    touch_pad_denoise_t denoise = {
        /* The bits to be cancelled are determined according to the noise level. */
        .grade = TOUCH_PAD_DENOISE_BIT4,
        .cap_level = TOUCH_PAD_DENOISE_CAP_L4,
    };
    touch_pad_denoise_set_config(&denoise);
    touch_pad_denoise_enable();
    /* Enable touch sensor clock. Work mode is "timer trigger". */
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_fsm_start();
}

void init_all(void)
{
    led_start(&strip);
    init_analog_read();
    init_touch_sensor();
    wifi_connect();
    gpio_reset_pin(A0_AT_12);
    gpio_set_direction(A0_AT_12, GPIO_MODE_OUTPUT);
    gpio_reset_pin(A1_AT_11);
    gpio_set_direction(A1_AT_11, GPIO_MODE_OUTPUT);
    gpio_reset_pin(A2_AT_10);
    gpio_set_direction(A2_AT_10, GPIO_MODE_OUTPUT);
    gpio_reset_pin(A3_AT_9);
    gpio_set_direction(A3_AT_9, GPIO_MODE_OUTPUT);
    gpio_reset_pin(E_AT_6);
    gpio_set_direction(E_AT_6, GPIO_MODE_OUTPUT);
    gpio_reset_pin(S1_AT_2);
    gpio_set_direction(S1_AT_2, GPIO_MODE_OUTPUT);
    gpio_reset_pin(S0_AT_3);
    gpio_set_direction(S0_AT_3, GPIO_MODE_OUTPUT);
}

void app_main(void)
{
    init_all();
    xTaskCreate(tcp_server_task, "tcp_server", 32768, (void *)AF_INET, 5, NULL);
}
