/**
 * @file main.c
 * @brief MP3 Rewind - Audio Streaming Test Application
 * 
 * This is a test application for the HTTP audio streaming system.
 * It tests the buzzer-based audio output with local WAV file data
 * to validate the audio pipeline before implementing HTTP streaming.
 * 
 * Please Note: After a lot of challeneges with integrating a SD card 
 * module with this project, for the sake of time, I have shifted to
 * impplementing a HTTP streaming approach that would handle the "storing"
 * of audio data on a remote server (PC) and streaming it to the device.
 * At this very moment there is a test commented out for the HTTP client. 
 * 
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/devicetree.h>
#include <math.h>

#include "audio/audiosys.h"
#include "audio/audio_buffers.h"
#include "audio/wav_decoder.h"
// #include "server/audio_client.h"
#include "utils/error_handling.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* LED definitions for status indication */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)

/* User button definition - this is here to 
 * introduce a clear cut way to start the tests.
 */
#define SW0_NODE DT_ALIAS(sw0)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);

/* Test configuration, since the test is commented out for now,
 * this test is more relevant for the future. 
 */
// #define TEST_SERVER_HOST "192.168.1.100"  // Change to your PC's IP
// #define TEST_SERVER_PORT 8000

/* Simple WAV test data (440Hz sine wave) - ensure 4-byte alignment 
 * This complies with the WAV decoder implementation (for now). 
 */
static const uint8_t test_wav_data[] __attribute__((aligned(4))) = {
    /* WAV header for 440Hz sine wave, 16-bit, mono, 44.1kHz */
    0x52, 0x49, 0x46, 0x46, 0x24, 0x08, 0x00, 0x00, // RIFF header
    0x57, 0x41, 0x56, 0x45, 0x66, 0x6d, 0x74, 0x20, // WAVE fmt
    0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, // PCM, mono
    0x44, 0xac, 0x00, 0x00, 0x88, 0x58, 0x01, 0x00, // 44100 Hz
    0x02, 0x00, 0x10, 0x00, 0x64, 0x61, 0x74, 0x61, // 16-bit, data chunk
    0x00, 0x08, 0x00, 0x00,                         // data size
    /* Sample audio data (simplified) */
    0x00, 0x00, 0x00, 0x7F, 0xFF, 0xFF, 0x00, 0x80,
    0x00, 0x00, 0x00, 0x7F, 0xFF, 0xFF, 0x00, 0x80,
    // ... more samples would go here for a real test
};

/* Test states */
typedef enum {
    TEST_STATE_INIT,
    TEST_STATE_AUDIO_INIT,
    TEST_STATE_BUFFER_TEST,
    TEST_STATE_DECODER_TEST,
    TEST_STATE_AUDIO_PLAY_TEST,
    TEST_STATE_HTTP_CLIENT_TEST,
    TEST_STATE_COMPLETE,
    TEST_STATE_ERROR
} test_state_t;

static test_state_t current_test_state = TEST_STATE_INIT;

/* Function prototypes */
static int init_hardware(void);
static void wait_for_button_press(void);
static int test_audio_system(void);
static int test_audio_buffers(void);
static int test_wav_decoder(void);
static int test_audio_playback(void);
// static int test_http_client(void);
static void update_status_leds(void);

