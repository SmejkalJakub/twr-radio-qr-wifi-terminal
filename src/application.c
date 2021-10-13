#include <application.h>

#include "qrcodegen.h"

#define BATTERY_UPDATE_INTERVAL (60 * 60 * 1000)


// LED instance
twr_led_t led;

// GFX instance
twr_gfx_t *gfx;

// LCD buttons instance
twr_button_t button_left;
twr_button_t button_right;

// QR code variables
char qr_code[150];
char ssid[32];
char password[64];

static const twr_radio_sub_t subs[] = {
    {"qr/-/chng/code", TWR_RADIO_SUB_PT_STRING, twr_change_qr_value, (void *) PASSWD}
};

uint32_t display_page_index = 0;

twr_tmp112_t temp;

void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    (void) event;
    (void) event_param;

    float voltage;
    int percentage;

    if (twr_module_battery_get_voltage(&voltage))
    {
        twr_radio_pub_battery(&voltage);
    }


    if (twr_module_battery_get_charge_level(&percentage))
    {
        twr_radio_pub_string("%d", percentage);
    }
}

void tmp112_event_handler(twr_tmp112_t *self, twr_tmp112_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == TWR_TMP112_EVENT_UPDATE)
    {
        float temperature = 0.0;
        twr_tmp112_get_temperature_celsius(&temp, &temperature);
        twr_radio_pub_temperature(TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE, &temperature);
    }
}

void twr_change_qr_value(uint64_t *id, const char *topic, void *value, void *param)
{
    int command = (int *) param;

    strncpy(qr_code, value, sizeof(qr_code));

    twr_eeprom_write(0, qr_code, sizeof(qr_code));
    get_qr_data();

    qrcode_project(qr_code);

    twr_scheduler_plan_now(500);

}

static void print_qr(const uint8_t qrcode[])
{
    get_qr_data();
    twr_gfx_clear(gfx);

    twr_gfx_set_font(gfx, &twr_font_ubuntu_13);
    twr_gfx_draw_string(gfx, 2, 0, "Scan and connect to: ", true);
    twr_gfx_draw_string(gfx, 2, 10, ssid, true);

    uint32_t offset_x = 8;
    uint32_t offset_y = 32;
    uint32_t box_size = 3;
	int size = qrcodegen_getSize(qrcode);
	int border = 2;
	for (int y = -border; y < size + border; y++) {
		for (int x = -border; x < size + border; x++) {
			//fputs((qrcodegen_getModule(qrcode, x, y) ? "##" : "  "), stdout);
            uint32_t x1 = offset_x + x * box_size;
            uint32_t y1 = offset_y + y * box_size;
            uint32_t x2 = x1 + box_size;
            uint32_t y2 = y1 + box_size;

            twr_gfx_draw_fill_rectangle(gfx, x1, y1, x2, y2, qrcodegen_getModule(qrcode, x, y));
		}
		//fputs("\n", stdout);
	}
	//fputs("\n", stdout);
    twr_gfx_update(gfx);
}

void get_qr_data()
{
    get_passwd();
    get_SSID();
}

char get_SSID()
{
    for(int i = 7; qr_code[i] != ';'; i++)
    {
        ssid[i - 7] = qr_code[i];
    }
    return ssid;

}

char get_passwd()
{
    int i = 0;
    int semicolons = 0;
    bool password_found = false;
    do
    {
        i++;
        if(qr_code[i] == ';')
        {
            semicolons++;
            if(semicolons == 2)
            {
                password_found = true;
            }
        }
    }
    while(!password_found);

    i += 3;


    int start_i = i;

    for(; qr_code[i] != ';'; i++)
    {
        password[i - start_i] = qr_code[i];
    }

    return password;
}

void lcd_event_handler(twr_module_lcd_event_t event, void *event_param)
{
    if(event == TWR_MODULE_LCD_EVENT_LEFT_CLICK)
    {
        twr_radio_pub_push_button(0);
    }
    else if(event == TWR_MODULE_LCD_EVENT_RIGHT_CLICK)
    {
        if(display_page_index == 0)
        {
            display_page_index++;
        }
        else if(display_page_index == 1)
        {
            display_page_index--;
        }
        twr_scheduler_plan_now(0);
    }

}

void qrcode_project(char *text)
{
    twr_system_pll_enable();

	// Make and print the QR Code symbol
	uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
	uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];
	bool ok = qrcodegen_encodeText(text, tempBuffer, qrcode, qrcodegen_Ecc_LOW,	qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true);

	if (ok)
    {
		print_qr(qrcode);
    }

    twr_system_pll_disable();
}

void lcd_page_with_data()
{
    twr_gfx_clear(gfx);
    twr_gfx_printf(gfx, 0, 0, true, "Connect to our wi-fi:");
    twr_gfx_printf(gfx, 0, 15, true, "SSID:");
    twr_gfx_printf(gfx, 0, 25, true, "%s", ssid);
    twr_gfx_printf(gfx, 0, 45, true, "Password:");
    twr_gfx_printf(gfx, 0, 55, true, "%s", password);

    twr_gfx_update(gfx);
}

void application_init(void)
{
    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);
    twr_radio_set_subs((twr_radio_sub_t *) subs, sizeof(subs)/sizeof(twr_radio_sub_t));
    twr_radio_set_rx_timeout_for_sleeping_node(250);

    twr_log_init(TWR_LOG_LEVEL_DUMP, TWR_LOG_TIMESTAMP_ABS);

    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);

    twr_module_lcd_init();
    gfx = twr_module_lcd_get_gfx();
    twr_module_lcd_set_event_handler(lcd_event_handler, NULL);


    // initialize TMP112 sensor
    twr_tmp112_init(&temp, TWR_I2C_I2C0, TWR_TAG_TEMPERATURE_I2C_ADDRESS_ALTERNATE);

    // set measurement handler (call "tmp112_event_handler()" after measurement)
    twr_tmp112_set_event_handler(&temp, tmp112_event_handler, NULL);

    // automatically measure the temperature every 15 minutes
    twr_tmp112_set_update_interval(&temp, 15 * 60 * 1000);



    // initialize LCD and load from eeprom
    twr_eeprom_read(0, qr_code, sizeof(qr_code));

    if(strstr(qr_code, "WIFI:S:") == NULL)
    {
        strncpy(qr_code, "WIFI:S:test;T:test;P:test;;", sizeof(qr_code));
    }



    // Initialze battery module
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);
    twr_radio_pairing_request("qr-terminal", VERSION);

}

void application_task(void)
{
    if(display_page_index == 0)
    {
        twr_log_debug("calling page 1");
        qrcode_project(qr_code);
    }
    else if(display_page_index == 1)
    {
        twr_log_debug("calling page 2");
        lcd_page_with_data();
    }
}
