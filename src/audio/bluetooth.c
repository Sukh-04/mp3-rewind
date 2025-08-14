/**
 * @file bluetooth.c
 * @brief Bluetooth LE Audio Output Implementation for SPBTLE-RF
 * 
 * Implements Bluetooth LE streaming for audio output using the ST SPBTLE-RF module
 * on the ST B-L475E-IOT01A Discovery Board. The SPBTLE-RF is connected via SPI3.
 * 
 * Key Features:
 * - Bluetooth LE 4.1 advertising and connection management via SPBTLE-RF
 * - Audio buffer management and streaming
 * - Connection status and basic audio control
 * 
 * Hardware:
 * - ST SPBTLE-RF module on ST B-L475E-IOT01A Discovery Board
 * - Connected via SPI3 interface with IRQ and RESET control
 * - Supports Bluetooth LE 4.1 with embedded protocol stack
 * 
 * Deciphyering and determining the best practices for Bluetooth audio streaming
 * was a challenge, therefore this implementation was debugged with the help of
 * Claude Sonnet 4.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/net_buf.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <math.h>

#include "audiosys.h"
#include "bluetooth.h"
#include "gatt_audio_service.h"
#include "../utils/circular_buffers.h"
#include "../utils/error_handling.h"

LOG_MODULE_REGISTER(bluetooth_audio, LOG_LEVEL_INF);

/* Bluetooth audio configuration */
#define BT_AUDIO_BUFFER_SIZE      2048
#define BT_AUDIO_CHUNK_SIZE       512
#define BT_AUDIO_SAMPLE_RATE      44100
#define BT_AUDIO_CHANNELS         2
#define BT_AUDIO_BITS_PER_SAMPLE  16

/* Bluetooth audio state */
typedef struct {
    bool initialized;
    bool connected;
    bool streaming;
    bool scanning;
    struct bt_conn *conn;
    bt_addr_le_t target_addr;  // Address of target device (Bose headphones)
    bool target_found;
    circular_buffer_t audio_buffer;
    uint8_t buffer_data[BT_AUDIO_BUFFER_SIZE];
    struct k_thread audio_thread;
    k_tid_t audio_thread_id;
    struct k_sem stream_sem;
    uint8_t volume;
} bt_audio_state_t;

static bt_audio_state_t bt_audio = {0};

/* Thread stack for Bluetooth audio streaming */
K_THREAD_STACK_DEFINE(bt_audio_stack, 2048);

/* Function prototypes */
static void bt_ready_callback(int err);
static void bt_connected_callback(struct bt_conn *conn, uint8_t err);
static void bt_disconnected_callback(struct bt_conn *conn, uint8_t reason);
static void bt_audio_streaming_thread(void *p1, void *p2, void *p3);
static int bt_start_advertising(void);
static int bt_start_scanning(void);
static void bt_scan_callback(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad);
static void bt_audio_control_callback(const gatt_audio_control_t *control);
static void test_audio_generation_work(struct k_work *work);

/* Bluetooth connection callbacks */
BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = bt_connected_callback,
    .disconnected = bt_disconnected_callback,
};

/* Bluetooth LE advertising data - keep it simple to avoid size issues */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, "MP3-Rewind", 10),
};

/**
 * @brief Initialize Bluetooth audio system
 */
int bluetooth_audio_init(const audio_format_t *format)
{
    int ret;
    
    LOG_INF("Initializing Bluetooth LE audio system with GATT service");
    
    if (bt_audio.initialized) {
        LOG_WRN("Bluetooth audio already initialized");
        return 0;
    }
    
    /* Initialize GATT Audio Service first */
    ret = gatt_audio_service_init();
    if (ret) {
        LOG_ERR("Failed to initialize GATT Audio Service: %d", ret);
        return ret;
    }
    
    /* Initialize circular buffer for audio data */
    ret = circular_buffer_init(&bt_audio.audio_buffer, 
                              bt_audio.buffer_data, 
                              BT_AUDIO_BUFFER_SIZE);
    if (ret < 0) {
        LOG_ERR("Failed to initialize audio buffer: %d", ret);
        return ret;
    }
    
    /* Initialize semaphore for streaming control */
    k_sem_init(&bt_audio.stream_sem, 0, 1);
    
    /* Enable Bluetooth */
    ret = bt_enable(bt_ready_callback);
    if (ret) {
        LOG_ERR("Bluetooth init failed: %d", ret);
        return ret;
    }
    
    /* Wait for Bluetooth to be ready */
    LOG_INF("Waiting for Bluetooth to initialize...");
    k_sleep(K_MSEC(2000));
    
    /* Set default volume */
    bt_audio.volume = 75;
    bt_audio.initialized = true;
    
    LOG_INF("🎵 Bluetooth LE audio system initialized successfully with GATT service");
    return 0;
}

