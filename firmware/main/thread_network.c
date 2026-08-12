/**
 * @file thread_network.c
 * @brief OpenThread networking for AirCube
 *
 * Thread Sleepy End Device that sends sensor data over the mesh network.
 * Uses OpenThread's built-in UDP socket API for data reporting.
 *
 * API mirrors zigbee.c so callers can use either module via the same
 * interface gated by Kconfig (CONFIG_AIRCUBE_NETWORK_THREAD).
 */

#include "thread_network.h"
#include "button.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_openthread.h"
#include "esp_openthread_types.h"
#include "esp_ieee802154.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_vfs_eventfd.h"
#include "nvs_flash.h"
#include "nvs.h"

#include <openthread/instance.h>
#include <openthread/ip6.h>
#include <openthread/link.h>
#include <openthread/thread.h>
#include <openthread/udp.h>
#include <openthread/joiner.h>
#include <openthread/dataset.h>

#include <string.h>
#include <stdio.h>

static const char *TAG = "thread_net";

/* ── Configuration ──────────────────────────────────────────────────── */

#define OPENTHREAD_UDP_PORT     61631   /* AirCube sensor data port      */
#define COMMISSIONING_TIMEOUT_MS 60000

/* SED poll period: check parent every 3 s for incoming data.
 * Longer periods save power but increase latency. */
#define SED_POLL_PERIOD_MS      3000

/* ── State ───────────────────────────────────────────────────────────── */

static otInstance                *s_instance   = NULL;
static volatile bool              s_connected  = false;
static volatile bool              s_commissioning = false;
static TickType_t                 s_comm_start = 0;
static otUdpSocket                s_udp_socket;

/* Commissioning flag (survives reboots when eg factory reset needed) */
#define NVS_NAMESPACE   "aircube"
#define NVS_KEY_COMM    "thr_comm"

static void set_commissioning_flag(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, NVS_KEY_COMM, 1);
        nvs_commit(h);
        nvs_close(h);
    }
}

static bool consume_commissioning_flag(void)
{
    nvs_handle_t h;
    uint8_t val = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    bool found = (nvs_get_u8(h, NVS_KEY_COMM, &val) == ESP_OK && val);
    nvs_erase_key(h, NVS_KEY_COMM);
    nvs_commit(h);
    nvs_close(h);
    return found;
}

/* ── Forward declarations ────────────────────────────────────────────── */

static void joiner_callback(otError error, void *ctx);
static void start_commissioning(void);

static float current_brightness_percent(void)
{
    return (float)button_get_brightness_percent();
}

static void start_commissioning(void)
{
    if (s_commissioning) return;

    esp_ieee802154_enable();
    otIp6SetEnabled(s_instance, true);
    otLinkSetPollPeriod(s_instance, SED_POLL_PERIOD_MS);

    otError err = otJoinerStart(s_instance, CONFIG_AIRCUBE_THREAD_PSKD,
                                NULL, NULL, NULL, NULL, NULL,
                                joiner_callback, NULL);
    if (err != OT_ERROR_NONE) {
        ESP_LOGE(TAG, "otJoinerStart failed: %s", otThreadErrorToString(err));
        return;
    }
    s_commissioning = true;
    s_comm_start = xTaskGetTickCount();
    ESP_LOGI(TAG, "Commissioning started (PSKd: %s)", CONFIG_AIRCUBE_THREAD_PSKD);
}

static void open_udp_socket(void)
{
    otSockAddr sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.mPort = OPENTHREAD_UDP_PORT;

    otError err = otUdpOpen(s_instance, &s_udp_socket, NULL, NULL);
    if (err != OT_ERROR_NONE) {
        ESP_LOGE(TAG, "otUdpOpen failed: %s", otThreadErrorToString(err));
        return;
    }
    err = otUdpBind(s_instance, &s_udp_socket, &sockaddr, OT_NETIF_THREAD_HOST);
    if (err != OT_ERROR_NONE) {
        ESP_LOGW(TAG, "otUdpBind failed: %s (sending still possible)", otThreadErrorToString(err));
    }
    ESP_LOGI(TAG, "UDP socket opened on port %u", OPENTHREAD_UDP_PORT);
}

/* ── Joiner callback ─────────────────────────────────────────────────── */

