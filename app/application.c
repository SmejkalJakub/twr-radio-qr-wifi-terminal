#include <application.h>

#include "qrcodegen.h"

#define BATTERY_UPDATE_INTERVAL (60 * 60 * 1000)


// LED instance
bc_led_t led;

// GFX instance
bc_gfx_t *gfx;

// LCD buttons instance
bc_button_t button_left;
bc_button_t button_right;

// QR code variables
char qr_code[150];
char ssid[32];
char password[64];

static const bc_radio_sub_t subs[] = {
    {"qr/-/chng/code", BC_RADIO_SUB_PT_STRING, bc_change_qr_value, (void *) PASSWD}
};

uint32_t display_page_index = 0;

bc_tmp112_t temp;

void battery_event_handler(bc_module_battery_event_t event, void *event_param)
{
    (void) event;
    (void) event_param;

    float voltage;
    int percentage;

    if (bc_module_battery_get_voltage(&voltage))
    {
        bc_radio_pub_battery(&voltage);
    }


    if (bc_module_battery_get_charge_level(&percentage))
    {
        bc_radio_pub_string("%d", percentage);
    }
}

void tmp112_event_handler(bc_tmp112_t *self, bc_tmp112_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == BC_TMP112_EVENT_UPDATE)
    {
        float temperature = 0.0;
        bc_tmp112_get_temperature_celsius(&temp, &temperature);
        bc_radio_pub_temperature(BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE, &temperature);
    }
}

void bc_change_qr_value(uint64_t *id, const char *topic, void *value, void *param)
{
    int command = (int *) param;

    strncpy(qr_code, value, sizeof(qr_code));

    bc_eeprom_write(0, qr_code, sizeof(qr_code));
    get_qr_data();

    qrcode_project(qr_code);

    bc_scheduler_plan_now(500);

}

static void print_qr(const uint8_t qrcode[]) 
{
    get_qr_data();
    bc_gfx_clear(gfx);

    bc_gfx_set_font(gfx, &bc_font_ubuntu_13);
    bc_gfx_draw_string(gfx, 2, 0, "Scan and connect to: ", true);
    bc_gfx_draw_string(gfx, 2, 10, ssid, true);

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

            bc_gfx_draw_fill_rectangle(gfx, x1, y1, x2, y2, qrcodegen_getModule(qrcode, x, y));
		}
		//fputs("\n", stdout);
	}
	//fputs("\n", stdout);
    bc_gfx_update(gfx);
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

void lcd_event_handler(bc_module_lcd_event_t event, void *event_param)
{ 
    if(event == BC_MODULE_LCD_EVENT_LEFT_CLICK) 
    {
        bc_radio_pub_push_button(0);
    }
    else if(event == BC_MODULE_LCD_EVENT_RIGHT_CLICK) 
    {
        if(display_page_index == 0)
        {
            display_page_index++;
        }
        else if(display_page_index == 1)
        {
            display_page_index--;
        }
        bc_scheduler_plan_now(0);
    }

}

void qrcode_project(char *text)
{
    bc_system_pll_enable();

	// Make and print the QR Code symbol
	uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
	uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];
	bool ok = qrcodegen_encodeText(text, tempBuffer, qrcode, qrcodegen_Ecc_LOW,	qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true);

	if (ok)
    {
		print_qr(qrcode);
    }

    bc_system_pll_disable();
}

void lcd_page_with_data()
{
    bc_gfx_clear(gfx);
    bc_gfx_printf(gfx, 0, 0, true, "Connect to our wi-fi:");
    bc_gfx_printf(gfx, 0, 15, true, "SSID:");
    bc_gfx_printf(gfx, 0, 25, true, "%s", ssid);
    bc_gfx_printf(gfx, 0, 45, true, "Password:");
    bc_gfx_printf(gfx, 0, 55, true, "%s", password);

    bc_gfx_update(gfx);
}

void application_init(void)
{
    bc_radio_init(BC_RADIO_MODE_NODE_SLEEPING);
    bc_radio_set_subs((bc_radio_sub_t *) subs, sizeof(subs)/sizeof(bc_radio_sub_t));
    bc_radio_set_rx_timeout_for_sleeping_node(250);

    bc_log_init(BC_LOG_LEVEL_DUMP, BC_LOG_TIMESTAMP_ABS);

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);

    bc_module_lcd_init();
    gfx = bc_module_lcd_get_gfx();
    bc_module_lcd_set_event_handler(lcd_event_handler, NULL);

    
    // initialize TMP112 sensor
    bc_tmp112_init(&temp, BC_I2C_I2C0, BC_TAG_TEMPERATURE_I2C_ADDRESS_ALTERNATE);

    // set measurement handler (call "tmp112_event_handler()" after measurement)
    bc_tmp112_set_event_handler(&temp, tmp112_event_handler, NULL);

    // automatically measure the temperature every 15 minutes
    bc_tmp112_set_update_interval(&temp, 15 * 60 * 1000);
    


    // initialize LCD and load from eeprom
    bc_eeprom_read(0, qr_code, sizeof(qr_code));

    if(strstr(qr_code, "WIFI:S:") == NULL)
    {
        strncpy(qr_code, "WIFI:S:test;T:test;P:test;;", sizeof(qr_code));
    }



    // Initialze battery module
    bc_module_battery_init();
    bc_module_battery_set_event_handler(battery_event_handler, NULL);
    bc_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);
    bc_radio_pairing_request("qr-terminal", VERSION);

}

void application_task(void)
{
    if(display_page_index == 0)
    {
        bc_log_debug("calling page 1");
        qrcode_project(qr_code);
    }
    else if(display_page_index == 1)
    {
        bc_log_debug("calling page 2");
        lcd_page_with_data();
    }
}