/**
 * @brief Bluetooth ready callback
 */
static void bt_ready_callback(int err)
{
    if (err) {
        LOG_ERR("Bluetooth init failed: %d", err);
        return;
    }
    
    LOG_INF("Bluetooth initialized successfully");
    
    /* Give the SPBTLE-RF module a moment to fully initialize */
    k_sleep(K_MSEC(100));
    
    /* Check if Bluetooth is actually ready */
    if (!bt_is_ready()) {
        LOG_ERR("Bluetooth reports not ready after initialization");
        return;
    }
    
    /* Start advertising immediately to be discoverable */
    int ret = bt_start_advertising();
    if (ret) {
        LOG_ERR("Failed to start advertising: %d", ret);
        return;
    }
    
    LOG_INF("✅ Bluetooth advertising active - device is discoverable");
    LOG_INF("📱 Connect from Nordic nRF Connect, LightBlue, or similar BLE apps");
    LOG_INF("🎵 Device name: 'MP3-Rewind' - look for this in your BLE scanner");
    
    /* Do NOT start scanning immediately - let advertising stabilize first */
    /* Scanning can be started later if no connections are made */
}

/* Simple advertising data without name - global scope for constant initialization */
static const struct bt_data simple_ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

/**
 * @brief Start Bluetooth LE advertising for connections
 */
static int bt_start_advertising(void)
{
    int ret;
    
    LOG_INF("🔵 Starting Bluetooth LE advertising...");
    LOG_INF("📱 Device will be discoverable as 'MP3-Rewind'");
    
    /* Use proper advertising parameters with connectable flag */
    struct bt_le_adv_param adv_param = {
        .id = BT_ID_DEFAULT,
        .sid = 0,
        .secondary_max_skip = 0,
        .options = BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_USE_NAME,
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,  /* 100ms */
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,  /* 150ms */
        .peer = NULL,
    };
    
    /* Start advertising with flags and name */
    ret = bt_le_adv_start(&adv_param, simple_ad, ARRAY_SIZE(simple_ad), NULL, 0);
    if (ret) {
        LOG_ERR("Advertising with flags failed: %d", ret);
        
        /* Try without USE_NAME option */
        adv_param.options = BT_LE_ADV_OPT_CONN;
        ret = bt_le_adv_start(&adv_param, simple_ad, ARRAY_SIZE(simple_ad), NULL, 0);
        if (ret) {
            LOG_ERR("All advertising methods failed: %d", ret);
            return ret;
        } else {
            LOG_INF("✅ Advertising started without name");
        }
    } else {
        LOG_INF("✅ Advertising started successfully with name");
    }
    
    LOG_INF("📱 Device is now discoverable - connect from your phone or headphones");
    return 0;
}

/**
 * @brief Bluetooth connection established callback
 */
