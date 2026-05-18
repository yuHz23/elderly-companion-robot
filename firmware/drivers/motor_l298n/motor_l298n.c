/*
 * motor_l298n — H-bridge driver for two BO motors via an L298N module.
 *
 * Why 20 kHz PWM? The 1 kHz default in many tutorials sits squarely in
 * the audible range, and a robot for an elderly user shouldn't whine.
 * 20 kHz is above hearing yet still well within the L298N switching
 * envelope (the BJT outputs can keep up; switching loss is acceptable).
 */

#include "motor_l298n.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "motor";

// --- Pin mapping (pin-mapping.md section, drive-spec.md section 3) -------
#define ENA_GPIO   6
#define IN1_GPIO   7
#define IN2_GPIO   8
#define IN3_GPIO   9
#define IN4_GPIO  10
#define ENB_GPIO  11

#define MOTOR_TIMER       LEDC_TIMER_2
#define MOTOR_MODE        LEDC_LOW_SPEED_MODE
#define MOTOR_FREQ_HZ     20000           // silent
#define MOTOR_RESOLUTION  LEDC_TIMER_10_BIT  // 1024 ticks; 5% steps are plenty
#define MOTOR_DUTY_MAX    1023

#define LEFT_PWM_CH       LEDC_CHANNEL_4
#define RIGHT_PWM_CH      LEDC_CHANNEL_5

// --- Dead-zone & cap ------------------------------------------------------
// BO motor stalls below ~20 % duty; mapping makes 1 % logical → 21 % real.
#define DEAD_ZONE_PCT   20
// L298N drops ~2.5V on 12V input → 9.5V on output; cap at 70 % keeps the
// 6V BO motor under its rated voltage.
#define PWM_CAP_PCT     70

// --- Direction state ------------------------------------------------------
// Track last commanded direction so motor_set() with the same dir + new
// speed avoids re-asserting GPIOs (saves a few cycles, but mainly makes
// scope traces less noisy when watching IN pins during dev).
static motor_dir_t s_last_dir[MOTOR_COUNT] = { MOTOR_BRAKE, MOTOR_BRAKE };

// --- Helpers --------------------------------------------------------------

static uint32_t pct_to_duty(uint8_t speed_pct)
{
    if (speed_pct == 0) return 0;
    if (speed_pct > PWM_CAP_PCT) speed_pct = PWM_CAP_PCT;
    // Logical 1..100 → real (DEAD_ZONE + ..)
    uint32_t real = DEAD_ZONE_PCT +
                    ((uint32_t)speed_pct * (PWM_CAP_PCT - DEAD_ZONE_PCT)) / 100;
    return (real * MOTOR_DUTY_MAX) / 100;
}

static void set_dir_gpios(motor_side_t side, motor_dir_t dir)
{
    int in_a = (side == MOTOR_LEFT) ? IN1_GPIO : IN3_GPIO;
    int in_b = (side == MOTOR_LEFT) ? IN2_GPIO : IN4_GPIO;
    int a = 0, b = 0;
    switch (dir) {
        case MOTOR_COAST: a = 0; b = 0; break;
        case MOTOR_FWD:   a = 1; b = 0; break;
        case MOTOR_REV:   a = 0; b = 1; break;
        case MOTOR_BRAKE: a = 1; b = 1; break;
    }
    gpio_set_level(in_a, a);
    gpio_set_level(in_b, b);
}

static ledc_channel_t pwm_channel(motor_side_t side)
{
    return (side == MOTOR_LEFT) ? LEFT_PWM_CH : RIGHT_PWM_CH;
}

// --- Init -----------------------------------------------------------------

void motor_init(void)
{
    // Direction GPIOs as outputs, default LOW (coast)
    const int dir_pins[] = { IN1_GPIO, IN2_GPIO, IN3_GPIO, IN4_GPIO };
    for (size_t i = 0; i < sizeof(dir_pins)/sizeof(dir_pins[0]); ++i) {
        gpio_reset_pin(dir_pins[i]);
        gpio_set_direction(dir_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(dir_pins[i], 0);
    }

    // LEDC timer 2 for motor PWM (timer 0 = camera XCLK, timer 1 = servo)
    ledc_timer_config_t timer = {
        .speed_mode      = MOTOR_MODE,
        .timer_num       = MOTOR_TIMER,
        .duty_resolution = MOTOR_RESOLUTION,
        .freq_hz         = MOTOR_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t left = {
        .speed_mode = MOTOR_MODE,
        .channel    = LEFT_PWM_CH,
        .timer_sel  = MOTOR_TIMER,
        .gpio_num   = ENA_GPIO,
        .duty       = 0,
        .hpoint     = 0,
        .intr_type  = LEDC_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&left));

    ledc_channel_config_t right = left;
    right.channel  = RIGHT_PWM_CH;
    right.gpio_num = ENB_GPIO;
    ESP_ERROR_CHECK(ledc_channel_config(&right));

    motor_stop_all();   // brake on init — never spin during boot
    ESP_LOGI(TAG, "init OK — ENA=%d ENB=%d, 20kHz/10-bit, cap=%d%%, dz=%d%%",
             ENA_GPIO, ENB_GPIO, PWM_CAP_PCT, DEAD_ZONE_PCT);
}

// --- Public API -----------------------------------------------------------

void motor_set(motor_side_t side, motor_dir_t dir, uint8_t speed_pct)
{
    if (side >= MOTOR_COUNT) return;
    if (speed_pct > 100) speed_pct = 100;

    if (dir != s_last_dir[side]) {
        set_dir_gpios(side, dir);
        s_last_dir[side] = dir;
    }

    // Brake / coast → duty 0 (direction pins do the work)
    uint32_t duty = (dir == MOTOR_FWD || dir == MOTOR_REV) ? pct_to_duty(speed_pct) : 0;
    ledc_set_duty(MOTOR_MODE, pwm_channel(side), duty);
    ledc_update_duty(MOTOR_MODE, pwm_channel(side));
}

void motor_stop_all(void)
{
    motor_set(MOTOR_LEFT,  MOTOR_BRAKE, 0);
    motor_set(MOTOR_RIGHT, MOTOR_BRAKE, 0);
}

void motor_coast_all(void)
{
    motor_set(MOTOR_LEFT,  MOTOR_COAST, 0);
    motor_set(MOTOR_RIGHT, MOTOR_COAST, 0);
}
