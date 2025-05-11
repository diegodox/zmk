#define DT_DRV_COMPAT zmk_behavior_hold_tap_mod

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zmk/keys.h>
#include <dt-bindings/zmk/keys.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>
#include <zmk/matrix.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_hold_tap_mod_config {
    char *tap_behavior_dev;
    char *hold_behavior_dev;
    char *mod_behavior_dev;
    uint32_t mods;
    int tapping_term_ms;
};

struct behavior_hold_tap_mod_data {};

struct active_hold_tap_mod {
    int32_t position;
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    uint8_t source;
#endif
    uint32_t param_hold;
    uint32_t param_tap;
    int64_t timestamp;
    const struct behavior_hold_tap_mod_config *config;
    struct k_work_delayable work;
    bool mod_active;
};

static struct active_hold_tap_mod active_mod_tap;

static int press_binding(const char *behavior_dev, uint32_t param,
                         struct zmk_behavior_binding_event event) {
    struct zmk_behavior_binding binding = {
        .behavior_dev = behavior_dev,
        .param1 = param,
    };
    return zmk_behavior_invoke_binding(&binding, event, true);
}

static int release_binding(const char *behavior_dev, uint32_t param,
                           struct zmk_behavior_binding_event event) {
    struct zmk_behavior_binding binding = {
        .behavior_dev = behavior_dev,
        .param1 = param,
    };
    return zmk_behavior_invoke_binding(&binding, event, false);
}

static void hold_tap_mod_timer_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct active_hold_tap_mod *tap = CONTAINER_OF(dwork, struct active_hold_tap_mod, work);

    if (tap->mod_active) {
        return;
    }

    press_binding(tap->config->hold_behavior_dev, tap->param_hold,
                  (struct zmk_behavior_binding_event){
                      .position = tap->position,
                      .timestamp = tap->timestamp,
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
                      .source = tap->source,
#endif
                  });
}

static int on_hold_tap_mod_binding_pressed(struct zmk_behavior_binding *binding,
                                           struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_hold_tap_mod_config *cfg = dev->config;

    uint32_t mods = zmk_hid_get_explicit_mods();
    bool mod_active = (mods & cfg->mods) != 0;

    active_mod_tap = (struct active_hold_tap_mod){
        .position = event.position,
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
        .source = event.source,
#endif
        .timestamp = event.timestamp,
        .config = cfg,
        .param_hold = binding->param1,
        .param_tap = binding->param2,
        .mod_active = mod_active,
    };

    if (mod_active) {
        return press_binding(cfg->mod_behavior_dev, binding->param1, event);
    }

    k_work_schedule(&active_mod_tap.work, K_MSEC(cfg->tapping_term_ms));
    return press_binding(cfg->tap_behavior_dev, binding->param2, event);
}

static int on_hold_tap_mod_binding_released(struct zmk_behavior_binding *binding,
                                            struct zmk_behavior_binding_event event) {
    const struct behavior_hold_tap_mod_config *cfg = active_mod_tap.config;
    k_work_cancel_delayable(&active_mod_tap.work);

    if (active_mod_tap.mod_active) {
        return release_binding(cfg->mod_behavior_dev, binding->param1, event);
    }

    return release_binding(cfg->tap_behavior_dev, binding->param2, event);
}

static const struct behavior_driver_api behavior_hold_tap_mod_driver_api = {
    .binding_pressed = on_hold_tap_mod_binding_pressed,
    .binding_released = on_hold_tap_mod_binding_released,
};

static int behavior_hold_tap_mod_init(const struct device *dev) {
    k_work_init_delayable(&active_mod_tap.work, hold_tap_mod_timer_handler);
    return 0;
}

#define KP_INST(n)                                                                                 \
    static const struct behavior_hold_tap_mod_config behavior_hold_tap_mod_config_##n = {          \
        .tap_behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(n, bindings, 0)),                \
        .hold_behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(n, bindings, 1)),               \
        .mod_behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(n, bindings, 2)),                \
        .mods = DT_INST_PROP(n, mods),                                                             \
        .tapping_term_ms = DT_INST_PROP(n, tapping_term_ms),                                       \
    };                                                                                             \
    static struct behavior_hold_tap_mod_data behavior_hold_tap_mod_data_##n = {};                  \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_hold_tap_mod_init, NULL, &behavior_hold_tap_mod_data_##n,  \
                            &behavior_hold_tap_mod_config_##n, POST_KERNEL,                        \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_hold_tap_mod_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KP_INST)

#endif
