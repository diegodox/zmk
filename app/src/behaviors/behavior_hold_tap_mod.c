#define DT_DRV_COMPAT zmk_behavior_hold_tap_mod

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>

#include <zmk/behavior.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_hold_tap_mod_config {
    const struct device *bindings[3]; // tap, hold, mod
    uint32_t mods;
    int tapping_term_ms;
};

struct behavior_hold_tap_mod_data {
    struct k_work_delayable work;
    struct zmk_behavior_binding_event event;
    struct zmk_behavior_binding binding;
    const struct device *active_behavior;
    bool released;
};

bool mod_active;
static void tapping_term_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct behavior_hold_tap_mod_data *data =
        CONTAINER_OF(dwork, struct behavior_hold_tap_mod_data, work);

    if (data->released || data->mod_active) {
        return;
    }

    const struct behavior_hold_tap_mod_config *cfg = data->active_behavior->config;

    struct zmk_behavior_binding binding = {
        .behavior_dev = device_get_name(cfg->bindings[1]),
        .param1 = data->binding.param1,
        .param2 = data->binding.param2,
    };

    behavior_press(cfg->bindings[1], &binding, data->event);
    data->active_behavior = cfg->bindings[1];
}

static int behavior_hold_tap_mod_press(const struct device *dev,
                                       struct zmk_behavior_binding *binding,
                                       struct zmk_behavior_binding_event event) {
    const struct behavior_hold_tap_mod_config *cfg = dev->config;
    struct behavior_hold_tap_mod_data *data = dev->data;

    uint32_t mods = zmk_hid_get_explicit_mods();
    bool mod_active = (mods & cfg->mods) != 0;

    data->event = event;
    data->binding = *binding;
    data->mod_active = mod_active;
    data->released = false;

    const struct device *behavior = mod_active ? cfg->bindings[2] : cfg->bindings[0];
    data->active_behavior = behavior;

    struct zmk_behavior_binding target_binding = {
        .behavior_dev = device_get_name(behavior),
        .param1 = binding->param1,
        .param2 = binding->param2,
    };

    if (!mod_active) {
        k_work_schedule(&data->work, K_MSEC(cfg->tapping_term_ms));
    }

    return behavior_press(behavior, &target_binding, event);
}

static int behavior_hold_tap_mod_release(const struct device *dev,
                                         struct zmk_behavior_binding *binding,
                                         struct zmk_behavior_binding_event event) {
    struct behavior_hold_tap_mod_data *data = dev->data;
    const struct behavior_hold_tap_mod_config *cfg = dev->config;

    data->released = true;
    k_work_cancel_delayable(&data->work);

    const struct device *behavior = data->active_behavior;

    struct zmk_behavior_binding target_binding = {
        .behavior_dev = device_get_name(behavior),
        .param1 = binding->param1,
        .param2 = binding->param2,
    };

    return behavior_release(behavior, &target_binding, event);
}

static int wrapper_pressed(struct zmk_behavior_binding *binding,
                           struct zmk_behavior_binding_event event) {
    const struct device *dev = device_get_binding(binding->behavior_dev);
    return behavior_hold_tap_mod_press(dev, binding, event);
}

static int wrapper_released(struct zmk_behavior_binding *binding,
                            struct zmk_behavior_binding_event event) {
    const struct device *dev = device_get_binding(binding->behavior_dev);
    return behavior_hold_tap_mod_release(dev, binding, event);
}

static const struct behavior_driver_api behavior_hold_tap_mod_driver_api = {
    .binding_pressed = wrapper_pressed,
    .binding_released = wrapper_released,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, &(struct behavior_hold_tap_mod_data){},
                        &(struct behavior_hold_tap_mod_config){
                            .bindings =
                                {
                                    DEVICE_DT_GET(DT_INST_PROP_BY_IDX(0, bindings, 0)),
                                    DEVICE_DT_GET(DT_INST_PROP_BY_IDX(0, bindings, 1)),
                                    DEVICE_DT_GET(DT_INST_PROP_BY_IDX(0, bindings, 2)),
                                },
                            .mods = DT_INST_PROP(0, mods),
                            .tapping_term_ms = DT_INST_PROP(0, tapping_term_ms),
                        },
                        POST_KERNEL, 50, &behavior_hold_tap_mod_driver_api);

#endif
