/*
 * sim800l — bit-stuffing-free AT command driver for the 2G module.
 *
 * The "trick" with SIM800L is the two-step SMS handshake:
 *   1. AT+CMGS="+84..."          → wait for '>' prompt
 *   2. <message body> + 0x1A     → wait for "+CMGS:" then "OK"
 * Other commands are vanilla request → response with OK/ERROR terminator.
 */

#include "sim800l.h"

#include <string.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sim800";

#define UART_PORT       UART_NUM_1
#define UART_TX_GPIO    48
#define UART_RX_GPIO    46
#define PWRKEY_GPIO     47
#define UART_BAUD       9600
#define RX_BUF_SIZE     1024
#define TX_BUF_SIZE     512

static sim800_state_t s_state = SIM800_OFF;

// -- low level -------------------------------------------------------------

static void uart_flush_rx(void)
{
    uart_flush_input(UART_PORT);
}

static int uart_send(const void *buf, size_t len)
{
    return uart_write_bytes(UART_PORT, buf, len);
}

// Read until either `terminator` substring appears OR timeout elapses.
// Always writes a NUL terminator at the end of `out` (if out_len > 0).
// Returns total bytes read.
static int uart_read_until(char *out, size_t out_len, const char *terminator, uint32_t timeout_ms)
{
    if (out_len == 0) return 0;
    out[0] = '\0';
    int total = 0;
    int64_t deadline = esp_log_timestamp() + timeout_ms;

    while ((int64_t)esp_log_timestamp() < deadline) {
        uint8_t c;
        int n = uart_read_bytes(UART_PORT, &c, 1, pdMS_TO_TICKS(50));
        if (n <= 0) continue;
        if ((size_t)total + 1 < out_len) {
            out[total++] = (char)c;
            out[total] = '\0';
        } else {
            // overflow — drain remaining quietly
            continue;
        }
        if (terminator && strstr(out, terminator)) {
            return total;
        }
    }
    return total;
}

// -- public ----------------------------------------------------------------

bool sim800_init(void)
{
    // PWRKEY: idle HIGH (module not pulsed). Open-drain would be safer if
    // PWRKEY uses internal pull-up, but most Shopee modules accept direct
    // push-pull 3.3V.
    gpio_reset_pin(PWRKEY_GPIO);
    gpio_set_direction(PWRKEY_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(PWRKEY_GPIO, 1);

    uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    if (uart_param_config(UART_PORT, &cfg) != ESP_OK) return false;
    if (uart_set_pin(UART_PORT, UART_TX_GPIO, UART_RX_GPIO,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) return false;
    if (uart_driver_install(UART_PORT, RX_BUF_SIZE, TX_BUF_SIZE, 0, NULL, 0) != ESP_OK) return false;

    s_state = SIM800_OFF;
    ESP_LOGI(TAG, "uart installed — TX=%d RX=%d PWRKEY=%d @ %d baud",
             UART_TX_GPIO, UART_RX_GPIO, PWRKEY_GPIO, UART_BAUD);
    return true;
}

bool sim800_power_on(void)
{
    if (s_state == SIM800_READY) return true;
    s_state = SIM800_POWERING_ON;

    ESP_LOGI(TAG, "pulsing PWRKEY...");
    gpio_set_level(PWRKEY_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PWRKEY_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(1500));
    gpio_set_level(PWRKEY_GPIO, 1);

    // Wait for module to boot + register on network. Try AT a few times.
    for (int attempt = 0; attempt < 15; ++attempt) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (sim800_at("AT", NULL, 0, 500)) {
            // turn off echo so subsequent parsing is cleaner
            sim800_at("ATE0", NULL, 0, 500);
            ESP_LOGI(TAG, "module responsive after %d s", attempt + 1);
            // Check registration (may take a few more seconds)
            for (int r = 0; r < 30; ++r) {
                if (sim800_is_registered()) {
                    int rssi = sim800_signal_quality();
                    ESP_LOGI(TAG, "registered, RSSI=%d", rssi);
                    s_state = SIM800_READY;
                    return true;
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            ESP_LOGW(TAG, "responsive but not registered (no SIM? no signal?)");
            s_state = SIM800_FAULT;
            return false;
        }
    }

    ESP_LOGE(TAG, "no response after 15s — check power/wiring");
    s_state = SIM800_FAULT;
    return false;
}

void sim800_power_off(void)
{
    gpio_set_level(PWRKEY_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(1500));
    gpio_set_level(PWRKEY_GPIO, 1);
    s_state = SIM800_OFF;
}

sim800_state_t sim800_state(void) { return s_state; }

bool sim800_at(const char *cmd, char *resp, size_t resp_len, uint32_t timeout_ms)
{
    uart_flush_rx();
    char line[256];
    int n = snprintf(line, sizeof(line), "%s\r\n", cmd);
    uart_send(line, n);

    char local[512];
    char *buf = resp ? resp : local;
    size_t len = resp ? resp_len : sizeof(local);
    uart_read_until(buf, len, "OK\r\n", timeout_ms);
    return strstr(buf, "OK") != NULL && strstr(buf, "ERROR") == NULL;
}

// -- queries ---------------------------------------------------------------

int sim800_signal_quality(void)
{
    char resp[128];
    if (!sim800_at("AT+CSQ", resp, sizeof(resp), 1000)) return -1;
    // Expected: "+CSQ: <rssi>,<ber>"
    char *p = strstr(resp, "+CSQ:");
    if (!p) return -1;
    int rssi = -1;
    if (sscanf(p, "+CSQ: %d", &rssi) != 1) return -1;
    if (rssi == 99) return -1;   // unknown
    return rssi;
}

bool sim800_is_registered(void)
{
    char resp[128];
    if (!sim800_at("AT+CREG?", resp, sizeof(resp), 1000)) return false;
    // Expected: "+CREG: <n>,<stat>"
    // stat == 1 → home, 5 → roaming, 0 → not registered, 2 → searching
    char *p = strstr(resp, "+CREG:");
    if (!p) return false;
    int n, stat;
    if (sscanf(p, "+CREG: %d,%d", &n, &stat) != 2) return false;
    return stat == 1 || stat == 5;
}

// -- SMS -------------------------------------------------------------------

bool sim800_send_sms(const char *phone, const char *message)
{
    if (s_state != SIM800_READY) return false;

    // Switch to text mode
    if (!sim800_at("AT+CMGF=1", NULL, 0, 500)) {
        ESP_LOGW(TAG, "CMGF=1 failed");
        return false;
    }

    // CMGS, expect '>' prompt
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"\r\n", phone);
    uart_flush_rx();
    uart_send(cmd, strlen(cmd));

    char resp[64];
    uart_read_until(resp, sizeof(resp), ">", 5000);
    if (!strchr(resp, '>')) {
        ESP_LOGW(TAG, "no '>' prompt — got: %s", resp);
        return false;
    }

    // Send body + Ctrl+Z
    uart_send(message, strlen(message));
    uart_send("\x1A", 1);

    char resp2[256];
    uart_read_until(resp2, sizeof(resp2), "OK\r\n", 15000);   // SMS can be slow
    bool ok = strstr(resp2, "+CMGS:") != NULL;
    ESP_LOGI(TAG, "SMS send result: %s", ok ? "OK" : "FAIL");
    return ok;
}

// -- voice call ------------------------------------------------------------

bool sim800_dial(const char *phone)
{
    if (s_state != SIM800_READY) return false;
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "ATD%s;", phone);
    return sim800_at(cmd, NULL, 0, 30000);
}

bool sim800_hangup(void)
{
    return sim800_at("ATH", NULL, 0, 2000);
}
