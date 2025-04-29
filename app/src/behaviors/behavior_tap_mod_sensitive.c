/*
 * Copyright (c) 2025
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_tap_mod_sensitive

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/hid.h>
#include <zmk/behavior_queue.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define MOD_LSHIFT 0x02
#define MOD_RSHIFT 0x20
#define MOD_MASK_SHIFT (MOD_LSHIFT | MOD_RSHIFT)
#define TAP_TERM_MS 200

struct behavior_tap_mod_sensitive_config {
    struct zmk_behavior_binding tap_binding;
    struct zmk_behavior_binding shifted_tap_binding;
    struct zmk_behavior_binding hold_binding;
};

struct behavior_tap_mod_sensitive_data {
    bool active;
    int64_t press_time;
    struct zmk_behavior_binding_event event;
    bool is_hold;
};

static int on_binding_pressed(struct zmk_behavior_binding *binding,
                              struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_tap_mod_sensitive_data *data = dev->data;

    data->press_time = k_uptime_get();
    data->event = event;
    data->active = true;
    data->is_hold = false;

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_binding_released(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_tap_mod_sensitive_config *config = dev->config;
    struct behavior_tap_mod_sensitive_data *data = dev->data;

    if (!data->active) {
        LOG_ERR("tap-mod-sensitive released without press");
        return -ENOTSUP;
    }

    data->active = false;
    int64_t elapsed = k_uptime_get() - data->press_time;

    const struct zmk_behavior_binding *out_binding = NULL;

    if (elapsed < TAP_TERM_MS) {
        uint8_t mods = zmk_hid_get_explicit_mods();
        out_binding = (mods & MOD_MASK_SHIFT) ? &config->shifted_tap_binding : &config->tap_binding;
    } else {
        out_binding = &config->hold_binding;
    }

    zmk_behavior_queue_add_event(out_binding, true, event.timestamp);
    zmk_behavior_queue_add_event(out_binding, false, event.timestamp);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api tap_mod_sensitive_driver_api = {
    .binding_pressed = on_binding_pressed,
    .binding_released = on_binding_released,
};

#define TRANSFORM_ENTRY(idx, node) \ 
    { \ 
        .behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(node, bindings, idx)), \
        .param1 = DT_INST_PHA_OR(node, bindings, idx, param1, 0), \
        .param2 = DT_INST_PHA_OR(node, bindings, idx, param2, 0), \
    }

#define TAP_MOD_SENSITIVE_INST(n) \ 
    static struct behavior_tap_mod_sensitive_config config_##n = { \ 
        .tap_binding = TRANSFORM_ENTRY(0, n), \ 
        .shifted_tap_binding = TRANSFORM_ENTRY(1, n), \ 
        .hold_binding = TRANSFORM_ENTRY(2, n), \ 
    }; \ 
    static struct behavior_tap_mod_sensitive_data data_##n = {}; \ 
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, &data_##n, &config_##n, \
                            POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, \
                            &tap_mod_sensitive_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TAP_MOD_SENSITIVE_INST)

#endif
