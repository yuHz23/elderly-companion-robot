/*
 * task_audio — async-command dispatcher around audio_i2s.
 *
 * One task with a single command slot (no queue depth — newer command
 * supersedes pending). The task spins on the slot, executes the
 * requested operation, then returns to idle.
 *
 * The recording buffer lives in PSRAM so multi-second clips don't
 * compete with WiFi/camera for internal RAM. Allocated once at start
 * with the maximum size we ever expect to record (10s).
 */

#include "task_audio.h"

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "audio_i2s.h"

static const char *TAG = "task_audio";

#define MAX_RECORD_MS    10000
#define MAX_RECORD_BYTES (AUDIO_SAMPLE_RATE_HZ * (MAX_RECORD_MS / 1000) * AUDIO_BYTES_PER_SAMPLE)

typedef enum { CMD_NONE, CMD_TONE, CMD_RECORD, CMD_LOOPBACK } cmd_t;

typedef struct {
    cmd_t   kind;
    uint32_t arg_freq_hz;
    uint32_t arg_duration_ms;
    uint8_t  arg_volume_pct;
} request_t;

static request_t       s_req = { CMD_NONE };
static SemaphoreHandle_t s_lock;

static audio_state_t s_state = AUDIO_IDLE;
static int16_t      *s_record_buf = NULL;       // PSRAM-allocated
static size_t        s_record_samples = 0;      // last recording length

// -- public ---------------------------------------------------------------

audio_state_t task_audio_state(void) { return s_state; }
size_t        task_audio_buf_samples(void) { return s_record_samples; }
const int16_t *task_audio_last_recording(size_t *n_samples)
{
    if (n_samples) *n_samples = s_record_samples;
    return s_record_buf;
}

static bool post_command(cmd_t k, uint32_t a1, uint32_t a2, uint8_t a3)
{
    if (s_state != AUDIO_IDLE) {
        ESP_LOGW(TAG, "busy in state %d, rejecting command %d", s_state, k);
        return false;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_req.kind = k;
    s_req.arg_freq_hz = a1;
    s_req.arg_duration_ms = a2;
    s_req.arg_volume_pct = a3;
    xSemaphoreGive(s_lock);
    return true;
}

bool task_audio_request_tone(uint32_t freq_hz, uint32_t duration_ms, uint8_t volume_pct)
{
    return post_command(CMD_TONE, freq_hz, duration_ms, volume_pct);
}

bool task_audio_request_record(uint32_t duration_ms)
{
    if (duration_ms == 0 || duration_ms > MAX_RECORD_MS) return false;
    return post_command(CMD_RECORD, 0, duration_ms, 0);
}

bool task_audio_request_loopback(uint32_t duration_ms)
{
    if (duration_ms == 0 || duration_ms > MAX_RECORD_MS) return false;
    return post_command(CMD_LOOPBACK, 0, duration_ms, 0);
}

// -- task -----------------------------------------------------------------

static void do_record(uint32_t duration_ms)
{
    size_t want = (AUDIO_SAMPLE_RATE_HZ * duration_ms) / 1000;
    if (want > MAX_RECORD_BYTES / AUDIO_BYTES_PER_SAMPLE) {
        want = MAX_RECORD_BYTES / AUDIO_BYTES_PER_SAMPLE;
    }
    ESP_LOGI(TAG, "recording %u samples (%lu ms)", (unsigned)want, (unsigned long)duration_ms);
    s_record_samples = 0;

    // Drop the first DMA frame — it typically contains stale junk.
    int16_t throwaway[256];
    audio_i2s_record(throwaway, 256);

    size_t got = audio_i2s_record(s_record_buf, want);
    s_record_samples = got / AUDIO_BYTES_PER_SAMPLE;
    ESP_LOGI(TAG, "recorded %u samples", (unsigned)s_record_samples);
}

static void do_play_buffered(void)
{
    if (s_record_samples == 0) return;
    ESP_LOGI(TAG, "playing %u samples", (unsigned)s_record_samples);
    audio_i2s_play(s_record_buf, s_record_samples);
}

static void audio_task(void *arg)
{
    if (!audio_i2s_init()) {
        ESP_LOGE(TAG, "audio init failed");
        vTaskDelete(NULL);
        return;
    }

    s_record_buf = heap_caps_malloc(MAX_RECORD_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_record_buf) {
        ESP_LOGE(TAG, "no PSRAM for record buf — disabled");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "task started — record buffer %u bytes in PSRAM",
             (unsigned)MAX_RECORD_BYTES);

    while (1) {
        request_t req = { CMD_NONE };
        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (s_req.kind != CMD_NONE) {
            req = s_req;
            s_req.kind = CMD_NONE;
        }
        xSemaphoreGive(s_lock);

        switch (req.kind) {
        case CMD_TONE:
            s_state = AUDIO_TONE;
            audio_i2s_play_tone(req.arg_freq_hz, req.arg_duration_ms, req.arg_volume_pct);
            s_state = AUDIO_IDLE;
            break;

        case CMD_RECORD:
            s_state = AUDIO_RECORDING;
            do_record(req.arg_duration_ms);
            s_state = AUDIO_IDLE;
            break;

        case CMD_LOOPBACK:
            s_state = AUDIO_RECORDING;
            do_record(req.arg_duration_ms);
            s_state = AUDIO_PLAYING;
            do_play_buffered();
            s_state = AUDIO_IDLE;
            break;

        case CMD_NONE:
        default:
            vTaskDelay(pdMS_TO_TICKS(20));
            break;
        }
    }
}

void task_audio_start(void)
{
    s_lock = xSemaphoreCreateMutex();
    xTaskCreate(audio_task, "audio", 8192, NULL, 5, NULL);
}
