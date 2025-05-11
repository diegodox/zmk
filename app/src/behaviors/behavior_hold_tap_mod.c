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
    uint32_t tap_param;
    uint32_t hold_param;
    uint32_t mod_param;
    uint32_t mods;
    int tapping_term_ms;
};

struct behavior_hold_tap_mod_data {
    struct k_work_delayable work;
    bool hold_sent;
    bool mod_sent;
    struct zmk_behavior_binding_event evt;
    const struct behavior_hold_tap_mod_config *cfg;
};

static int invoke_press(const struct device *dev, uint32_t param,
                        struct zmk_behavior_binding_event ev) {
    struct zmk_behavior_binding b = {
        .behavior_dev = dev->name,
        .param1 = param,
    };
    return zmk_behavior_invoke_binding(&b, ev, true);
}

static int invoke_release(const struct device *dev, uint32_t param,
                          struct zmk_behavior_binding_event ev) {
    struct zmk_behavior_binding b = {
        .behavior_dev = dev->name,
        .param1 = param,
    };
    return zmk_behavior_invoke_binding(&b, ev, false);
}

static void timer_handler(struct k_work *work) {
    struct k_work_delayable *dw = k_work_delayable_from_work(work);
    struct behavior_hold_tap_mod_data *d =
        CONTAINER_OF(dw, struct behavior_hold_tap_mod_data, work);

    /* if we already sent a mod, or mods are active, do nothing */
    if (d->mod_sent || (zmk_hid_get_explicit_mods() & d->cfg->mods)) {
        return;
    }

    d->hold_sent = true;
    invoke_press(d->cfg->hold_dev, d->cfg->hold_param, d->evt);
}

static int on_pressed(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event ev) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_hold_tap_mod_data *d = dev->data;
    d->cfg = dev->config;
    d->evt = ev;
    d->hold_sent = false;
    d->mod_sent = false;

    bool mod_active = (zmk_hid_get_explicit_mods() & d->cfg->mods) != 0;

    if (mod_active) {
        d->mod_sent = true;
        return invoke_press(d->cfg->mod_dev, d->cfg->mod_param, ev);
    }

    k_work_schedule(&d->work, K_MSEC(d->cfg->tapping_term_ms));
    return invoke_press(d->cfg->tap_dev, d->cfg->tap_param, ev);
}

static int on_released(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event ev) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_hold_tap_mod_data *d = dev->data;
    const struct behavior_hold_tap_mod_config *cfg = d->cfg;

    k_work_cancel_delayable(&d->work);

    if (d->mod_sent) {
        return invoke_release(cfg->mod_dev, cfg->mod_param, ev);
    } else if (d->hold_sent) {
        return invoke_release(cfg->hold_dev, cfg->hold_param, ev);
    } else {
        return invoke_release(cfg->tap_dev, cfg->tap_param, ev);
    }
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
        .tap_param = DT_INST_PHA_BY_IDX(n, bindings, 0, param1),                                   \
        .hold_param = DT_INST_PHA_BY_IDX(n, bindings, 1, param1),                                  \
        .mod_param = DT_INST_PHA_BY_IDX(n, bindings, 2, param1),                                   \
        .mods = DT_INST_PROP(n, mods),                                                             \
        .tapping_term_ms = DT_INST_PROP(n, tapping_term_ms),                                       \
    };                                                                                             \
    static struct behavior_hold_tap_mod_data behavior_hold_tap_mod_data_##n;                       \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_hold_tap_mod_init, NULL, &behavior_hold_tap_mod_data_##n,  \
                            &behavior_hold_tap_mod_config_##n, POST_KERNEL,                        \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_hold_tap_mod_api);

DT_INST_FOREACH_STATUS_OKAY(HTM_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
