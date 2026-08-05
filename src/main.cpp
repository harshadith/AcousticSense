#include <Arduino.h>
#include <driver/i2s.h>
#include <AcousticSense_inferencing.h> // Replace with your actual Edge Impulse header name

// I2S Pin Definitions
#define I2S_WS   GPIO_NUM_4
#define I2S_SCK  GPIO_NUM_5
#define I2S_SD   GPIO_NUM_6
#define I2S_PORT I2S_NUM_0

// Audio Buffer Settings
#define SAMPLE_RATE 16000
#define buffer_len 512
int16_t sBuffer[buffer_len];

static bool debug_nn = false; 

void i2s_init() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = buffer_len,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD
    };

    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
}

static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    size_t bytes_read = 0;
    i2s_read(I2S_PORT, (void *)sBuffer, length * sizeof(int16_t), &bytes_read, portMAX_DELAY);
    
    for (size_t i = 0; i < length; i++) {
        out_ptr[i] = (float)sBuffer[i];
    }
    return 0;
}

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println("Initializing Acoustic Motor Monitoring Node...");
    i2s_init();
    
    if (EI_CLASSIFIER_RAW_SAMPLE_COUNT > buffer_len) {
        Serial.println("ERROR: EI Buffer size mismatch");
    }
}

void loop() {
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &microphone_audio_signal_get_data;

    ei_impulse_result_t result = { 0 };

    EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK) {
        Serial.printf("ERR: Failed to run classifier (%d)\n", r);
        return;
    }

    // Print inference results
    Serial.printf("Predictions (DSP: %d ms, Classification: %d ms):\n",
        result.timing.dsp, result.timing.classification);
        
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        Serial.printf("  %s: %.5f\n", result.classification[ix].label, result.classification[ix].value);
    }

    delay(1000); // Poll every second
}