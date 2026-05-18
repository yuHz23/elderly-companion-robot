/*
 * audio_i2s — INMP441 + MAX98357A on a single I2S controller.
 *
 * Uses the ESP-IDF v5 i2s_std API (driver/i2s_std.h). The legacy
 * driver/i2s.h still works but emits deprecation warnings.
 *
 * INMP441 outputs 24-bit samples padded into 32-bit slots. In 16-bit
 * mode we just take the top 16 MSBs of each slot — loses 8 LSBs but
 * gains 50% memory and is plenty for voice quality at 16 kHz.
 */

#include "audio_i2s.h"

#include <math.h>
#include <string.h>

#include "esp_log.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "audio";

#define BCLK_GPIO   14
#define LRCLK_GPIO  12
#define DOUT_GPIO   16
#define DIN_GPIO    17

#define DMA_DESC_NUM   8
#define DMA_FRAME_NUM  256

static i2s_chan_handle_t s_tx = NULL;
static i2s_chan_handle_t s_rx = NULL;
static bool s_inited = false;

bool audio_i2s_init(void)
{
    if (s_inited) return true;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = DMA_DESC_NUM;
    chan_cfg.dma_frame_num = DMA_FRAME_NUM;
    chan_cfg.auto_clear = true;     // zero-fill TX DMA when underrun → no garbage click

    esp_err_t err = i2s_new_channel(&chan_cfg, &s_tx, &s_rx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(err));
        return false;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT,
                        I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = BCLK_GPIO,
            .ws   = LRCLK_GPIO,
            .dout = DOUT_GPIO,
            .din  = DIN_GPIO,
            .invert_flags = { 0, 0, 0 },
        },
    };

    // INMP441 outputs data on WS LOW (left channel) when L/R = GND.
    // Philips slot mode reads left channel first → matches.
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    if ((err = i2s_channel_init_std_mode(s_tx, &std_cfg)) != ESP_OK ||
        (err = i2s_channel_init_std_mode(s_rx, &std_cfg)) != ESP_OK) {
        ESP_LOGE(TAG, "init_std_mode failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx));

    s_inited = true;
    ESP_LOGI(TAG, "init OK — BCLK=%d WS=%d DOUT=%d DIN=%d, 16kHz/16b/mono",
             BCLK_GPIO, LRCLK_GPIO, DOUT_GPIO, DIN_GPIO);
    return true;
}

size_t audio_i2s_record(int16_t *samples, size_t n_samples)
{
    if (!s_inited || !samples || n_samples == 0) return 0;
    size_t bytes_to_read = n_samples * AUDIO_BYTES_PER_SAMPLE;
    size_t bytes_read = 0;
    esp_err_t err = i2s_channel_read(s_rx, samples, bytes_to_read,
                                     &bytes_read, portMAX_DELAY);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "record err: %s (bytes=%u)", esp_err_to_name(err), (unsigned)bytes_read);
    }
    return bytes_read;
}

size_t audio_i2s_play(const int16_t *samples, size_t n_samples)
{
    if (!s_inited || !samples || n_samples == 0) return 0;
    size_t bytes_to_write = n_samples * AUDIO_BYTES_PER_SAMPLE;
    size_t bytes_written = 0;
    esp_err_t err = i2s_channel_write(s_tx, samples, bytes_to_write,
                                      &bytes_written, portMAX_DELAY);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "play err: %s (bytes=%u)", esp_err_to_name(err), (unsigned)bytes_written);
    }
    return bytes_written;
}

void audio_i2s_play_tone(uint32_t freq_hz, uint32_t duration_ms, uint8_t volume_pct)
{
    if (!s_inited || freq_hz == 0 || duration_ms == 0) return;
    if (volume_pct > 100) volume_pct = 100;

    const size_t chunk = 512;                                      // 32 ms @ 16k mono
    int16_t buf[chunk];
    int16_t amp = (int16_t)((INT16_MAX * volume_pct) / 100);

    const float two_pi_f_over_sr = 2.0f * (float)M_PI * (float)freq_hz / AUDIO_SAMPLE_RATE_HZ;
    uint32_t phase = 0;
    uint32_t total_samples = (AUDIO_SAMPLE_RATE_HZ * duration_ms) / 1000;

    while (total_samples > 0) {
        size_t n = total_samples > chunk ? chunk : total_samples;
        for (size_t i = 0; i < n; ++i) {
            buf[i] = (int16_t)(amp * sinf(two_pi_f_over_sr * phase));
            phase++;
        }
        audio_i2s_play(buf, n);
        total_samples -= n;
    }
}

void audio_i2s_flush(void)
{
    if (!s_inited) return;
    i2s_channel_disable(s_tx);
    i2s_channel_disable(s_rx);
    i2s_channel_enable(s_tx);
    i2s_channel_enable(s_rx);
}
