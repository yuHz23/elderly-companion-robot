#pragma once

/*
 * WiFi credentials provisioning.
 *
 * Flow:
 *   - On boot, try to load SSID/PSK from NVS namespace "wifi".
 *   - If absent (first boot or wiped), start a softAP "elderly-bot-XXXX"
 *     with a tiny HTML form. User opens http://192.168.4.1/, enters
 *     credentials; the device stores them in NVS and reboots into STA mode.
 *   - In STA mode, if connect fails 5 times in a row, fall back to AP
 *     so the user can re-provision without flashing.
 *
 * Phase 3 only uses the NVS read side via smoke_test. The AP/portal
 * comes online once `wifi_manager_start()` is called from a later phase.
 */

#include <stdbool.h>
#include <stddef.h>

bool wifi_manager_load_creds(char *ssid, size_t ssid_len,
                             char *psk,  size_t psk_len);

bool wifi_manager_save_creds(const char *ssid, const char *psk);

void wifi_manager_start_portal(void);     // softAP + captive page
