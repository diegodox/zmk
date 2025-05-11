#define DT_DRV_COMPAT zmk_behavior_hold_tap_mod

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>
#include <zmk/hid.h>
#include <dt-bindings/zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_hold_tap_mod_config {
    const struct device *tap_dev;
    const struct device *hold_dev;
    const struct device *mod_dev;
    uint32_t mods;
    int tapping_term_ms;
};

struct behavior_hold_tap_mod_data {
    struct k_work_delayable work;
    struct {
        int32_t position;
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
        uint8_t source;
#endif
        int64_t timestamp;
        uint32_t param_hold;
        uint32_t param_tap;
        uint32_t param_mod;
        const struct behavior_hold_tap_mod_config *cfg;
        bool mod_active;
        bool hold_sent;
    } active;
};

static int invoke_press(const struct device *dev, uint32_t param,
                        struct zmk_behavior_binding_event ev) {
    struct zmk_behavior_binding b = {
        .behavior_dev = dev,
        .param1 = param,
    };
    return zmk_behavior_invoke_binding(&b, ev, true);
}

static int invoke_release(const struct device *dev, uint32_t param,
                          struct zmk_behavior_binding_event ev) {
    struct zmk_behavior_binding b = {
        .behavior_dev = dev,
        .param1 = param,
    };
    return zmk_behavior_invoke_binding(&b, ev, false);
}

static void timer_handler(struct k_work *work) {
    struct behavior_hold_tap_mod_data *d =
        CONTAINER_OF(work, struct behavior_hold_tap_mod_data, work);
    auto *a = &d->active;

    if (a->mod_active) {
        return;
    }

    a->hold_sent = true;
    invoke_press(a->cfg->hold_dev, a->param_hold,
                 (struct zmk_behavior_binding_event){
                     .position = a->position,
                     .timestamp = a->timestamp,
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
                     .source = a->source,
#endif
                 });
}

static int on_pressed(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event ev) {
    const struct device *inst = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_hold_tap_mod_data *d = inst->data;
    const struct behavior_hold_tap_mod_config *cfg = inst->config;

    uint32_t explicit_mods = zmk_hid_get_explicit_mods();
    bool mod_active = (explicit_mods & cfg->mods) != 0;

    d->active = (typeof(d->active)){
        .position = ev.position,
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
        .source = ev.source,
#endif
        .timestamp = ev.timestamp,
        .param_hold = binding->param1,
        .param_tap = binding->param2,
        .param_mod = binding->param3,
        .cfg = cfg,
        .mod_active = mod_active,
        .hold_sent = false,
    };

    if (mod_active) {
        return invoke_press(cfg->mod_dev, d->active.param_mod, ev);
    }

    k_work_schedule(&d->work, K_MSEC(cfg->tapping_term_ms));
    return invoke_press(cfg->tap_dev, d->active.param_tap, ev);
}

static int on_released(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event ev) {
    const struct device *inst = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_hold_tap_mod_data *d = inst->data;
    auto *a = &d->active;
    const struct behavior_hold_tap_mod_config *cfg = a->cfg;

    k_work_cancel_delayable(&d->work);

    if (a->mod_active) {
        return invoke_release(cfg->mod_dev, a->param_mod, ev);
    }

    if (a->hold_sent) {
        return invoke_release(cfg->hold_dev, a->param_hold, ev);
    }

    return invoke_release(cfg->tap_dev, a->param_tap, ev);
}

static const struct behavior_driver_api behavior_hold_tap_mod_api = {
    .binding_pressed = on_pressed,
    .binding_released = on_released,
};

static int behavior_hold_tap_mod_init(const struct device *dev) {
    struct behavior_hold_tap_mod_data *d = dev->data;
    k_work_init_delayable(&d->work, timer_handler);
    return 0;
}

#define HTM_INST(n)                                                                                \
    static const struct behavior_hold_tap_mod_config behavior_hold_tap_mod_config_##n = {          \
        .tap_dev = DEVICE_DT_GET(DT_INST_PHANDLE_BY_IDX(n, bindings, 0)),                          \
        .hold_dev = DEVICE_DT_GET(DT_INST_PHANDLE_BY_IDX(n, bindings, 1)),                         \
        .mod_dev = DEVICE_DT_GET(DT_INST_PHANDLE_BY_IDX(n, bindings, 2)),                          \
        .mods = DT_INST_PROP(n, mods),                                                             \
        .tapping_term_ms = DT_INST_PROP(n, tapping_term_ms),                                       \
    };                                                                                             \
    static struct behavior_hold_tap_mod_data behavior_hold_tap_mod_data_##n;                       \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_hold_tap_mod_init, NULL, &behavior_hold_tap_mod_data_##n,  \
                            &behavior_hold_tap_mod_config_##n, POST_KERNEL,                        \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_hold_tap_mod_api);

DT_INST_FOREACH_STATUS_OKAY(HTM_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
