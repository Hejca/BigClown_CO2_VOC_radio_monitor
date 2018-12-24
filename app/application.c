#include <application.h>

// Application timming
#define SERVICE_INTERVAL_INTERVAL (60 * 60 * 1000)
#define BATTERY_UPDATE_INTERVAL (60 * 60 * 1000)

#define TEMPERATURE_TAG_PUB_NO_CHANGE_INTEVAL (15 * 60 * 1000)
#define TEMPERATURE_TAG_PUB_VALUE_CHANGE 0.2f
#define TEMPERATURE_UPDATE_SERVICE_INTERVAL (5 * 1000)
#define TEMPERATURE_UPDATE_NORMAL_INTERVAL (10 * 1000)

#define HUMIDITY_TAG_PUB_NO_CHANGE_INTEVAL (15 * 60 * 1000)
#define HUMIDITY_TAG_PUB_VALUE_CHANGE 5.0f
#define HUMIDITY_TAG_UPDATE_SERVICE_INTERVAL (5 * 1000)
#define HUMIDITY_TAG_UPDATE_NORMAL_INTERVAL (10 * 1000)

/*#define BAROMETER_TAG_PUB_NO_CHANGE_INTEVAL (15 * 60 * 1000)
#define BAROMETER_TAG_PUB_VALUE_CHANGE 20.0f
#define BAROMETER_TAG_UPDATE_SERVICE_INTERVAL (1 * 60 * 1000)
#define BAROMETER_TAG_UPDATE_NORMAL_INTERVAL  (5 * 60 * 1000) */

#define CO2_PUB_NO_CHANGE_INTERVAL (15 * 60 * 1000)
#define CO2_PUB_VALUE_CHANGE 50.0f
#define CO2_UPDATE_SERVICE_INTERVAL (1 * 60 * 1000)
#define CO2_UPDATE_NORMAL_INTERVAL  (5 * 60 * 1000)

#define VOC_TAG_PUB_NO_CHANGE_INTEVAL (15*60 * 1000)
#define VOC_TAG_PUB_VALUE_CHANGE 50.0f
#define VOC_TAG_UPDATE_SERVICE_INTERVAL (5 * 1000)
#define VOC_TAG_UPDATE_NORMAL_INTERVAL (10 * 1000)

#define CALIBRATION_DELAY (10 * 60 * 1000)


// Sensors, buttons and LEDs Definition used in project
// LED instance - LED on Core module
bc_led_t led;

// Button instance - Button on Core module
bc_button_t button;

// Temperature tag instance
bc_tag_temperature_t temperature;
event_param_t temperature_event_param = { .next_pub = 0 };

// Humidity tag instance
bc_tag_humidity_t humidity;
event_param_t humidity_event_param = { .next_pub = 0 };

// CO2 module
event_param_t co2_event_param = { .next_pub = 0 };

//VOC tag instance
bc_tag_voc_lp_t tag_voc_lp;
event_param_t voc_lp_event_param = { .next_pub = 0 };

// Global parameters
float teplota = NAN; //temperature store for VOC correction
float vlhkost = NAN; //humidity store for VOC correction

//Event handlers
//required functions
// CO2 module callibration
void calibration_task(void *param)
{
    (void) param;

    bc_led_set_mode(&led, BC_LED_MODE_OFF);

    bc_module_co2_calibration(BC_LP8_CALIBRATION_BACKGROUND_FILTERED);

    bc_scheduler_unregister(bc_scheduler_get_current_task_id());
}


// Button
void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == BC_BUTTON_EVENT_PRESS)
    {
        bc_led_pulse(&led, 100);  //Led pulse when button pressed
    }
    else if (event == BC_BUTTON_EVENT_HOLD)
    {
      bc_led_set_mode(&led, BC_LED_MODE_BLINK); //Core module LED start blinking

      bc_scheduler_register(calibration_task, NULL, bc_tick_get() + CALIBRATION_DELAY); //Publishing the callibration request to the scheduler
    }
}

// Send battery voltage
void battery_event_handler(bc_module_battery_event_t event, void *event_param)
{
    (void) event_param;

    float voltage;

    if (event == BC_MODULE_BATTERY_EVENT_UPDATE)
    {
        if (bc_module_battery_get_voltage(&voltage))
        {
            bc_radio_pub_battery(&voltage);
        }
    }
}

