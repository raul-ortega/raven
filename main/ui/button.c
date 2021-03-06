#include <driver/rtc_io.h>
#include <driver/touch_pad.h>

#include <hal/log.h>

#include "ui/button.h"

#include "util/time.h"

static const char *TAG = "button";

#define LONG_PRESS_INTERVAL MILLIS_TO_TICKS(300)
#define REALLY_LONG_PRESS_INTERVAL MILLIS_TO_TICKS(3000)

static int touch_pad_num_from_pin(int pin)
{
    // See touch_pad_t
    switch (pin)
    {
    case TOUCH_PAD_NUM0_GPIO_NUM:
        return TOUCH_PAD_NUM0;
    case TOUCH_PAD_NUM1_GPIO_NUM:
        return TOUCH_PAD_NUM1;
    case TOUCH_PAD_NUM2_GPIO_NUM:
        return TOUCH_PAD_NUM2;
    case TOUCH_PAD_NUM3_GPIO_NUM:
        return TOUCH_PAD_NUM3;
    case TOUCH_PAD_NUM4_GPIO_NUM:
        return TOUCH_PAD_NUM4;
    case TOUCH_PAD_NUM5_GPIO_NUM:
        return TOUCH_PAD_NUM5;
    case TOUCH_PAD_NUM6_GPIO_NUM:
        return TOUCH_PAD_NUM6;
    case TOUCH_PAD_NUM7_GPIO_NUM:
        return TOUCH_PAD_NUM7;
    case TOUCH_PAD_NUM8_GPIO_NUM:
        return TOUCH_PAD_NUM8;
    case TOUCH_PAD_NUM9_GPIO_NUM:
        return TOUCH_PAD_NUM9;
    }
    return -1;
}

static bool button_is_down(button_t *button)
{
    if (button->is_touch)
    {
        uint16_t touch_value;
        uint16_t touch_filter_value;
        int touch_num = touch_pad_num_from_pin(button->pin);
        ESP_ERROR_CHECK(touch_pad_read(touch_num, &touch_value));
        ESP_ERROR_CHECK(touch_pad_read_filtered(touch_num, &touch_filter_value));
        return touch_value < 2100;
    }
    return gpio_get_level(button->pin) == 0;
}

void button_init(button_t *button)
{

    if (button->is_touch)
    {
        int touch_num = touch_pad_num_from_pin(button->pin);
        assert(touch_num >= 0);
        // Enable touch only on this pin
        ESP_ERROR_CHECK(touch_pad_clear_group_mask(TOUCH_PAD_BIT_MASK_MAX, TOUCH_PAD_BIT_MASK_MAX, TOUCH_PAD_BIT_MASK_MAX));
        ESP_ERROR_CHECK(touch_pad_init());
        ESP_ERROR_CHECK(touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V));
        ESP_ERROR_CHECK(touch_pad_filter_start(10));
        ESP_ERROR_CHECK(touch_pad_config(touch_num, 0));
    }
    else
    {
        // See https://github.com/espressif/esp-idf/blob/master/docs/api-reference/system/sleep_modes.rst
        // Pins used for wakeup need to be manually unmapped from RTC
        ESP_ERROR_CHECK(rtc_gpio_deinit(button->pin));

        ESP_ERROR_CHECK(gpio_set_direction(button->pin, GPIO_MODE_INPUT));
        ESP_ERROR_CHECK(gpio_set_pull_mode(button->pin, GPIO_PULLUP_ONLY));
    }

    button->state.is_down = button_is_down(button);
    button->state.ignore = button->state.is_down;
    button->state.long_press_sent = false;
    button->state.down_since = 0;
}

void button_update(button_t *button)
{
    bool is_down = button_is_down(button);
    time_ticks_t now = time_ticks_now();
    if (button->state.ignore)
    {
        if (!is_down)
        {
            button->state.ignore = false;
        }
        return;
    }
    if (is_down)
    {
        if (!button->state.is_down)
        {
            button->state.down_since = now;
            button->state.long_press_sent = false;
            button->state.really_long_press_sent = false;
        }
        else if (button->state.down_since + LONG_PRESS_INTERVAL < now)
        {
            if (!button->state.long_press_sent)
            {
                LOG_I(TAG, "Long press in %d", button->pin);
                if (button->long_press_callback)
                {
                    button->long_press_callback(button->user_data);
                }
                button->state.long_press_sent = true;
            }
            if (button->state.down_since + REALLY_LONG_PRESS_INTERVAL < now)
            {
                if (!button->state.really_long_press_sent)
                {
                    LOG_I(TAG, "Really long press in %d", button->pin);
                    if (button->really_long_press_callback)
                    {
                        button->really_long_press_callback(button->user_data);
                    }
                    button->state.really_long_press_sent = true;
                }
            }
        }
    }
    else
    {
        if (button->state.is_down && !button->state.long_press_sent && !button->state.really_long_press_sent)
        {
            LOG_I(TAG, "Short press in %d", button->pin);
            if (button->press_callback)
            {
                button->press_callback(button->user_data);
            }
        }
    }
    button->state.is_down = is_down;
}
