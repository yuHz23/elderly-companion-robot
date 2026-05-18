/*
 * mqtt_client — thin wrapper over esp-mqtt.
 *
 * Boots only when `nvs::mqtt::broker_uri` is set. The broker URI takes
 * mqtt:// or mqtts:// schemes; auth is in-URI (mqtts://user:pass@host).
 *
 * Behaviour command subscriber is intentionally minimal: it just
 * translates the payload string to a task_behavior_request_* call.
 * That keeps the routing logic in one place (task_behavior) — this file
 * is only a transport adapter.
 */

#include "robot_mqtt.h"

#include <string.h>

#include "esp_log.h"
#include "mqtt_client.h"   // esp-mqtt top-level header (component "mqtt")
#include "nvs.h"

#include "task_behavior.h"

static const char *TAG = "mqtt";

#define NVS_NS         "mqtt"
#define TOPIC_PREFIX   "elderly_robot"

static esp_mqtt_client_handle_t s_client = NULL;
static bool s_connected = false;

// -- handlers -------------------------------------------------------------

static void on_data(esp_mqtt_event_handle_t evt)
{
    char topic[64] = {0};
    char data[64]  = {0};
    int tlen = evt->topic_len < (int)sizeof(topic) - 1 ? evt->topic_len : (int)sizeof(topic) - 1;
    int dlen = evt->data_len  < (int)sizeof(data)  - 1 ? evt->data_len  : (int)sizeof(data)  - 1;
    memcpy(topic, evt->topic, tlen);
    memcpy(data,  evt->data,  dlen);

    if (strstr(topic, "cmd/behavior")) {
        if      (!strcmp(data, "idle"))   task_behavior_request_idle();
        else if (!strcmp(data, "patrol")) task_behavior_request_patrol();
        else if (!strcmp(data, "dock"))   task_behavior_request_dock();
        else if (!strcmp(data, "leave"))  task_behavior_request_leave();
        else ESP_LOGW(TAG, "unknown behavior cmd: %s", data);
    }
}

static void event_handler(void *handler_args, esp_event_base_t base,
                          int32_t id, void *data)
{
    esp_mqtt_event_handle_t evt = data;
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected to broker");
        s_connected = true;
        esp_mqtt_client_subscribe(evt->client, TOPIC_PREFIX "/cmd/behavior", 1);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "disconnected");
        s_connected = false;
        break;
    case MQTT_EVENT_DATA:
        on_data(evt);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "broker error");
        break;
    default: break;
    }
}

// -- public ---------------------------------------------------------------

bool mqtt_client_start(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "no broker configured — skipping");
        return false;
    }
    char uri[128] = {0};
    size_t ulen = sizeof(uri);
    esp_err_t err = nvs_get_str(h, "broker_uri", uri, &ulen);
    nvs_close(h);
    if (err != ESP_OK || ulen <= 1) {
        ESP_LOGI(TAG, "no broker_uri — skipping");
        return false;
    }

    ESP_LOGI(TAG, "connecting to %s", uri);
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = uri,
        .credentials.client_id = "elderly-robot",
        .session.keepalive = 30,
    };
    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) return false;
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, event_handler, NULL);
    esp_mqtt_client_start(s_client);
    return true;
}

void mqtt_publish_state_json(const char *json)
{
    if (!s_client || !s_connected) return;
    esp_mqtt_client_publish(s_client, TOPIC_PREFIX "/state", json, 0, 0, 0);
}

void mqtt_publish_event(const char *topic_suffix, const char *payload)
{
    if (!s_client || !s_connected) return;
    char topic[64];
    snprintf(topic, sizeof(topic), TOPIC_PREFIX "/event/%s", topic_suffix);
    esp_mqtt_client_publish(s_client, topic, payload, 0, 1, 0);
}
