#include <zephyr/kernel.h>
#include <zmk/behavior.h>
#include <zmk/keymap.h>
#include <zmk/hid.h>
#include <zmk/behavior_queue.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/behavior.h>

#define TAP_TERM_MS 200

// Mask to detect shift pressed
#define MOD_LSHIFT 0x02
#define MOD_RSHIFT 0x20
#define MOD_MASK_SHIFT (MOD_LSHIFT | MOD_RSHIFT)

struct behavior_config {
    struct zmk_behavior_binding tap_normal;
    struct zmk_behavior_binding tap_shifted;
    struct zmk_behavior_binding hold;
};

struct active_state {
    int64_t press_time;
    bool active;
    struct zmk_behavior_binding_event event;
};

static struct active_state state = {0};

static int behavior_press(struct zmk_behavior_binding *binding,
                          struct behavior_config *config,
                          struct zmk_behavior_binding_event event) {
    state.press_time = k_uptime_get();
    state.active = true;
    state.event = event;
    return ZMK_BEHAVIOR_OPAQUE;
}

static int behavior_release(struct zmk_behavior_binding *binding,
                            struct behavior_config *config,
                            struct zmk_behavior_binding_event event) {
    if (!state.active) return 0;
    state.active = false;

    int64_t now = k_uptime_get();
    int64_t elapsed = now - state.press_time;

    if (elapsed < TAP_TERM_MS) {
        uint8_t mods = zmk_hid_get_explicit_mods();
        const struct zmk_behavior_binding *tap_binding =
            (mods & MOD_MASK_SHIFT) ? &config->tap_shifted : &config->tap_normal;

        zmk_behavior_queue_add_event(tap_binding, true, event.timestamp);
        zmk_behavior_queue_add_event(tap_binding, false, event.timestamp);
    } else {
        zmk_behavior_queue_add_event(&config->hold, true, event.timestamp);
        zmk_behavior_queue_add_event(&config->hold, false, event.timestamp);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_driver_api = {
    .binding_pressed = behavior_press,
    .binding_released = behavior_release,
};

BEHAVIOR_DEFINE(tap_mod_sensitive, behavior_driver_api);