static void bt_connected_callback(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed: %d", err);
        return;
    }
    
    bt_audio.conn = bt_conn_ref(conn);
    bt_audio.connected = true;
    
    LOG_INF("🎧 Bluetooth device connected successfully");
    
    /* Check if we can support multiple connections */
    /* For now, keep advertising to allow Nordic nRF Connect to discover us */
    LOG_INF("📱 Keeping advertising active for additional connections (like nRF Connect)");
    
    /* Register GATT audio control callback */
    gatt_audio_register_control_callback(bt_audio_control_callback);
    
    /* Set audio format in GATT service */
    gatt_audio_format_t format = {
        .sample_rate = BT_AUDIO_SAMPLE_RATE,
        .channels = BT_AUDIO_CHANNELS,
        .bits_per_sample = BT_AUDIO_BITS_PER_SAMPLE,
        .frame_size = (BT_AUDIO_CHANNELS * BT_AUDIO_BITS_PER_SAMPLE) / 8
    };
    gatt_audio_set_format(&format, conn);
    
    /* Note: Streaming will be started manually in Test 2 after notifications are enabled */
    LOG_INF("🎵 Connection established - waiting for manual streaming start in Test 2");
    LOG_INF("📱 Client can now enable notifications and prepare for audio data");
    
    /* Initialize streaming thread but don't start streaming yet */
    bt_audio.audio_thread_id = k_thread_create(
        &bt_audio.audio_thread,
        bt_audio_stack,
        K_THREAD_STACK_SIZEOF(bt_audio_stack),
        bt_audio_streaming_thread,
        NULL, NULL, NULL,
        K_PRIO_PREEMPT(7),
        0,
        K_NO_WAIT
    );
    
    k_thread_name_set(bt_audio.audio_thread_id, "bt_gatt_audio");
    LOG_INF("🎵 Bluetooth GATT audio streaming thread ready (not started yet)");
}

/**
 * @brief Bluetooth connection disconnected callback
 */
static void bt_disconnected_callback(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Bluetooth disconnected (reason %u)", reason);
    
    bt_audio.connected = false;
    bt_audio.streaming = false;
    
    if (bt_audio.conn) {
        bt_conn_unref(bt_audio.conn);
        bt_audio.conn = NULL;
    }
    
    /* Terminate audio thread */
    if (bt_audio.audio_thread_id) {
        k_thread_abort(bt_audio.audio_thread_id);
        bt_audio.audio_thread_id = NULL;
    }
    
    /* Clear audio buffer */
    circular_buffer_clear(&bt_audio.audio_buffer);
    
    /* Restart advertising for new connections */
    bt_start_advertising();
    
    LOG_INF("Ready for new Bluetooth connections");
}

/**
 * @brief Start Bluetooth audio streaming
 */
int bluetooth_audio_start(void)
{
    if (!bt_audio.initialized) {
        LOG_ERR("Bluetooth audio not initialized");
        return -EINVAL;
    }
    
    if (!bt_audio.connected) {
        LOG_WRN("No Bluetooth device connected");
        return -ENOTCONN;
    }
    
    if (bt_audio.streaming) {
        LOG_WRN("Already streaming");
        return 0;
    }
    
    bt_audio.streaming = true;
    k_sem_give(&bt_audio.stream_sem);
    
    LOG_INF("Bluetooth audio streaming started");
    return 0;
}

/**
 * @brief Stop Bluetooth audio streaming
 */
int bluetooth_audio_stop(void)
{
    if (!bt_audio.streaming) {
        return 0;
    }
    
    bt_audio.streaming = false;
    k_sem_reset(&bt_audio.stream_sem);
    
    /* Clear any remaining audio data */
    circular_buffer_clear(&bt_audio.audio_buffer);
    
    LOG_INF("Bluetooth audio streaming stopped");
    return 0;
}

/**
 * @brief Write audio data to Bluetooth output
 */
int bluetooth_audio_write(const uint8_t *data, size_t len)
{
    if (!bt_audio.initialized || !bt_audio.connected) {
        return -ENOTCONN;
    }
    
    /* Write data to circular buffer */
    size_t written = circular_buffer_write(&bt_audio.audio_buffer, data, len);
    
    if (written < len) {
        LOG_WRN("Audio buffer full, dropping %zu bytes", len - written);
    }
    
    LOG_DBG("Wrote %zu bytes to Bluetooth audio buffer", written);
    return written;
}

/**
 * @brief Audio streaming thread - handles actual Bluetooth LE GATT audio transmission
 */
