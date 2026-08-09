#include <Arduino.h>
#include <AcousticSense_v3_inferencing.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"

// --- OLED Display Libraries ---
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- I2C OLED Pins (ESP32-S3 Defaults) ---
#define I2C_SDA 8
#define I2C_SCL 9
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Custom I2S Pins for INMP441 ---
#define I2S_WS 4
#define I2S_SCK 5
#define I2S_SD 6
#define I2S_PORT I2S_NUM_0

/** Audio buffers, pointers and selectors */
typedef struct {
    signed short *buffers[2];
    unsigned char buf_select;
    unsigned char buf_ready;
    unsigned int buf_count;
    unsigned int n_samples;
} inference_t;

static inference_t inference;
static const uint32_t sample_buffer_size = 2048;
static signed short sampleBuffer[sample_buffer_size];
static bool debug_nn = false;
static int print_results = -(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW);
static bool record_status = true;

// Scrolling waveform history array (42 bars across 128px screen)
static uint8_t waveHistory[42] = {0}; 

// Forward declarations
static bool microphone_inference_start(uint32_t n_samples);
static bool microphone_inference_record(void);
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr);
static int i2s_init(uint32_t sampling_rate);
static int i2s_deinit(void);
void drawAudioBars(int peak);

void setup()
{
    Serial.begin(115200);
    while (!Serial);

    // 1. Initialize OLED Display
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000); // Overclock I2C to 400kHz for smooth graphics
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
    }
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("Starting TinyML...");
    display.display();

    // 2. Initialize Edge Impulse
    run_classifier_init();
    
    if (microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE) == false) {
        Serial.println("ERR: Could not allocate audio buffer");
        display.clearDisplay();
        display.setCursor(0,0);
        display.print("Audio Buffer Error!");
        display.display();
        return;
    }
    Serial.println("Recording...");
}

void loop()
{
    bool m = microphone_inference_record();
    if (!m) return;

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
    signal.get_data = &microphone_audio_signal_get_data;
    ei_impulse_result_t result = {0};

    EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK) return;

    if (++print_results >= (EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW)) {
        
        // Find the label with the highest confidence score
        float highest_score = 0;
        const char* best_label = "Unknown";

        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            if (result.classification[ix].value > highest_score) {
                highest_score = result.classification[ix].value;
                best_label = result.classification[ix].label;
            }
        }

        // --- MAP THE LABELS TO CUSTOM DISPLAY NAMES ---
        String display_name = best_label; // Default to raw label
        if (strcmp(best_label, "Motor normal") == 0) display_name = "Healthy";
        else if (strcmp(best_label, "Motor friction") == 0) display_name = "Friction";
        else if (strcmp(best_label, "Motor loose wire") == 0) display_name = "Loose Wire";
        else if (strcmp(best_label, "ambient") == 0) display_name = "Ambient";

        // Output to Serial Monitor
        Serial.printf("Predicted: %s (%.2f%%)\n", best_label, highest_score * 100);

        // Clear ONLY the text region (y=0 to y=43) so we don't erase the waveform
        display.fillRect(0, 0, 128, 44, SSD1306_BLACK);
        
        // Header
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.print("Motor Status:");

        // Prediction Result (Large Text)
        display.setTextSize(2);
        display.setCursor(0, 16);
        display.print(display_name);

        // Confidence Percentage
        display.setTextSize(1);
        display.setCursor(0, 35);
        display.print("Conf: ");
        display.print(highest_score * 100, 1); 
        display.print("%");

        print_results = 0;
    }
}

// --- Draw Animated Symmetrical Waveform ---
// --- Draw Animated Stationary Symmetrical Bars ---
void drawAudioBars(int peak) {
    int level = 0; 
    int noiseGate = 1500; // Background noise threshold
    
    // Map peak volume to 1-9 pixel height (symmetrical = 18px total height)
    if (peak > noiseGate) {
        level = map(peak, noiseGate, 12000, 1, 9); 
    }
    if (level > 9) level = 9;
    if (level < 0) level = 0;

    // Clear ONLY the bottom right corner region (y=44 to y=63)
    display.fillRect(92, 44, 36, 20, SSD1306_BLACK);

    int centerY = 54; // Center axis for the symmetrical bars

    // Draw 8 stationary, slim bars
    for(int i = 0; i < 8; i++) {
        // Add dynamic flutter so the bars bounce independently
        int h = level;
        if (h > 0) {
            h = level - random(0, level / 2 + 1);
            if (h < 1) h = 1;
        }
        
        // Space the bars out (2px bar, 2px gap)
        int x = 94 + (i * 4); 
        
        if (h == 0) {
            // Silence baseline dot 
            display.drawPixel(x, centerY, SSD1306_WHITE);
            display.drawPixel(x + 1, centerY, SSD1306_WHITE);
        } else {
            // Draw symmetrical slim bar (Top and Bottom from center)
            display.drawLine(x, centerY - h, x, centerY + h, SSD1306_WHITE);
            display.drawLine(x + 1, centerY - h, x + 1, centerY + h, SSD1306_WHITE);
        }
    }
    
    // Push the updated graphics to the screen
    display.display(); 
}
// --- Audio Callback ---
static void audio_inference_callback(uint32_t n_bytes)
{
    for(int i = 0; i < n_bytes>>1; i++) {
        inference.buffers[inference.buf_select][inference.buf_count++] = sampleBuffer[i];

        if(inference.buf_count >= inference.n_samples) {
            inference.buf_select ^= 1;
            inference.buf_count = 0;
            inference.buf_ready = 1;
        }
    }
}

