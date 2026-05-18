#pragma once

/*
 * SIM800L 2G GSM/GPRS module driver — UART_NUM_1 on GPIO46/48 + PWRKEY GPIO47.
 *
 * Send-and-wait AT command pattern. All calls are synchronous and run on
 * the calling task — wrap them in task_sos if you need to keep them off
 * the HTTP server's worker stack.
 *
 * The driver does not poll for unsolicited result codes (URCs). Incoming
 * SMS / calls are out of scope for Phase 8 — the robot is the sender.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    SIM800_OFF       = 0,
    SIM800_POWERING_ON,
    SIM800_READY,
    SIM800_FAULT,
} sim800_state_t;

bool sim800_init(void);          // installs UART + GPIO, leaves module OFF
bool sim800_power_on(void);      // pulses PWRKEY, waits for network registration
void sim800_power_off(void);     // pulses PWRKEY again
sim800_state_t sim800_state(void);

// Signal quality 0-31 (higher = better), or -1 if unreadable.
int sim800_signal_quality(void);

// Network registration: true if registered home or roaming.
bool sim800_is_registered(void);

// Send a short text message. Both args are NUL-terminated C strings.
// `phone` MUST be in international format ("+84909123456"). Returns
// true if SIM800L acknowledged with +CMGS / OK.
bool sim800_send_sms(const char *phone, const char *message);

// Dial a voice call (returns immediately after OK; call may still ring).
bool sim800_dial(const char *phone);
bool sim800_hangup(void);

// Raw AT — returns true if the response contained "OK" within timeout.
// `resp` (optional) gets the bytes received, NUL-terminated.
bool sim800_at(const char *cmd, char *resp, size_t resp_len, uint32_t timeout_ms);