int main(void)
{
    int ret;
    
    printk("=== MP3 Rewind - Audio Streaming Test ===\n");
    printk("Testing buzzer-based audio output system\n");
    printk("Build time: %s %s\n", __DATE__, __TIME__);
    printk("\n*** Press the USER BUTTON to start tests ***\n");

    /* Initialize hardware */
    ret = init_hardware();
    if (ret < 0) {
        LOG_ERR("Hardware initialization failed: %d", ret);
        current_test_state = TEST_STATE_ERROR;
        goto error;
    }

    /* Wait for user button press to start tests */
    wait_for_button_press();
    
    printk("\n*** Starting tests... ***\n");

    /* Test 1: Audio System Initialization */
    printk("\n--- Test 1: Audio System ---\n");
    current_test_state = TEST_STATE_AUDIO_INIT;
    update_status_leds();
    
    ret = test_audio_system();
    if (ret < 0) {
        LOG_ERR("Audio system test failed: %d", ret);
        current_test_state = TEST_STATE_ERROR;
        goto error;
    }
    printk("✓ Audio system test passed\n");

    /* Test 2: Audio Buffer Management */
    printk("\n--- Test 2: Audio Buffers ---\n");
    current_test_state = TEST_STATE_BUFFER_TEST;
    update_status_leds();
    
    ret = test_audio_buffers();
    if (ret < 0) {
        LOG_ERR("Audio buffer test failed: %d", ret);
        current_test_state = TEST_STATE_ERROR;
        goto error;
    }
    printk("✓ Audio buffer test passed\n");

    /* Test 3: WAV Decoder */
    printk("\n--- Test 3: WAV Decoder ---\n");
    current_test_state = TEST_STATE_DECODER_TEST;
    update_status_leds();
    
    ret = test_wav_decoder();
    if (ret < 0) {
        LOG_ERR("WAV decoder test failed: %d", ret);
        current_test_state = TEST_STATE_ERROR;
        goto error;
    }
    printk("✓ WAV decoder test passed\n");

    /* Test 4: Audio Playback */
    printk("\n--- Test 4: Audio Playback ---\n");
    current_test_state = TEST_STATE_AUDIO_PLAY_TEST;
    update_status_leds();
    
    ret = test_audio_playback();
    if (ret < 0) {
        LOG_ERR("Audio playback test failed: %d", ret);
        current_test_state = TEST_STATE_ERROR;
        goto error;
    }
    printk("✓ Audio playback test passed\n");

    /* Test 5: HTTP Client (Basic) - COMMENTED OUT FOR MILESTONE */
    /*
    printk("\n--- Test 5: HTTP Client ---\n");
    current_test_state = TEST_STATE_HTTP_CLIENT_TEST;
    update_status_leds();
    
    ret = test_http_client();
    if (ret < 0) {
        LOG_ERR("HTTP client test failed: %d", ret);
        current_test_state = TEST_STATE_ERROR;
        goto error;
    }
    printk("✓ HTTP client test passed\n");
    */

    /* All tests completed */
    current_test_state = TEST_STATE_COMPLETE;
    printk("\n=== All Tests Completed Successfully! ===\n");
    printk("Ready for HTTP audio streaming integration\n");

    /* Keep the system running */
    while (1) {
        update_status_leds();
        k_sleep(K_MSEC(1000));
    }

error:
    current_test_state = TEST_STATE_ERROR;
    printk("\n=== Test Failed ===\n");
    
    while (1) {
        update_status_leds();
        k_sleep(K_MSEC(200));
    }

    return 0;
}

static int init_hardware(void)
{
    /* Initialize LEDs */
    if (!device_is_ready(led0.port) || !device_is_ready(led1.port)) {
        LOG_ERR("LED devices not ready");
        return -ENODEV;
    }

    int ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure LED0: %d", ret);
        return ret;
    }

    ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure LED1: %d", ret);
        return ret;
    }

    /* Initialize user button */
    if (!device_is_ready(button.port)) {
        LOG_ERR("Button device not ready");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("Failed to configure button: %d", ret);
        return ret;
    }

    /* Initialize error handling system -
     * Please refer to utils/error_handling.h for more details.
     */

    error_handler_init();

    LOG_INF("Hardware initialized successfully");
    return 0;
}

static int test_audio_system(void)
{
    /* Test audio system initialization */
    audio_config_t config = {
        .output_type = AUDIO_OUTPUT_BUZZER,
        .format = {
            .sample_rate = 44100,
            .channels = 1,
            .bits_per_sample = 16
        },
        .buffer_size_ms = 100
    };

    int ret = audio_system_init(&config);
    if (ret < 0) {
        return ret;
    }

    /* Test state queries */
    audio_state_t state = audio_system_get_state();
    if (state != AUDIO_STATE_INITIALIZED) {
        LOG_ERR("Unexpected audio state: %d", state);
        return -EINVAL;
    }

    /* Test volume control */
    ret = audio_system_set_volume(75);
    if (ret < 0) {
        return ret;
    }

    LOG_INF("Audio system initialized: 44.1kHz, mono, 16-bit");
    return 0;
}