// --- Custom INMP441 Capture Task ---
static void capture_samples(void* arg) {
  const int32_t expected_16bit_bytes = (uint32_t)arg; 
  const int32_t bytes_to_read_32bit = expected_16bit_bytes * 2;
  int32_t* raw_buffer = (int32_t*)malloc(bytes_to_read_32bit);
  
  size_t bytes_read = 0;

  while (record_status) {
    i2s_read(I2S_PORT, (void*)raw_buffer, bytes_to_read_32bit, &bytes_read, portMAX_DELAY);

    if (bytes_read > 0) {
      int samples_read = bytes_read / sizeof(int32_t);
      
      for (int i = 0; i < samples_read; i++) {
        sampleBuffer[i] = (int16_t)(raw_buffer[i] >> 14); 
      }

      if (record_status) {
        audio_inference_callback(samples_read * sizeof(int16_t));
      } else {
        break;
      }
    }
  }
  free(raw_buffer);
  vTaskDelete(NULL);
}

// --- AI Setup & Buffers ---
static bool microphone_inference_start(uint32_t n_samples)
{
    inference.buffers[0] = (signed short *)malloc(n_samples * sizeof(signed short));
    if (inference.buffers[0] == NULL) return false;

    inference.buffers[1] = (signed short *)malloc(n_samples * sizeof(signed short));
    if (inference.buffers[1] == NULL) {
        ei_free(inference.buffers[0]);
        return false;
    }

    inference.buf_select = 0;
    inference.buf_count = 0;
    inference.n_samples = n_samples;
    inference.buf_ready = 0;

    if (i2s_init(EI_CLASSIFIER_FREQUENCY)) {
        Serial.println("Failed to start I2S!");
    }

    ei_sleep(100);
    record_status = true;
    xTaskCreate(capture_samples, "CaptureSamples", 1024 * 32, (void*)sample_buffer_size, 10, NULL);
    return true;
}

static bool microphone_inference_record(void)
{
    if (inference.buf_ready == 1) {
        Serial.println("Error sample buffer overrun.");
        return false;
    }
    
    unsigned long lastAnimTime = 0;
    
    // While accumulating audio frames for inference...
    while (inference.buf_ready == 0) {
        // Refresh animation every 50ms (~20 FPS)
        if (millis() - lastAnimTime > 50) {
            lastAnimTime = millis();
            
            int peak = 0;
            int start_idx = (inference.buf_count > 100) ? (inference.buf_count - 100) : 0;
            for (int i = start_idx; i < inference.buf_count; i++) {
                int val = abs(inference.buffers[inference.buf_select][i]);
                if (val > peak) peak = val;
            }
            drawAudioBars(peak);
        }
        delay(1);
    }
    
    inference.buf_ready = 0;
    return true;
}

static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
    numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);
    return 0;
}

// --- Custom INMP441 I2S Driver Setup ---
static int i2s_init(uint32_t sampling_rate) {
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = sampling_rate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, 
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, 
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 512,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0,
  };
  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_SCK,
      .ws_io_num = I2S_WS,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = I2S_SD,
  };

  esp_err_t ret = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (ret != ESP_OK) Serial.println("Error in i2s_driver_install");

  ret = i2s_set_pin(I2S_PORT, &pin_config);
  if (ret != ESP_OK) Serial.println("Error in i2s_set_pin");

  ret = i2s_zero_dma_buffer(I2S_PORT);
  if (ret != ESP_OK) Serial.println("Error in initializing dma buffer");

  return int(ret);
}

static int i2s_deinit(void) {
    i2s_driver_uninstall(I2S_PORT); 
    return 0;
}