static void joiner_callback(otError error, void *ctx)
{
    (void)ctx;
    if (error == OT_ERROR_NONE) {
        ESP_LOGI(TAG, "Commissioning succeeded – joined Thread network");
        s_connected = true;
        s_commissioning = false;
        consume_commissioning_flag();

        /* Enable Thread interface now that we have credentials */
        otThreadSetEnabled(s_instance, true);

        /* Set SED poll period */
        otLinkSetPollPeriod(s_instance, SED_POLL_PERIOD_MS);

        open_udp_socket();

        /* Log network info */
        ESP_LOGI(TAG, "Thread network joined (channel %u, PAN 0x%04x, rloc16 0x%04x)",
                 otLinkGetChannel(s_instance),
                 otLinkGetPanId(s_instance),
                 otThreadGetRloc16(s_instance));
    } else {
        ESP_LOGE(TAG, "Commissioning failed: %s", otThreadErrorToString(error));
        s_commissioning = false;
    }
}

/* ── State change callback ──────────────────────────────────────────── */

static void state_change_callback(otChangedFlags flags, void *ctx)
{
    (void)ctx;

    otDeviceRole role = otThreadGetDeviceRole(s_instance);
    ESP_LOGI(TAG, "OT state change (flags 0x%08lx, role %d)", (unsigned long)flags, (int)role);

    if (role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_ROUTER ||
        role == OT_DEVICE_ROLE_LEADER) {
        if (!s_connected) {
            s_connected = true;
            ESP_LOGI(TAG, "Thread attached as %s",
                     role == OT_DEVICE_ROLE_CHILD  ? "child" :
                     role == OT_DEVICE_ROLE_ROUTER ? "router" : "leader");
            open_udp_socket();
        }
    } else if (role == OT_DEVICE_ROLE_DETACHED ||
               role == OT_DEVICE_ROLE_DISABLED) {
        if (s_connected) {
            s_connected = false;
            ESP_LOGW(TAG, "Thread detached");
            otUdpClose(s_instance, &s_udp_socket);
        }
    }
}

/* ── OT main loop task ──────────────────────────────────────────────── */

static void ot_task(void *pvParameters)
{
    (void)pvParameters;

    /* Run the OpenThread main loop (does not return) */
    esp_openthread_launch_mainloop();

    /* Should never reach here */
    ESP_LOGE(TAG, "OpenThread mainloop exited");
    vTaskDelete(NULL);
}

/* ── Platform configuration ─────────────────────────────────────────── */

static esp_openthread_platform_config_t platform_config(void)
{
    esp_openthread_platform_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.radio_config.radio_mode = RADIO_MODE_NATIVE;
    cfg.host_config.host_connection_mode = HOST_CONNECTION_MODE_NONE;
    cfg.port_config.storage_partition_name = "nvs";
    cfg.port_config.netif_queue_size = 4;
    cfg.port_config.task_queue_size = 4;
    return cfg;
}

/* ── Public API ──────────────────────────────────────────────────────── */

