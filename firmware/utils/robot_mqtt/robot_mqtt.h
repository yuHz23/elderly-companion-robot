#pragma once

/*
 * MQTT client scaffold for Home Assistant integration.
 *
 * Disabled by default — only spins up if NVS namespace "mqtt" contains
 * a non-empty `broker_uri`. This keeps the firmware happy for users who
 * don't run a broker.
 *
 * Topics: see docs/firmware/architecture.md §6.
 *
 * Phase 10 ships publish-only + a single subscription (cmd/behavior).
 * Richer command handling (PTZ, TTS, SOS) is wired up in Phase 12.
 */

#include <stdbool.h>

bool mqtt_client_start(void);    // returns true if broker URI was configured & connect attempted

// Convenience publishers
void mqtt_publish_state_json(const char *json);
void mqtt_publish_event(const char *topic_suffix, const char *payload);
