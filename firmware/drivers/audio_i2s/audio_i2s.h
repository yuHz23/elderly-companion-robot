#pragma once

/*
 * I2S full-duplex audio — INMP441 mic + MAX98357A class-D amp.
 *
 * Format: 16 kHz / 16-bit signed / mono / Philips standard.
 *   - 1 second of audio = 32 KB (fits anywhere; multi-second clips
 *     should be allocated from PSRAM via MALLOC_CAP_SPIRAM).
 *
 * The driver owns one I2S_NUM_0 controller with both TX and RX channels
 * enabled, sharing BCLK + LRCLK. Calls are blocking (DMA-backed); use
 * task_audio if you want them inside a task that can be paused.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AUDIO_SAMPLE_RATE_HZ  16000
#define AUDIO_BITS_PER_SAMPLE 16
#define AUDIO_BYTES_PER_SAMPLE 2

bool audio_i2s_init(void);

// Blocking record: fills `samples` int16_t entries. Returns bytes read.
size_t audio_i2s_record(int16_t *samples, size_t n_samples);

// Blocking play: writes `n_samples` int16_t entries. Returns bytes written.
size_t audio_i2s_play(const int16_t *samples, size_t n_samples);

// Convenience: play a sine wave at given freq for given duration.
void audio_i2s_play_tone(uint32_t freq_hz, uint32_t duration_ms, uint8_t volume_pct);

// Drain DMA — call before powering down or switching modes.
void audio_i2s_flush(void);
