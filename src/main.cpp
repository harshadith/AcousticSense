#include <Arduino.h>
#include <driver/i2s.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AcousticSense_inferencing.h> // Ensure this matches your downloaded library name!

// Pin Definitions - INMP441 Mic
#define I2S_WS   GPIO_NUM_4
#define I2S_SCK  GPIO_NUM_5
#define I2S_SD   GPIO_NUM_6
#define I2S_PORT I2S_NUM_0

// Pin Definitions - OLED Display (I2C)
#define OLED_SDA 8
#define OLED_SCL 9
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SAMPLE_RATE 16000
int16_t *audio_buffer;
static bool debug_nn = false;

void i2s_init() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 512,
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
    numpy::int16_to_float(&audio_buffer[offset], out_ptr, length);
    return 0;
}

void record_audio() {
    size_t bytes_read;
    int32_t i2s_read_buffer[512];

    int buffer_index = 0;
    while (buffer_index < EI_CLASSIFIER_RAW_SAMPLE_COUNT) {
        i2s_read(I2S_PORT, (void *)i2s_read_buffer, sizeof(i2s_read_buffer), &bytes_read, portMAX_DELAY);
        int samples_read = bytes_read / sizeof(int32_t);

        for (int i = 0; i < samples_read && buffer_index < EI_CLASSIFIER_RAW_SAMPLE_COUNT; i++) {
            audio_buffer[buffer_index++] = (int16_t)(i2s_read_buffer[i] >> 14);
        }
    }
}

void update_oled(const char* label, float confidence) {
    display.clearDisplay();

    // Top Header Bar
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("MOTOR MONITOR");
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    // Classification Result
    display.setCursor(0, 20);
    display.setTextSize(2);
    
    // Capitalize / format label display
    if (strcmp(label, "clean") == 0) {
        display.println("HEALTHY");
    } else if (strcmp(label, "friction") == 0) {
        display.println("FRICTION!");
    } else if (strcmp(label, "loose_wire") == 0) {
        display.println("LOOSE WIRE");
    } else {
        display.println("AMBIENT");
    }

    // Confidence Score Bar
    display.setTextSize(1);
    display.setCursor(0, 48);
    display.printf("Conf: %.1f%%", confidence * 100.0);
    
    // Visual progress bar for confidence
    int barWidth = (int)(confidence * 128.0);
    display.fillRect(0, 58, barWidth, 6, SSD1306_WHITE);

    display.display();
}

void setup() {
    Serial.begin(115200);

    // Initialize OLED on custom GPIOs 8 (SDA) and 9 (SCL)
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("OLED Allocation Failed!");
        for(;;);
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 25);
    display.println("Initializing System...");
    display.display();

    audio_buffer = (int16_t *)malloc(EI_CLASSIFIER_RAW_SAMPLE_COUNT * sizeof(int16_t));
    if (!audio_buffer) {
        Serial.println("RAM Allocation Failed!");
        while(1);
    }

    i2s_init();
    delay(1000);
}

void loop() {
    record_audio();

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &microphone_audio_signal_get_data;

    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK) {
        return;
    }

    // Find the prediction with the highest confidence value
    size_t max_index = 0;
    float max_value = 0.0;
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        if (result.classification[ix].value > max_value) {
            max_value = result.classification[ix].value;
            max_index = ix;
        }
    }

    // Update screen with highest probability result
    update_oled(result.classification[max_index].label, max_value);
}