static void bt_audio_streaming_thread(void *p1, void *p2, void *p3)
{
    uint8_t audio_chunk[64];   /* Even smaller chunk size for BLE reliability */
    size_t bytes_read;
    int ret;
    int failed_attempts = 0;
    
    LOG_INF("🎵 Bluetooth LE GATT audio streaming thread started");
    
    while (bt_audio.connected) {
        /* Wait for streaming to be enabled */
        if (!bt_audio.streaming) {
            k_sem_take(&bt_audio.stream_sem, K_FOREVER);
            continue;
        }
        
        /* Check if client is subscribed to GATT notifications */
        if (!gatt_audio_is_subscribed(bt_audio.conn)) {
            LOG_DBG("Waiting for GATT client to subscribe to audio notifications...");
            k_sleep(K_MSEC(1000));
            continue;
        }
        
        /* Read audio data from circular buffer */
        bytes_read = circular_buffer_read_timeout(&bt_audio.audio_buffer, 
                                                 audio_chunk, 
                                                 sizeof(audio_chunk),
                                                 K_MSEC(100));
        
        if (bytes_read > 0) {
            /* Stream audio via Bluetooth LE GATT service with conservative flow control */
            ret = gatt_audio_send_data(audio_chunk, bytes_read, bt_audio.conn);
            if (ret > 0) {
                LOG_DBG("🎵 Streamed %d bytes via GATT Audio Service", ret);
                failed_attempts = 0;  /* Reset failure counter */
                
                /* Very conservative timing for BLE notifications - 100ms between sends */
                k_sleep(K_MSEC(100));
                
            } else if (ret == -ENOTCONN) {
                LOG_DBG("Client not subscribed, waiting...");
                k_sleep(K_MSEC(200));
            } else if (ret == -EAGAIN) {
                /* Rate limited - just wait a bit */
                LOG_DBG("Rate limited, waiting...");
                k_sleep(K_MSEC(50));
            } else if (ret == -ENOMEM || ret == -12) {
                /* Buffer overflow - implement backoff strategy */
                failed_attempts++;
                if (failed_attempts < 3) {
                    LOG_DBG("BLE buffer full, backing off... (attempt %d)", failed_attempts);
                    k_sleep(K_MSEC(200 * failed_attempts));  /* Conservative backoff */
                } else {
                    LOG_WRN("Too many BLE buffer failures, pausing streaming...");
                    k_sleep(K_MSEC(2000));  /* Longer pause */
                    failed_attempts = 0;
                }
            } else {
                LOG_ERR("Failed to send audio data: %d", ret);
                k_sleep(K_MSEC(100));
            }
        } else {
            /* No data available, small delay to prevent busy waiting */
            k_sleep(K_MSEC(50));
        }
    }
    
    LOG_INF("Bluetooth LE GATT audio streaming thread terminated");
}

/**
 * @brief Set Bluetooth audio volume
 */
int bluetooth_audio_set_volume(uint8_t volume)
{
    if (volume > 100) {
        return -EINVAL;
    }
    
    bt_audio.volume = volume;
    
    /* TODO: Implement actual Bluetooth volume control via GATT */
    /* For BLE, this would typically be done through a custom GATT service */
    LOG_INF("Bluetooth LE simulated volume set to %u%%", volume);
    return 0;
}

/**
 * @brief Get Bluetooth connection status
 */
bool bluetooth_audio_is_connected(void)
{
    return bt_audio.connected;
}

/**
 * @brief Get Bluetooth streaming status
 */
bool bluetooth_audio_is_streaming(void)
{
    return bt_audio.streaming;
}

/**
 * @brief Get available space in audio buffer
 */
size_t bluetooth_audio_get_free_space(void)
{
    if (!bt_audio.initialized) {
        return 0;
    }
    
    return circular_buffer_space_get(&bt_audio.audio_buffer);
}

/**
 * @brief Start scanning for Bluetooth audio devices
 */