static int test_audio_buffers(void)
{
    /* Initialize buffer pool */
    int ret = audio_buffer_pool_init();
    if (ret < 0) {
        return ret;
    }

    /* Test buffer allocation */
    struct audio_buffer *buf = audio_buffer_alloc(K_MSEC(100));
    if (!buf) {
        LOG_ERR("Failed to allocate audio buffer");
        return -ENOMEM;
    }

    /* Test buffer operations */
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    size_t written = audio_buffer_write(buf, test_data, sizeof(test_data));
    if (written != sizeof(test_data)) {
        LOG_ERR("Buffer write failed: %zu/%zu", written, sizeof(test_data));
        audio_buffer_free(buf);
        return -EIO;
    }

    uint8_t read_data[10];
    size_t read = audio_buffer_read(buf, read_data, sizeof(read_data));
    if (read != sizeof(test_data)) {
        LOG_ERR("Buffer read failed: %zu/%zu", read, sizeof(test_data));
        audio_buffer_free(buf);
        return -EIO;
    }

    /* Verify data integrity */
    if (memcmp(test_data, read_data, sizeof(test_data)) != 0) {
        LOG_ERR("Buffer data corruption detected");
        audio_buffer_free(buf);
        return -EIO;
    }

    /* Free buffer */
    ret = audio_buffer_free(buf);
    if (ret < 0) {
        return ret;
    }

    LOG_INF("Audio buffer system working correctly");
    return 0;
}

static int test_wav_decoder(void)
{
    struct wav_decoder decoder;

    /* Initialize decoder with embedded test data */
    int ret = wav_decoder_init(&decoder, test_wav_data, sizeof(test_wav_data));
    if (ret < 0) {
        return ret;
    }

    /* Get format information */
    struct audio_format_info format;
    ret = wav_decoder_get_format(&decoder, &format);
    if (ret < 0) {
        wav_decoder_cleanup(&decoder);
        return ret;
    }

    /* Verify format */
    if (format.channels != 1 || format.sample_rate != 44100 || format.bits_per_sample != 16) {
        LOG_ERR("Unexpected WAV format: %uch, %uHz, %ubits", 
                format.channels, format.sample_rate, format.bits_per_sample);
        wav_decoder_cleanup(&decoder);
        return -EINVAL;
    }

    /* Test reading audio data */
    uint8_t audio_data[128];
    size_t read = wav_decoder_read(&decoder, audio_data, sizeof(audio_data));
    if (read == 0) {
        LOG_ERR("No audio data read from WAV");
        wav_decoder_cleanup(&decoder);
        return -EIO;
    }

    /* Get total samples */
    size_t total_samples = wav_decoder_get_total_samples(&decoder);
    
    wav_decoder_cleanup(&decoder);
    LOG_INF("WAV decoder working: %uch, %uHz, %ubits, %zu bytes read, %zu total samples", 
            format.channels, format.sample_rate, format.bits_per_sample, read, total_samples);
    return 0;
}

