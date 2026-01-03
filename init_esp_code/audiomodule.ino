#if 0
#include <Arduino.h>
#include "driver/i2s.h"
#include <math.h>

#define I2S_BCLK 18
#define I2S_LRCK 19
#define I2S_DOUT 23

#define AMP_SD_PIN    21   // later move off SDA
#define AMP_GAIN_PIN  25   // ⚠ shared with WS2812 UP panel (temporary only)

#define SAMPLE_RATE 44100
#define FREQ        1000
#define AMP         4000

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("SETUP");

  // Enable amp
  pinMode(AMP_SD_PIN, OUTPUT);
  digitalWrite(AMP_SD_PIN, HIGH);

  // Gain control
  pinMode(AMP_GAIN_PIN, OUTPUT);
  digitalWrite(AMP_GAIN_PIN, 1);  // HIGH = louder (board-dependent)

  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 128,
    .use_apll = true,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num  = I2S_LRCK,
    .data_out_num = I2S_DOUT,
    .data_in_num  = I2S_PIN_NO_CHANGE
  };

  Serial.printf("i2s_driver_install=%d\n",
                (int)i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL));
  Serial.printf("i2s_set_pin=%d\n",
                (int)i2s_set_pin(I2S_NUM_0, &pins));

  i2s_zero_dma_buffer(I2S_NUM_0);

  Serial.println("Beep test running");
}

void loop() {
  static int16_t buffer[256 * 2];
  static float phase = 0;

  for (int i = 0; i < 256; i++) {
    int16_t s = (int16_t)(sinf(phase) * AMP);
    buffer[2*i]   = s;
    buffer[2*i+1] = s;

    phase += 2.0f * PI * FREQ / SAMPLE_RATE;
    if (phase >= 2.0f * PI) phase -= 2.0f * PI;
  }

  size_t written;
  i2s_write(I2S_NUM_0, buffer, sizeof(buffer), &written, portMAX_DELAY);
}
#endif