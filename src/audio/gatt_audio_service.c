/**
 * @file gatt_audio_service.c
 * @brief Custom BLE GATT Audio Service Implementation for MP3 Rewind
 * 
 * This service implements audio streaming over Bluetooth LE using custom
 * GATT characteristics. It provides real audio transmission capability
 * for the MP3 Rewind project on ST B-L475E-IOT01A Discovery Board.
 * 
 * Architecture:
 * - Audio Data Characteristic: Handles PCM audio stream via notifications
 * - Audio Control Characteristic: Receives commands (play/pause/volume)
 * - Audio Info Characteristic: Provides format information to client
 * 
 * The service replaces the previous simulation-only implementation with
 * actual BLE GATT audio transmission.
 * 
 * This implementation, especially of the GATT characteristics, is designed
 * and debugged with the help of Claude Sonnet 4. You will notice I try to 
 * be as transparent as possible with how the code was developed. In this case
 * I was trying to bridge the gap created when I realized the SPBTLE-RF module
 * does not support A2DP profile, so we are using a custom GATT for audio streaming.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <string.h>

#include "gatt_audio_service.h"

LOG_MODULE_REGISTER(gatt_audio_service, LOG_LEVEL_DBG);

/* Service state */
static struct {
    bool initialized;
    gatt_audio_format_t current_format;
    gatt_audio_control_cb_t control_callback;
    bool audio_data_subscribed;
    uint16_t audio_data_ccc_value;
} gatt_audio_state = {0};

/* Forward declarations */
static ssize_t gatt_audio_data_read(struct bt_conn *conn,
                                   const struct bt_gatt_attr *attr,
                                   void *buf, uint16_t len, uint16_t offset);

static void gatt_audio_data_ccc_cfg_changed(const struct bt_gatt_attr *attr,
                                           uint16_t value);

static ssize_t gatt_audio_control_write(struct bt_conn *conn,
                                       const struct bt_gatt_attr *attr,
                                       const void *buf, uint16_t len,
                                       uint16_t offset, uint8_t flags);

static ssize_t gatt_audio_info_read(struct bt_conn *conn,
                                   const struct bt_gatt_attr *attr,
                                   void *buf, uint16_t len, uint16_t offset);