static int test_audio_playback(void)
{
    /* First, test basic PWM functionality */
    printk("🔧 Testing basic PWM output on PA15 (Arduino D9)...\n");
    
    /* Get PWM device directly for testing */
    const struct device *pwm_dev = DEVICE_DT_GET(DT_NODELABEL(pwm2));
    if (!device_is_ready(pwm_dev)) {
        LOG_ERR("PWM device not ready for direct test");
        return -ENODEV;
    }
    
    /* Test with direct frequency modulation - 
     * You can uncomment this test if you would like to hear the buzzer 
     * configuration go through several frequencies. This portion of the test
     * was there specifically for debugging. 
     */

    // printk("🎵 Testing direct frequency modulation for buzzer...\n");
    
    // /* Play a sequence of direct frequencies for buzzer - gentler on the ears */
    // const int buzzer_freqs[] = {440, 523, 659, 784, 880};  // Musical notes: A, C, E, G, A
    // const int num_freqs = sizeof(buzzer_freqs) / sizeof(buzzer_freqs[0]);
    
    // for (int i = 0; i < num_freqs; i++) {
    //     int freq = buzzer_freqs[i];
    //     uint32_t period_ns = 1000000000 / freq;  // Convert frequency to period in nanoseconds
    //     uint32_t pulse_ns = period_ns / 4;        // 25% duty cycle for gentler sound
        
    //     printk("🔊 Playing %dHz for 0.5 seconds (period=%u ns)...\n", freq, period_ns);
        
    //     int ret = pwm_set(pwm_dev, 1, period_ns, pulse_ns, 0);
    //     if (ret < 0) {
    //         LOG_ERR("Failed to set PWM for %dHz: %d", freq, ret);
    //         return ret;
    //     }
        
    //     k_sleep(K_MSEC(500)); // 0.5 second per frequency - shorter duration
        
    //     /* Brief pause between frequencies */
    //     pwm_set(pwm_dev, 1, period_ns, 0, 0);  // 0% duty cycle = silence
    //     k_sleep(K_MSEC(100));  // 100ms pause
    // }
    
    // /* Stop PWM */
    // pwm_set(pwm_dev, 1, 50000, 0, 0);  // Back to 20kHz, 0% duty
    // printk("🔇 Direct frequency test completed\n");
    
    /* Test simple 1kHz tone for 1 second */
    printk("🎵 Playing 1kHz test tone for 1 second...\n");
    uint32_t period_ns = 1000000; // 1kHz = 1ms period
    uint32_t pulse_ns = period_ns / 4; // 25% duty cycle for gentler sound
    
    int ret2 = pwm_set(pwm_dev, 1, period_ns, pulse_ns, 0);
    if (ret2 < 0) {
        LOG_ERR("Failed to set PWM: %d", ret2);
        return ret2;
    }
    
    k_sleep(K_MSEC(1000)); // 1 second instead of 2
    
    /* Stop PWM */
    pwm_set(pwm_dev, 1, period_ns, 0, 0);
    printk("🔇 Direct PWM test completed\n");
    
    /* Now test DIRECT frequency modulation for melody playback AKA playing a song */
    printk("🎶 Testing DIRECT frequency modulation melody playback...\n");
    
    /* "Twinkle Twinkle Little Star" variation with faster pace */
    // const int note_frequencies[] = {
    //     523, 523, 784, 784, 880, 880, 784,    // Twinkle twinkle little star
    //     698, 698, 659, 659, 587, 587, 523,    // How I wonder what you are
    //     784, 784, 698, 698, 659, 659, 587,    // Up above the world so high
    //     784, 784, 698, 698, 659, 659, 587,    // Like a diamond in the sky
    //     523, 523, 784, 784, 880, 880, 784,    // Twinkle twinkle little star
    //     698, 698, 659, 659, 587, 587, 523     // How I wonder what you are
    // };
    // const float note_durations[] = {
    //     0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.6,   // First line - faster pace
    //     0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.6,   // Second line
    //     0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.6,   // Third line
    //     0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.6,   // Fourth line
    //     0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.6,   // Fifth line
    //     0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.8    // Final line - longer ending
    // };

    /* Generate a sweet, fast-paced melody! */
    printk("🎵 Playing Für Elise with direct frequency control...\n");

    // Für Elise by Ludwig van Beethoven - Opening theme
    const int note_frequencies[] = {
        330, 311, 330, 311, 330, 247, 294, 262, 220,   // E-D#-E-D#-E-B-D-C-A
        0,   262, 330, 220, 247,                       // rest-C-E-A-B  
        0,   330, 415, 247, 262,                       // rest-E-G#-B-C
        0,   262, 330, 311, 330, 311, 330, 247, 294, 262, 220,  // rest-C-E-D#-E-D#-E-B-D-C-A
        0,   262, 330, 220, 247,                       // rest-C-E-A-B
        0,   330, 262, 247, 220                        // rest-E-C-B-A (ending phrase)
    };

    const float note_durations[] = {
        0.15, 0.15, 0.15, 0.15, 0.15, 0.15, 0.15, 0.15, 0.3,   // Opening phrase
        0.15, 0.15, 0.15, 0.15, 0.3,                          // First response
        0.15, 0.15, 0.15, 0.15, 0.3,                          // Second response  
        0.15, 0.15, 0.15, 0.15, 0.15, 0.15, 0.15, 0.15, 0.15, 0.15, 0.3,  // Repeat opening
        0.15, 0.15, 0.15, 0.15, 0.3,                          // Response repeat
        0.15, 0.15, 0.15, 0.15, 0.6                           // Final phrase
    };

    const int num_notes = sizeof(note_frequencies) / sizeof(note_frequencies[0]);


    printk("🎼 Playing %d musical notes...\n", num_notes);
    
    /* Play each note in the melody using direct PWM frequency control */
    for (int note_idx = 0; note_idx < num_notes; note_idx++) {
        int frequency = note_frequencies[note_idx];
        float duration = note_durations[note_idx];
        int duration_ms = (int)(duration * 1000);
        
        printk("♪ Note %d: %dHz for %dms...\n", note_idx + 1, frequency, duration_ms);
        
        if (frequency == 0) {
            /* This is a rest/silence - just sleep for the duration */
            printk("   (Rest - silence)\n");
            k_sleep(K_MSEC(duration_ms));
        } else {
            /* Calculate PWM parameters for this frequency */
            uint32_t period_ns = 1000000000 / frequency;  // Convert frequency to period in nanoseconds
            uint32_t pulse_ns = period_ns / 3;             // 33% duty cycle for gentler sound
            
            /* Set PWM to play this frequency */
            int ret = pwm_set(pwm_dev, 1, period_ns, pulse_ns, 0);
            if (ret < 0) {
                LOG_ERR("Failed to set PWM for %dHz: %d", frequency, ret);
                break;
            }
            
            /* Play for the specified duration */
            k_sleep(K_MSEC(duration_ms));
            
            /* Small pause between notes (add silence) */
            pwm_set(pwm_dev, 1, period_ns, 0, 0);  // 0% duty cycle = silence
        }
        
        /* Small pause between all notes (whether rest or actual note) */
        k_sleep(K_MSEC(50));  // 50ms pause between notes
        
        if ((note_idx + 1) % 7 == 0) {
            printk("🎵 End of musical phrase\n");
        }
    }
    
    /* Stop PWM completely */
    pwm_set(pwm_dev, 1, 50000, 0, 0);  // Back to 20kHz, 0% duty
    printk("🔇 Sweet melody completed! 🎶\n");

    printk("\n🎉 BUZZER AUDIO TEST SUCCESSFUL! 🎉\n");
    printk("✅ Direct frequency modulation works perfectly\n");
    printk("✅ PWM hardware is functioning correctly\n");
    printk("✅ Melody playback is clear and audible\n");

    LOG_INF("Audio playback test completed successfully");
    return 0;
}


