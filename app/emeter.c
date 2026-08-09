#include "emeter.h"
#include "adc.h"
#include "debounce.h"
#include <string.h>

#define PWR_SCALE 1600.0f // imp/kWh

#define BASELINE_DARK 2000

#define Q_SIZE 64
#define NOISE_MARGIN 2048
#define MIN_PULSE_WIDTH_MS 11
#define MAX_PULSE_WIDTH_MS 120
#define DEBOUNCE_TIME_MS 15 // wait after pulse before next detection

#define L_TRIG_THRS 330 // ms
#define L_TRIG_SET_THRS (L_TRIG_THRS * 9 / 10)
#define L_TRIG_CLR_THRS (L_TRIG_THRS * 1 / 10)

static uint32_t pulse_acc = 0;
static uint32_t last_pulse_timestamp = 0;

static float power_now = 0;

static uint16_t history[Q_SIZE];
static int history_index = 0;

static debounce_t deb_light_on = {0};

static uint16_t baseline_level = 4095;
static bool light_enabled = false;

void emeter_init(void)
{
	memset(&deb_light_on, 0, sizeof(deb_light_on));
	deb_light_on.cfg_max_thrs = L_TRIG_THRS;
	deb_light_on.cfg_press_thrs = L_TRIG_SET_THRS;
	deb_light_on.cfg_release_thrs = L_TRIG_CLR_THRS;

	for(uint32_t i = 0; i < sizeof(history) / sizeof(history[0]); i++)
	{
		history[i] = 4095 / 2; // Initialize history with middle value
	}
}

void emeter_poll(uint32_t diff_ms)
{
	if(diff_ms == 0) return;
	last_pulse_timestamp += diff_ms;

	static bool prev_on = false, out_st = false;

	if(!prev_on)
	{
		history[history_index] = adc_val.ir;
		history_index = (history_index + 1) % Q_SIZE;

		uint16_t sorted[Q_SIZE];
		for(uint32_t i = 0; i < Q_SIZE; i++)
		{
			sorted[i] = history[i];
		}

		for(uint32_t i = 0; i < Q_SIZE - 1; i++) // bubble
		{
			for(uint32_t j = 0; j < Q_SIZE - i - 1; j++)
			{
				if(sorted[j] > sorted[j + 1])
				{
					uint16_t temp = sorted[j];
					sorted[j] = sorted[j + 1];
					sorted[j + 1] = temp;
				}
			}
		}

		baseline_level = sorted[Q_SIZE / 2];
	}

	static uint32_t pulse_deb_tim = 0, pulse_width_tim = 0, em_pulse_tim = 0;

	bool on = adc_val.ir < baseline_level - NOISE_MARGIN;
	if(on && !prev_on)
	{
		prev_on = true;
		pulse_width_tim = 0;
	}
	else if(!on && prev_on)
	{
		uint32_t pulse_width = pulse_width_tim;
		if(pulse_width >= MIN_PULSE_WIDTH_MS &&
		   pulse_width <= MAX_PULSE_WIDTH_MS)
		{
			pulse_deb_tim = 0;
			out_st = true;
		}
		prev_on = false;
	}

	if(out_st && pulse_deb_tim > DEBOUNCE_TIME_MS) out_st = false;

	static bool prev_out_st = false;
	if(prev_out_st == false && out_st)
	{
		pulse_acc++;
		power_now = em_pulse_tim == 0 ? 0 : 1 / (PWR_SCALE * (float)em_pulse_tim * 0.001f / 3600.0f);
		em_pulse_tim = 0;
	}
	prev_out_st = out_st;

	pulse_width_tim += diff_ms;
	pulse_deb_tim += diff_ms;
	em_pulse_tim += diff_ms;

	debounce_update(&deb_light_on, baseline_level < BASELINE_DARK, diff_ms);
	light_enabled = deb_light_on.state == TRIG_SET;
}

uint32_t emeter_get_cnt(void) { return pulse_acc; }
float emeter_get_power_kw(void) { return power_now; }
float emeter_get_energy_kwh(void) { return pulse_acc / PWR_SCALE; }