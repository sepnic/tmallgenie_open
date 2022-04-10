#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "osal/os_misc.h"
#include "osal/os_thread.h"
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "cutils/mlooper.h"
#include "httpclient/httpclient.h"

#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#define LOG_TAG "sysutils_main"

struct priv_data {
    const char *str;
};
static void msg_handle(struct message *arg)
{
    struct message *msg = arg;
    OS_LOGD(LOG_TAG, "--> Handle message: what=[%d], str=[%s]", msg->what, msg->data);
    //sleep(1); // delay to make message:1000 timeout
}
static void msg_free(struct message *msg)
{
    OS_LOGD(LOG_TAG, "--> Free message: what=[%d], str=[%s]", msg->what, msg->data);
    OS_FREE(msg->data);
}
static void msg_handle2(struct message *arg)
{
    struct message *msg = arg;
    struct priv_data *priv = msg->data;
    OS_LOGD(LOG_TAG, "--> Handle message: what=[%d], str=[%s]", msg->what, priv->str);
}
static void msg_free2(struct message *msg)
{
    struct priv_data *priv = msg->data;
    OS_LOGD(LOG_TAG, "--> Free message: what=[%d], str=[%s]", msg->what, priv->str);
    OS_FREE(priv->str);
}
static void msg_timeout(struct message *msg)
{
    OS_LOGE(LOG_TAG, "--> Timeout message: what=[%d]", msg->what);
}
int mlooper_test()
{
    struct os_thread_attr attr;
    mlooper_handle looper;
    struct message *msg;
    const char *str;

    attr.name = "msglooper_test";
    attr.priority = OS_THREAD_PRIO_NORMAL;
    attr.stacksize = 4096;
    looper = mlooper_create(&attr, msg_handle, msg_free);

    mlooper_dump(looper);

    mlooper_start(looper);

    {
        str = OS_STRDUP("mlooper_post_message_delay");
        // timeout 2s
        msg = message_obtain(1000, 0, 0, (void *)str);
        message_set_timeout_cb(msg, msg_timeout, 2001);
        mlooper_post_message_delay(looper, msg, 2000); // delay 2s
    }

    {
        for (int i = 0; i < 3; i++) {
            str = OS_STRDUP("mlooper_post_message");
            msg = message_obtain(i+100, 0, 0, (void *)str);
            mlooper_post_message(looper, msg);
        }
    }

    {
        msg = message_obtain_buffer_obtain(0, 0, 0, sizeof(struct priv_data));
        struct priv_data *priv = (struct priv_data *)msg->data;
        priv->str = OS_STRDUP("mlooper_post_message_front");
        message_set_handle_cb(msg, msg_handle2);
        message_set_free_cb(msg, msg_free2);
        mlooper_post_message_front(looper, msg);
    }

    mlooper_dump(looper);

    //OS_LOGI(LOG_TAG, "remove what=1000");
    //mlooper_remove_message(looper, 1000);
    //mlooper_dump(looper);

    os_thread_sleep_msec(5000);
    mlooper_destroy(looper);
    return 0;
}

int random_test()
{
    unsigned long buffer[4];
    for (int i = 0; i < 3; i++) {
        os_random(buffer, sizeof(buffer));
        OS_LOGI(LOG_TAG, "os_random[%d]: %08x %08x %08x %08x", i, buffer[0], buffer[1], buffer[2], buffer[3]);
    }
    return 0;
}

void *httpclient_test(void *arg)
{
    const char          *url = "https://httpbin.org/get";
    char                *header_buf = OS_CALLOC(1, 1024);
    char                *response_buf = OS_CALLOC(1, 2048);
    httpclient_t         client;
    httpclient_data_t    client_data;
    memset(&client, 0, sizeof(httpclient_t));
    memset(&client_data, 0, sizeof(httpclient_data_t));

    int ret = httpclient_connect(&client, (char *)url);
    if (ret != HTTPCLIENT_OK) {
        OS_LOGE(LOG_TAG, "httpclient_connect failed, ret=%d", ret);
        goto httpcient_out;
    }

    client_data.header_buf       = header_buf;
    client_data.header_buf_len   = 1024;
    client_data.response_buf     = response_buf;
    client_data.response_buf_len = 2048;
    ret = httpclient_send_request(&client, (char *)url, HTTPCLIENT_GET, &client_data);
    if (ret != HTTPCLIENT_OK) {
        OS_LOGE(LOG_TAG, "httpclient_connect failed, ret=%d", ret);
        goto httpcient_out;
    }
    do {
        ret = httpclient_recv_response(&client, &client_data);
        if (ret < 0) {
            OS_LOGE(LOG_TAG, "httpclient_recv_response failed, ret=%d", ret);
            goto httpcient_out;
        }
        printf("%s", response_buf);
    } while (ret > 0);
    printf("\n");

httpcient_out:
    httpclient_close(&client);
    OS_FREE(header_buf);
    OS_FREE(response_buf);
    return NULL;
}

void app_main()
{
    printf("Hello sysutils!\n");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    OS_LOGI(LOG_TAG, "Connected to AP");

    random_test();

    mlooper_test();

    struct os_thread_attr attr = {
        .name = "httpclient_test",
        .priority = OS_THREAD_PRIO_NORMAL,
        .stacksize = 8192,
        .joinable = false,
    };
    os_thread_create(&attr, httpclient_test, NULL);

    while (1)
        os_thread_sleep_msec(100);
}
