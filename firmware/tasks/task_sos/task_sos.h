#pragma once

/*
 * task_sos — auto-dial / auto-SMS when a fall event is detected.
 *
 * Subscribes to EVT_FALL_DETECTED from task_sensor_fusion. When set,
 * it reads the configured contacts from NVS and dispatches:
 *   1. SMS to phone1 and phone2 (if set)
 *   2. Voice dial to phone1 (rings, hangup after timeout)
 *
 * Manual trigger via task_sos_trigger() is also exposed for HTTP /sos/trigger.
 *
 * Contacts and SMS template are persisted in NVS namespace "sos".
 */

#include <stdbool.h>
#include <stdint.h>

#define SOS_PHONE_MAX_LEN  16
#define SOS_SMS_MAX_LEN    160

void task_sos_start(void);

// Manual trigger (called from HTTP or behaviour layer)
void task_sos_trigger(void);

// Config (write to NVS); empty string = clear
bool task_sos_set_phone1(const char *phone_e164);
bool task_sos_set_phone2(const char *phone_e164);
bool task_sos_set_sms_text(const char *text);

// Read current config (NUL-terminated). Returns true if a value is set.
bool task_sos_get_phone1(char *out, size_t out_len);
bool task_sos_get_phone2(char *out, size_t out_len);
bool task_sos_get_sms_text(char *out, size_t out_len);

uint32_t task_sos_trigger_count(void);
int64_t  task_sos_last_trigger_us(void);