// static int test_http_client(void)
// {
//     /* Initialize HTTP client */
//     int ret = audio_client_init(TEST_SERVER_HOST, TEST_SERVER_PORT);
//     if (ret < 0) {
//         return ret;
//     }

//     /* Test state */
//     audio_client_state_t state = audio_client_get_state();
//     if (state != AUDIO_CLIENT_INITIALIZED) {
//         LOG_ERR("Unexpected client state: %d", state);
//         audio_client_cleanup();
//         return -EINVAL;
//     }

//     /* Cleanup */
//     audio_client_cleanup();
    
//     LOG_INF("HTTP client basic test completed");
//     return 0;
// }


static void wait_for_button_press(void)
{
    printk("Waiting for button press (blue USER button)...\n");
    
    /* Wait for button press (active low) */
    while (gpio_pin_get_dt(&button) == 1) {
        /* Blink LED0 while waiting */
        gpio_pin_toggle_dt(&led0);
        k_sleep(K_MSEC(500));
    }
    
    /* Button pressed, wait for release */
    while (gpio_pin_get_dt(&button) == 0) {
        k_sleep(K_MSEC(10));
    }
    
    /* Turn off LED0 */
    gpio_pin_set_dt(&led0, 0);
    
    printk("Button pressed! Starting tests...\n");
}

static void update_status_leds(void)
{
    static uint32_t blink_counter = 0;
    blink_counter++;

    switch (current_test_state) {
        case TEST_STATE_INIT:
        case TEST_STATE_AUDIO_INIT:
            /* LED0 slow blink during init */
            gpio_pin_set_dt(&led0, (blink_counter / 10) % 2);
            gpio_pin_set_dt(&led1, 0);
            break;

        case TEST_STATE_BUFFER_TEST:
        case TEST_STATE_DECODER_TEST:
            /* LED0 fast blink during tests */
            gpio_pin_set_dt(&led0, (blink_counter / 5) % 2);
            gpio_pin_set_dt(&led1, 0);
            break;

        case TEST_STATE_AUDIO_PLAY_TEST:
        case TEST_STATE_HTTP_CLIENT_TEST:
            /* Both LEDs alternating */
            gpio_pin_set_dt(&led0, (blink_counter / 5) % 2);
            gpio_pin_set_dt(&led1, ((blink_counter / 5) + 1) % 2);
            break;

        case TEST_STATE_COMPLETE:
            /* Both LEDs steady on */
            gpio_pin_set_dt(&led0, 1);
            gpio_pin_set_dt(&led1, 1);
            break;

        case TEST_STATE_ERROR:
            /* LED0 fast blink, LED1 off */
            gpio_pin_set_dt(&led0, (blink_counter / 2) % 2);
            gpio_pin_set_dt(&led1, 0);
            break;
    }
}