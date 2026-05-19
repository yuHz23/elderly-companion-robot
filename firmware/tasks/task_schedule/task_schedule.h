#pragma once

/*
 * task_schedule — daily routine cron, simplified.
 *
 * Six entries (NVS schedule::e0..e5), each storing a (hour, minute, cmd)
 * triple. cmd ∈ {"idle","patrol","dock","leave"}. Every minute the task
 * checks the local time and fires any entry that matches HH:MM exactly.
 * Once fired, it sleeps the rest of the minute before re-checking.
 *
 * Default schedule (HDSD Phase 12 §"Vận hành hằng ngày"):
 *   06:00 leave
 *   08:00 patrol
 *   12:00 dock        (lunch reminder → return-home)
 *   14:00 leave       (afternoon rounds — placeholder)
 *   19:00 patrol
 *   22:00 dock
 *
 * Time source is SNTP — see sntp_time_init() called from main.
 */

#include <stdbool.h>
#include <stdint.h>

#define SCHED_ENTRY_COUNT 6
#define SCHED_CMD_MAX_LEN 16

typedef struct {
    uint8_t hour;       // 0..23, 255 = disabled
    uint8_t minute;     // 0..59
    char    cmd[SCHED_CMD_MAX_LEN];
} sched_entry_t;

void task_schedule_start(void);

bool task_schedule_get(uint8_t idx, sched_entry_t *out);
bool task_schedule_set(uint8_t idx, const sched_entry_t *in);

// Time helpers (SNTP-backed)
void sntp_time_init(void);
bool sntp_time_synced(void);
