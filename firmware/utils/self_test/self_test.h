#pragma once

/*
 * self_test — on-device diagnostic suite.
 *
 * Runs a sequence of safe, fast checks (each well under 1 second) and
 * returns a JSON report. Safe to call any time AFTER boot has settled
 * (typically > 5 s of uptime so SIM800 + WiFi have had a chance to come
 * online).
 *
 * The motor pulse step assumes the robot is on its stand. Call
 * self_test_run_full() with motor_pulse=false to skip that step in
 * production environments.
 */

#include <stdbool.h>
#include <stddef.h>

// Populates `out_json` with a self-test result document. Returns bytes
// written (excluding NUL).
size_t self_test_run(char *out_json, size_t out_len, bool motor_pulse);