/* GATT Service Definition */
BT_GATT_SERVICE_DEFINE(gatt_audio_svc,
    /* Service Declaration */
    BT_GATT_PRIMARY_SERVICE(BT_UUID_AUDIO_SERVICE),
    
    /* Audio Data Characteristic */
    BT_GATT_CHARACTERISTIC(BT_UUID_AUDIO_DATA,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          gatt_audio_data_read, NULL, NULL),
    BT_GATT_CCC(gatt_audio_data_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    
    /* Audio Control Characteristic */  
    BT_GATT_CHARACTERISTIC(BT_UUID_AUDIO_CONTROL,
                          BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                          BT_GATT_PERM_WRITE,
                          NULL, gatt_audio_control_write, NULL),
    
    /* Audio Info Characteristic */
    BT_GATT_CHARACTERISTIC(BT_UUID_AUDIO_INFO,
                          BT_GATT_CHRC_READ,
                          BT_GATT_PERM_READ,
                          gatt_audio_info_read, NULL, NULL),
);

/**
 * @brief Initialize GATT Audio Service
 */
int gatt_audio_service_init(void)
{
    if (gatt_audio_state.initialized) {
        LOG_WRN("GATT Audio Service already initialized");
        return 0;
    }
    
    /* Set default audio format */
    gatt_audio_state.current_format.sample_rate = 44100;
    gatt_audio_state.current_format.channels = 2;
    gatt_audio_state.current_format.bits_per_sample = 16;
    gatt_audio_state.current_format.frame_size = 
        (gatt_audio_state.current_format.channels * 
         gatt_audio_state.current_format.bits_per_sample) / 8;
    
    gatt_audio_state.initialized = true;
    gatt_audio_state.audio_data_subscribed = true;  // Auto-enable for testing
    
    LOG_INF("🎵 GATT Audio Service initialized successfully");
    LOG_INF("📊 Default format: %u Hz, %u channels, %u-bit", 
            gatt_audio_state.current_format.sample_rate,
            gatt_audio_state.current_format.channels,
            gatt_audio_state.current_format.bits_per_sample);
    LOG_INF("🔔 Auto-enabled notifications for testing purposes");
    
    return 0;
}

/**
 * @brief Send audio data through GATT characteristic
 */
int gatt_audio_send_data(const uint8_t *data, size_t len, struct bt_conn *conn)
{
    int ret;
    const struct bt_gatt_attr *attr;
    static uint32_t last_send_time = 0;
    
    if (!gatt_audio_state.initialized) {
        LOG_ERR("GATT Audio Service not initialized");
        return -EINVAL;
    }
    
    if (!conn) {
        LOG_ERR("No Bluetooth connection");
        return -ENOTCONN;
    }
    
    /* Check if connection is in connected state */
    struct bt_conn_info conn_info;
    ret = bt_conn_get_info(conn, &conn_info);
    if (ret || conn_info.state != BT_CONN_STATE_CONNECTED) {
        LOG_DBG("Connection not ready (state: %d)", conn_info.state);
        return -ENOTCONN;
    }
    
    if (!gatt_audio_state.audio_data_subscribed) {
        LOG_DBG("Client not subscribed to audio data notifications");
        return -ENOTCONN;
    }
    
    if (!data || len == 0) {
        return -EINVAL;
    }
    
    /* Rate limit notifications to prevent BLE stack overflow - more conservative */
    uint32_t current_time = k_uptime_get_32();
    if (current_time - last_send_time < 50) { /* Minimum 50ms between notifications */
        return -EAGAIN; /* Try again later */
    }
    
    /* Always use the absolute minimum chunk size */
    size_t max_chunk = 20; /* Fixed to BLE minimum for maximum reliability */
    if (len > max_chunk) {
        len = max_chunk;
        LOG_DBG("Truncating audio data to %zu bytes (BLE minimum chunk size)", len);
    }
    
    /* Find the audio data characteristic attribute */
    attr = bt_gatt_find_by_uuid(gatt_audio_svc.attrs, gatt_audio_svc.attr_count, BT_UUID_AUDIO_DATA);
    if (!attr) {
        LOG_ERR("Audio data characteristic not found");
        return -ENOENT;
    }
    
    /* Send audio data as GATT notification */
    ret = bt_gatt_notify(conn, attr, data, len);
    if (ret) {
        LOG_ERR("Failed to send audio data notification: %d", ret);
        return ret;
    }
    
    last_send_time = current_time;
    LOG_DBG("🎵 Sent %zu bytes of audio data via GATT", len);
    return len;
}

/**
 * @brief Set audio format information
 */
int gatt_audio_set_format(const gatt_audio_format_t *format, struct bt_conn *conn)
{
    if (!gatt_audio_state.initialized || !format) {
        return -EINVAL;
    }
    
    /* Update format */
    memcpy(&gatt_audio_state.current_format, format, sizeof(gatt_audio_format_t));
    
    LOG_INF("📊 Audio format updated: %u Hz, %u ch, %u-bit",
            format->sample_rate, format->channels, format->bits_per_sample);
    
    /* Don't notify immediately - wait for client to read or subscribe */
    /* This prevents the GATT assertion failure on connection */
    
    return 0;
}

/**
 * @brief Check if audio client is subscribed to notifications
 */
bool gatt_audio_is_subscribed(struct bt_conn *conn)
{
    return gatt_audio_state.audio_data_subscribed;
}

/**
 * @brief Get maximum supported chunk size for current connection
 */
size_t gatt_audio_get_max_chunk_size(struct bt_conn *conn)
{
    if (!conn) {
        return 20; /* Very conservative default for no connection */
    }
    
    /* Get MTU but always use very conservative chunk sizes for reliability */
    uint16_t mtu = bt_gatt_get_mtu(conn);
    
    /* Use the absolute minimum to avoid any ATT channel issues */
    /* BLE minimum is 23 bytes MTU, minus 3 bytes ATT header = 20 bytes max payload */
    size_t max_chunk = 20; /* Always use minimum for maximum compatibility */
    
    LOG_DBG("Using conservative chunk size %zu for MTU %u (always 20 for stability)", max_chunk, mtu);
    return max_chunk;
}

/**
 * @brief Register audio control callback
 */
int gatt_audio_register_control_callback(gatt_audio_control_cb_t callback)
{
    if (!gatt_audio_state.initialized) {
        return -EINVAL;
    }
    
    gatt_audio_state.control_callback = callback;
    LOG_INF("🎛️ Audio control callback registered");
    return 0;
}

/* GATT Characteristic Implementations */

/**
 * @brief Audio Data Characteristic Read Handler
 */
static ssize_t gatt_audio_data_read(struct bt_conn *conn,
                                   const struct bt_gatt_attr *attr,
                                   void *buf, uint16_t len, uint16_t offset)
{
    /* This characteristic is primarily for notifications, 
     * but we can return a status message for reads */
    const char *status = gatt_audio_state.audio_data_subscribed ? 
                        "STREAMING" : "READY";
    
    return bt_gatt_attr_read(conn, attr, buf, len, offset, status, strlen(status));
}

/**
 * @brief Audio Data CCC (Client Characteristic Configuration) Changed Handler
 */
static void gatt_audio_data_ccc_cfg_changed(const struct bt_gatt_attr *attr,
                                           uint16_t value)
{
    gatt_audio_state.audio_data_ccc_value = value;
    gatt_audio_state.audio_data_subscribed = (value == BT_GATT_CCC_NOTIFY);
    
    if (gatt_audio_state.audio_data_subscribed) {
        LOG_INF("🔔 Client subscribed to audio data notifications - READY FOR STREAMING!");
        LOG_INF("🎵 Audio streaming can now begin - client is listening");
    } else {
        LOG_INF("🔕 Client unsubscribed from audio data notifications");
        LOG_INF("⏸️  Audio streaming paused - no client listening");
    }
}

/**
 * @brief Audio Control Characteristic Write Handler
 */
static ssize_t gatt_audio_control_write(struct bt_conn *conn,
                                       const struct bt_gatt_attr *attr,
                                       const void *buf, uint16_t len,
                                       uint16_t offset, uint8_t flags)
{
    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    
    if (len != sizeof(gatt_audio_control_t)) {
        LOG_ERR("Invalid control data length: %u (expected %zu)", 
                len, sizeof(gatt_audio_control_t));
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    
    const gatt_audio_control_t *control = (const gatt_audio_control_t *)buf;
    
    LOG_INF("🎛️ Received audio control command: %u", control->command);
    
    /* Process control command */
    switch (control->command) {
        case AUDIO_CMD_PLAY:
            LOG_INF("▶️  Play command received");
            break;
        case AUDIO_CMD_PAUSE:
            LOG_INF("⏸️  Pause command received");
            break;
        case AUDIO_CMD_STOP:
            LOG_INF("⏹️  Stop command received");
            break;
        case AUDIO_CMD_VOLUME:
            LOG_INF("🔊 Volume command: %u%%", control->volume);
            break;
        case AUDIO_CMD_MUTE:
            LOG_INF("🔇 Mute command received");
            break;
        case AUDIO_CMD_UNMUTE:
            LOG_INF("🔊 Unmute command received");
            break;
        default:
            LOG_WRN("❓ Unknown audio control command: %u", control->command);
            return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
    }
    
    /* Call registered callback if available */
    if (gatt_audio_state.control_callback) {
        gatt_audio_state.control_callback(control);
    }
    
    return len;
}

/**
 * @brief Audio Info Characteristic Read Handler
 */
static ssize_t gatt_audio_info_read(struct bt_conn *conn,
                                   const struct bt_gatt_attr *attr,
                                   void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           &gatt_audio_state.current_format,
                           sizeof(gatt_audio_format_t));
}