// Temperature measurement
void temperature_tag_event_handler(bc_tag_temperature_t *self, bc_tag_temperature_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    teplota = NAN;

    if (event == BC_TAG_TEMPERATURE_EVENT_UPDATE)
    {
        if (bc_tag_temperature_get_temperature_celsius(self, &value))
        teplota = value;
        {
            if ((fabsf(value - param->value) >= TEMPERATURE_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
            {
                bc_radio_pub_temperature(param->channel, &value);
                param->value = value;
                param->next_pub = bc_scheduler_get_spin_tick() + TEMPERATURE_TAG_PUB_NO_CHANGE_INTEVAL;
            }
        }
    }
}

// humidity measurement
void humidity_tag_event_handler(bc_tag_humidity_t *self, bc_tag_humidity_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    vlhkost = NAN;

    if (event != BC_TAG_HUMIDITY_EVENT_UPDATE)
    {
        return;
    }

    if (bc_tag_humidity_get_humidity_percentage(self, &value))
    {
        vlhkost = value;
        if ((fabsf(value - param->value) >= HUMIDITY_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
        {
            bc_radio_pub_humidity(param->channel, &value);
            param->value = value;
            param->next_pub = bc_scheduler_get_spin_tick() + HUMIDITY_TAG_PUB_NO_CHANGE_INTEVAL;
        }
    }
}

// CO2 measurement
void co2_event_handler(bc_module_co2_event_t event, void *event_param)
{
    event_param_t *param = (event_param_t *) event_param;
    float value;

    if (event == BC_MODULE_CO2_EVENT_UPDATE)
    {
        if (bc_module_co2_get_concentration_ppm(&value))
        {
            if ((fabsf(value - param->value) >= CO2_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
            {
                bc_radio_pub_co2(&value);
                param->value = value;
                param->next_pub = bc_scheduler_get_spin_tick() + CO2_PUB_NO_CHANGE_INTERVAL;
            }
        }
    }
}

// VOC measurement
void voc_tag_event_handler(bc_tag_voc_lp_t *self, bc_tag_voc_lp_event_t event, void *event_param)
{
    uint16_t value;
    event_param_t *param = (event_param_t *)event_param;

    if (event == BC_TAG_VOC_LP_EVENT_UPDATE)
    {
        bc_tag_voc_lp_set_compensation(self, isnan(teplota) ? NULL : &teplota, isnan(vlhkost) ? NULL: &vlhkost);  // asi by melo byt bez ampersantu u teploty a vlhkosti
        if (bc_tag_voc_lp_get_tvoc_ppb(self, &value))
        {
            if ((fabsf(value - param->value) >= VOC_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
            {
              int radio_tvoc = value;
              bc_radio_pub_int("voc-lp-sensor/0:0/tvoc", &radio_tvoc);
              param->value = value;
              param->next_pub = bc_scheduler_get_spin_tick() + VOC_TAG_PUB_NO_CHANGE_INTEVAL;
            }
        }
    }
}

// another functions
// STD renew parameters
void switch_to_normal_mode_task(void *param)
{
    bc_tag_temperature_set_update_interval(&temperature, TEMPERATURE_UPDATE_NORMAL_INTERVAL);

    bc_tag_humidity_set_update_interval(&humidity, HUMIDITY_TAG_UPDATE_NORMAL_INTERVAL);

    bc_tag_voc_lp_set_update_interval(&tag_voc_lp, VOC_TAG_UPDATE_NORMAL_INTERVAL);

    bc_module_co2_set_update_interval(CO2_UPDATE_NORMAL_INTERVAL);

    bc_scheduler_unregister(bc_scheduler_get_current_task_id());
}



// Main part
void application_init(void)
{
    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_OFF);

      // Initialize thermometer sensor on core module
    //bc_tmp112_init(&tmp112, BC_I2C_I2C0, 0x49);  - NOT USED

    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_hold_time(&button, 10000);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize battery
    bc_module_battery_init();
    bc_module_battery_set_event_handler(battery_event_handler, NULL);
    bc_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize temperature
    temperature_event_param.channel = BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT;
    bc_tag_temperature_init(&temperature, BC_I2C_I2C0, BC_TAG_TEMPERATURE_I2C_ADDRESS_DEFAULT);
    bc_tag_temperature_set_update_interval(&temperature, TEMPERATURE_UPDATE_SERVICE_INTERVAL);
    bc_tag_temperature_set_event_handler(&temperature, temperature_tag_event_handler, &temperature_event_param);

    // Initialize humidity
    humidity_event_param.channel = BC_RADIO_PUB_CHANNEL_R3_I2C0_ADDRESS_DEFAULT;
    bc_tag_humidity_init(&humidity, BC_TAG_HUMIDITY_REVISION_R3, BC_I2C_I2C0, BC_TAG_HUMIDITY_I2C_ADDRESS_DEFAULT);
    bc_tag_humidity_set_update_interval(&humidity, HUMIDITY_TAG_UPDATE_SERVICE_INTERVAL);
    bc_tag_humidity_set_event_handler(&humidity, humidity_tag_event_handler, &humidity_event_param);

    // Initialize CO2
    bc_module_co2_init();
    bc_module_co2_set_update_interval(CO2_UPDATE_SERVICE_INTERVAL);
    bc_module_co2_set_event_handler(co2_event_handler, &co2_event_param);

    // Initialize VOC
    //voc_lp_event_param.channel = NAN;
    bc_tag_voc_lp_init(&tag_voc_lp, BC_I2C_I2C0);
    bc_tag_voc_lp_set_update_interval(&tag_voc_lp, VOC_TAG_UPDATE_SERVICE_INTERVAL);
    bc_tag_voc_lp_set_event_handler(&tag_voc_lp, voc_tag_event_handler, &voc_lp_event_param);
    /* To be filled */

    // Initialize radio
    bc_radio_init(BC_RADIO_MODE_NODE_SLEEPING);
    bc_radio_pairing_request("co2-voc-lp-monitor", VERSION);
    // renew of normal intervals
    bc_scheduler_register(switch_to_normal_mode_task, NULL, SERVICE_INTERVAL_INTERVAL);
    // LED blinking when init finished
    bc_led_pulse(&led, 2000);
}