void thread_network_init(void)
{
    esp_openthread_platform_config_t cfg = platform_config();

    /* Required by OpenThread platform: netif + event loop + eventfd */
    esp_netif_init();
    esp_event_loop_create_default();
    esp_vfs_eventfd_config_t eventfd_cfg = ESP_VFS_EVENTD_CONFIG_DEFAULT();
    esp_vfs_eventfd_register(&eventfd_cfg);

    ESP_ERROR_CHECK(esp_openthread_init(&cfg));

    s_instance = esp_openthread_get_instance();
    assert(s_instance);

    /* Register callbacks */
    otSetStateChangedCallback(s_instance, state_change_callback, NULL);

    /* Keep radio idle until commissioned; enabling IPv6 early causes
     * periodic radio activity that glitches the WS2812 RMT signal. */

    /* Check if we have an active dataset (previously commissioned) */
    if (otDatasetIsCommissioned(s_instance)) {
        ESP_LOGI(TAG, "Active Thread dataset found – enabling Thread");
        otIp6SetEnabled(s_instance, true);
        otLinkSetPollPeriod(s_instance, SED_POLL_PERIOD_MS);
        otThreadSetEnabled(s_instance, true);
    } else {
        ESP_LOGI(TAG, "No active Thread dataset – waiting for commissioning");
        /* Put radio to sleep to prevent RMT glitches on WS2812 LEDs */
        esp_ieee802154_sleep();
        /* If a commissioning flag was set before reboot, start it now */
        if (consume_commissioning_flag()) {
            ESP_LOGI(TAG, "Commissioning flag consumed – joining");
            start_commissioning();
        }
    }

    /* Launch OT main loop in a separate task */
    xTaskCreate(ot_task, "ot_main", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Thread initialized");
}

void thread_network_update_sensors(float temp_c, float humidity, int eco2,
                                   int etvoc, int aqi, float co2_ppm, float lux,
                                   float ens210_temp_c, float ens210_humidity)
{
    if (!s_connected) {
        return;
    }

    /* Build JSON payload and send via UDP to mesh-local all-nodes multicast */
    char payload[256];
    int len = snprintf(payload, sizeof(payload),
        "{\"t\":%.2f,\"h\":%.2f,\"eco2\":%d,\"etvoc\":%d,\"aqi\":%d,"
        "\"co2\":%.0f,\"lux\":%.1f,\"bri\":%d}",
        temp_c, humidity, eco2, etvoc, aqi, co2_ppm, lux,
        (int)current_brightness_percent());

    if (len <= 0 || (size_t)len >= sizeof(payload)) {
        ESP_LOGW(TAG, "UDP payload truncated");
        return;
    }

    otMessageInfo msg_info;
    memset(&msg_info, 0, sizeof(msg_info));
    msg_info.mPeerPort = OPENTHREAD_UDP_PORT;

    /* ff03::1 = realm-local all-nodes multicast (scope 3) */
    const otIp6Address dest = {
        .mFields = { .m8 = { 0xff, 0x03, 0,0,0,0,0,0, 0,0,0,0,0,0,0,0x01 } }
    };
    msg_info.mPeerAddr = dest;

    otMessage *msg = otUdpNewMessage(s_instance, NULL);
    if (!msg) {
        ESP_LOGW(TAG, "Failed to allocate UDP message");
        return;
    }

    otMessageAppend(msg, payload, (uint16_t)len);

    otError err = otUdpSend(s_instance, &s_udp_socket, msg, &msg_info);
    if (err != OT_ERROR_NONE) {
        ESP_LOGW(TAG, "otUdpSend failed: %s", otThreadErrorToString(err));
        otMessageFree(msg);
    }
    /* msg is freed by otUdpSend on success */
}

bool thread_network_is_connected(void)
{
    return s_connected;
}

void thread_network_start_commissioning(void)
{
    ESP_LOGI(TAG, "Commissioning requested");

    if (s_connected && s_instance) {
        ESP_LOGI(TAG, "Already connected – resetting to factory-new state");
        /* Remove active dataset so the next boot starts fresh */
        otInstanceErasePersistentInfo(s_instance);
        set_commissioning_flag();
        esp_restart();
        return;
    }

    s_commissioning = true;
    s_comm_start = xTaskGetTickCount();
    set_commissioning_flag();

    if (s_instance) {
        /* If not initialized yet (joiner before OT task), the flag
         * will be consumed in thread_network_init() at next boot.
         * Otherwise start joiner now. */
        if (otThreadGetDeviceRole(s_instance) != OT_DEVICE_ROLE_DISABLED) {
            ESP_LOGI(TAG, "Resetting and rebooting for clean commissioning");
            otInstanceErasePersistentInfo(s_instance);
            esp_restart();
            return;
        }
        start_commissioning();
    } else {
        ESP_LOGW(TAG, "OT instance not ready – flag set, will commission at init");
    }
}

bool thread_network_is_commissioning(void)
{
    if (!s_commissioning) {
        return false;
    }
    if ((xTaskGetTickCount() - s_comm_start) > pdMS_TO_TICKS(COMMISSIONING_TIMEOUT_MS)) {
        s_commissioning = false;
        ESP_LOGW(TAG, "Commissioning timed out");
        return false;
    }
    return true;
}

void thread_network_report_brightness(void)
{
    /* Brightness is included in the periodic sensor UDP message;
     * this function exists for API symmetry with zigbee.h. */
}
