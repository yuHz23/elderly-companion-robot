/*
 * i2c_bus — single shared I2C master bus.
 *
 * Idempotent init: any device driver may call i2c_bus_init() during
 * its own setup; later calls are no-ops.
 */

#include "i2c_bus.h"

#include <stdbool.h>

#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "i2c_bus";

#define I2C_PORT       I2C_NUM_0
#define I2C_SDA_GPIO   21
#define I2C_SCL_GPIO   22
#define I2C_FREQ_HZ    400000
#define I2C_TIMEOUT_MS 50

static bool s_inited = false;

void i2c_bus_init(void)
{
    if (s_inited) return;

    i2c_config_t cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = I2C_SDA_GPIO,
        .scl_io_num       = I2C_SCL_GPIO,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,   // belt + braces alongside external 4.7k
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &cfg));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, cfg.mode, 0, 0, 0));
    s_inited = true;
    ESP_LOGI(TAG, "init OK — SDA=%d SCL=%d @ %d kHz",
             I2C_SDA_GPIO, I2C_SCL_GPIO, I2C_FREQ_HZ / 1000);
}

esp_err_t i2c_bus_write(uint8_t addr, const uint8_t *data, size_t len)
{
    return i2c_master_write_to_device(I2C_PORT, addr, data, len,
                                      pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

esp_err_t i2c_bus_read(uint8_t addr, uint8_t reg, uint8_t *out, size_t len)
{
    return i2c_master_write_read_device(I2C_PORT, addr, &reg, 1, out, len,
                                        pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

esp_err_t i2c_bus_write_reg(uint8_t addr, uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { reg, value };
    return i2c_bus_write(addr, buf, sizeof(buf));
}

bool i2c_bus_probe(uint8_t addr)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return err == ESP_OK;
}
