#include "adc.h"
#include "debounce.h"
#include "emeter.h"

#define PWR_SCALE 1600.0f // imp/kWh

static uint32_t pulse_acc = 0;
static uint32_t last_pulse_timestamp = 0;

static float power_now = 0;

static debounce_ctrl_t led_ctrl = {0};

void emeter_init(void)
{
    debounce_init(&led_ctrl, 1);
}

void emeter_poll(uint32_t diff_ms)
{
    last_pulse_timestamp += diff_ms;

    debounce_cb(&led_ctrl, adc_val.ir > 2048, diff_ms);
    if(led_ctrl.pressed_shot)
    {
        pulse_acc++;
        power_now = 1 / (PWR_SCALE * (float)last_pulse_timestamp * 0.001f / 3600.0f);
        last_pulse_timestamp = 0;
    }
}

float emeter_get_power_kw(void) { return power_now; }
float emeter_get_energy_kwh(void) { return pulse_acc / PWR_SCALE; }