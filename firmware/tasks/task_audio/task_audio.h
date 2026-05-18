#pragma once

/*
 * task_audio — owns the audio HW state machine.
 *
 * Phase 7 deliverables: tone, record-to-buffer, loopback. The full
 * voice pipeline (wake word → STT → LLM → TTS) plugs in here later.
 *
 * The task is mostly idle; HTTP handlers dispatch one-shot commands by
 * writing to a request struct and the task picks them up on the next
 * tick. This keeps long-running audio off the HTTPD's worker stack.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    AUDIO_IDLE = 0,
    AUDIO_RECORDING,
    AUDIO_PLAYING,
    AUDIO_LOOPBACK,
    AUDIO_TONE,
} audio_state_t;

void task_audio_start(void);

audio_state_t task_audio_state(void);
size_t        task_audio_buf_samples(void);

// Asynchronous commands (return immediately, task does work in background)
bool task_audio_request_tone(uint32_t freq_hz, uint32_t duration_ms, uint8_t volume_pct);
bool task_audio_request_record(uint32_t duration_ms);
bool task_audio_request_loopback(uint32_t duration_ms);

// Synchronous accessor — caller must NOT call while AUDIO_RECORDING is in
// progress. Returns pointer + length of the most recent recording.
const int16_t *task_audio_last_recording(size_t *n_samples);
