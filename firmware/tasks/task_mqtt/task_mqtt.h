#pragma once

/*
 * task_mqtt — active publisher to Home Assistant.
 *
 * Builds JSON state from all subsystems and publishes
 * elderly_robot/state at 1 Hz. Also watches the sensor event group
 * and emits one-shot publishes when fall / battery / dock transitions
 * fire, so HA automations can trigger on edges rather than polling.
 *
 * Only spins up if robot_mqtt's broker_uri is configured (NVS
 * mqtt::broker_uri). Without that, this task exits early.
 */

void task_mqtt_start(void);