static int bt_start_scanning(void)
{
    int ret;
    
    struct bt_le_scan_param scan_param = {
        .type = BT_LE_SCAN_TYPE_ACTIVE,
        .options = BT_LE_SCAN_OPT_NONE,
        .interval = BT_GAP_SCAN_FAST_INTERVAL,
        .window = BT_GAP_SCAN_FAST_WINDOW,
    };
    
    LOG_INF("🔍 Scanning for Bluetooth audio devices...");
    LOG_INF("💡 Make sure your Bose QC Whisper headphones are in pairing mode");
    
    ret = bt_le_scan_start(&scan_param, bt_scan_callback);
    if (ret) {
        LOG_ERR("Failed to start scanning: %d", ret);
        return ret;
    }
    
    bt_audio.scanning = true;
    LOG_INF("✅ Bluetooth scanning started - looking for audio devices");
    return 0;
}

/**
 * @brief Bluetooth scan callback - called when devices are discovered
 */
static void bt_scan_callback(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    char device_name[32] = {0};
    bool name_found = false;
    
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    
    /* Parse advertising data to find device name */
    while (ad->len > 1) {
        uint8_t len = net_buf_simple_pull_u8(ad);
        uint8_t type_byte = net_buf_simple_pull_u8(ad);
        
        if (len == 0) {
            break;
        }
        
        if (type_byte == BT_DATA_NAME_COMPLETE || type_byte == BT_DATA_NAME_SHORTENED) {
            size_t name_len = len - 1;
            if (name_len >= sizeof(device_name)) {
                name_len = sizeof(device_name) - 1;
            }
            memcpy(device_name, ad->data, name_len);
            device_name[name_len] = '\0';
            name_found = true;
            break;
        }
        
        /* Skip this advertising data element */
        if (len > 1) {
            net_buf_simple_pull(ad, len - 1);
        }
    }
    
    if (name_found) {
        LOG_INF("📱 Found device: '%s' [%s] RSSI: %d dBm", device_name, addr_str, rssi);
        
        /* Check if this looks like audio device (Bose, Sony, etc.) */
        if (strstr(device_name, "Bose") || strstr(device_name, "QC") || 
            strstr(device_name, "Whisper") || strstr(device_name, "headphone") ||
            strstr(device_name, "Headphone") || strstr(device_name, "Audio")) {
            
            LOG_INF("🎧 AUDIO DEVICE DETECTED: %s", device_name);
            LOG_INF("🔗 Attempting to connect...");
            
            /* Stop scanning and try to connect */
            bt_le_scan_stop();
            bt_audio.scanning = false;
            
            /* Store target address */
            memcpy(&bt_audio.target_addr, addr, sizeof(bt_addr_le_t));
            bt_audio.target_found = true;
            
            /* Attempt connection */
            struct bt_conn *conn;
            int ret = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &conn);
            if (ret) {
                LOG_ERR("Failed to initiate connection: %d", ret);
                /* Resume scanning */
                bt_start_scanning();
            } else {
                LOG_INF("Connection initiated to %s...", device_name);
            }
            
            return;  // Don't log every device if we found our target
        }
    } else {
        LOG_DBG("Device [%s] RSSI: %d dBm (no name)", addr_str, rssi);
    }
}

/**
 * @brief Get Bluetooth scanning status
 */
bool bluetooth_audio_is_scanning(void)
{
    return bt_audio.scanning;
}

/**
 * @brief Stop scanning and start advertising if no device found
 */
int bluetooth_audio_fallback_to_advertising(void)
{
    if (bt_audio.scanning) {
        bt_le_scan_stop();
        bt_audio.scanning = false;
        LOG_INF("Stopped scanning, starting advertising mode");
    }
    
    return bt_start_advertising();
}

/**
 * @brief Cleanup Bluetooth audio system
 */
