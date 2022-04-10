#include <stdio.h>
#include "osal/os_timer.h"
#include "osal/os_thread.h"
#include "cutils/log_helper.h"

#define LOG_TAG "timertest"

static void oneshot_timer_cb()
{
    static int i = 0;
    OS_LOGD(LOG_TAG, "-->oneshot_timer: count=[%d]", i++);
}

static void reload_timer_cb()
{
    static int i = 0;
    OS_LOGD(LOG_TAG, "-->reload_timer: count=[%d]", i++);
    //os_thread_sleep_msec(501); // sleep 501ms to exceed reloed-period
}

int main()
{
    struct os_timer_attr oneshot_attr = {
        .name = "oneshot_timer",
        .period_ms = 500,
        .reload = false,
    };

    struct os_timer_attr reload_attr = {
        .name = "reload_timer",
        .period_ms = 500,
        .reload = true,
    };

    os_timer oneshot_timer, reload_timer;

    OS_LOGD(LOG_TAG, "create oneshot_timer");
    oneshot_timer = os_timer_create(&oneshot_attr, oneshot_timer_cb);

    OS_LOGD(LOG_TAG, "create reload_timer");
    reload_timer = os_timer_create(&reload_attr, reload_timer_cb);

    OS_LOGD(LOG_TAG, "start oneshot_timer");
    os_timer_start(oneshot_timer);

    OS_LOGD(LOG_TAG, "start reload_timer");
    os_timer_start(reload_timer);

    OS_LOGD(LOG_TAG, "sleep 5000 ms");
    os_thread_sleep_msec(5000);

    OS_LOGD(LOG_TAG, "oneshot_timer state: [%s]", os_timer_is_active(oneshot_timer) ? "active" : "inactive");
    OS_LOGD(LOG_TAG, "reload_timer state: [%s]", os_timer_is_active(reload_timer) ? "active" : "inactive");

    OS_LOGD(LOG_TAG, "stop oneshot_timer");
    os_timer_stop(oneshot_timer);

    OS_LOGD(LOG_TAG, "stop reload_timer");
    os_timer_stop(reload_timer);

    OS_LOGD(LOG_TAG, "destroy oneshot_timer");
    os_timer_destroy(oneshot_timer);

    OS_LOGD(LOG_TAG, "destroy reload_timer");
    os_timer_destroy(reload_timer);

    return 0;
}
