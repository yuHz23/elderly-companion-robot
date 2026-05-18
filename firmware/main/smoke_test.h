#pragma once

/*
 * Phase 3 smoke-test: brings up WiFi STA + camera, exposes an MJPEG
 * stream at http://<ip>/stream and a JSON status at http://<ip>/status.
 *
 * Returns when the web server has started; blocks no longer than it
 * takes to connect to WiFi (timeout 30s — see WIFI_CONNECT_TIMEOUT_MS).
 */
void smoke_test_run(void);