int bluetooth_audio_cleanup(void)
{
    if (!bt_audio.initialized) {
        return 0;
    }
    
    /* Stop streaming */
    bluetooth_audio_stop();
    
    /* Disconnect if connected */
    if (bt_audio.connected && bt_audio.conn) {
        bt_conn_disconnect(bt_audio.conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        bt_conn_unref(bt_audio.conn);
        bt_audio.conn = NULL;
    }
    
    /* Cleanup circular buffer */
    circular_buffer_cleanup(&bt_audio.audio_buffer);
    
    bt_audio.initialized = false;
    bt_audio.connected = false;
    bt_audio.streaming = false;
    
    LOG_INF("Bluetooth audio system cleaned up");
    return 0;
}

/**
 * @brief Start device discovery to find nearby Bluetooth audio devices
 */
int bluetooth_audio_discover_devices(void)
{
    if (!bt_audio.initialized) {
        LOG_ERR("Bluetooth not initialized");
        return -EINVAL;
    }
    
    LOG_INF("Starting Bluetooth device discovery...");
    LOG_INF("Put your headphones in pairing mode now");
    
    /* TODO: Implement proper device discovery for A2DP devices */
    /* For now, just indicate we're ready for connections */
    LOG_INF("Device 'MP3-Rewind' is discoverable - connect from your headphones");
    
    return 0;
}

/**
 * @brief GATT Audio Control Callback - handles remote control commands
 */
static void bt_audio_control_callback(const gatt_audio_control_t *control)
{
    if (!control) {
        return;
    }
    
    LOG_INF("🎛️ Processing audio control command from client");
    
    switch (control->command) {
        case AUDIO_CMD_PLAY:
            LOG_INF("▶️  Remote PLAY command - starting audio streaming");
            bluetooth_audio_start();
            break;
            
        case AUDIO_CMD_PAUSE:
            LOG_INF("⏸️  Remote PAUSE command - pausing audio streaming");
            bluetooth_audio_stop();
            break;
            
        case AUDIO_CMD_STOP:
            LOG_INF("⏹️  Remote STOP command - stopping audio streaming");
            bluetooth_audio_stop();
            circular_buffer_clear(&bt_audio.audio_buffer);
            break;
            
        case AUDIO_CMD_VOLUME:
            LOG_INF("🔊 Remote VOLUME command: %u%%", control->volume);
            bluetooth_audio_set_volume(control->volume);
            break;
            
        case AUDIO_CMD_MUTE:
            LOG_INF("🔇 Remote MUTE command");
            bluetooth_audio_set_volume(0);
            break;
            
        case AUDIO_CMD_UNMUTE:
            LOG_INF("🔊 Remote UNMUTE command - restoring volume");
            bluetooth_audio_set_volume(bt_audio.volume);
            break;
            
        default:
            LOG_WRN("❓ Unknown remote control command: %u", control->command);
            break;
    }
}

/**
 * @brief Test audio generation work function
 */
static void test_audio_generation_work(struct k_work *work)
{
    if (!bt_audio.connected) {
        return;
    }
    
    LOG_INF("🎵 Generating test audio for GATT streaming...");
    
    /* Generate a simple 440Hz sine wave test tone */
    uint8_t test_audio[512];
    const int sample_rate = 44100;
    const int frequency = 440;
    const int samples = sizeof(test_audio) / 4; // 16-bit stereo
    
    for (size_t i = 0; i < samples; i++) {
        double t = (double)i / sample_rate;
        double sample_value = sin(2.0 * M_PI * frequency * t) * 16000;
        int16_t sample = (int16_t)sample_value;
        
        // Stereo output
        test_audio[i * 4 + 0] = sample & 0xFF;
        test_audio[i * 4 + 1] = (sample >> 8) & 0xFF;
        test_audio[i * 4 + 2] = sample & 0xFF;
        test_audio[i * 4 + 3] = (sample >> 8) & 0xFF;
    }
    
    /* Write test audio to buffer */
    size_t written = circular_buffer_write(&bt_audio.audio_buffer, test_audio, sizeof(test_audio));
    LOG_INF("🎵 Generated %zu bytes of test audio", written);
    
    /* Schedule next audio generation if still connected and streaming */
    if (bt_audio.connected && bt_audio.streaming) {
        struct k_work_delayable *delayable_work = k_work_delayable_from_work(work);
        k_work_schedule(delayable_work, K_MSEC(200)); // 200ms intervals
    }
}
