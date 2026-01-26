#include "audio.h"
#include "driver/i2s.h"
#include <math.h>

// ───────────────────────── Pin configuration ─────────────────────────
#define I2S_BCLK 18
#define I2S_LRCK 19
#define I2S_DOUT 23

#define AMP_SD_PIN    21
#define AMP_GAIN_PIN  25

// ───────────────────────── Audio config ─────────────────────────
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_DMA_BUF_LEN 128
#define AUDIO_DMA_BUF_CNT 8
#define AUDIO_AMPLITUDE   4000

static bool audioInited = false;

// ───────────────────────── Init ─────────────────────────
void audio_init() {
  if (audioInited) return;
  audioInited = true;

  pinMode(AMP_SD_PIN, OUTPUT);
  digitalWrite(AMP_SD_PIN, HIGH);

  pinMode(AMP_GAIN_PIN, OUTPUT);
  digitalWrite(AMP_GAIN_PIN, HIGH); // louder (board dependent)

  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = AUDIO_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = AUDIO_DMA_BUF_CNT,
    .dma_buf_len = AUDIO_DMA_BUF_LEN,
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

  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

// ───────────────────────── Beep generator ─────────────────────────
void audio_playBeep(uint16_t freq, uint16_t duration_ms) {
  static int16_t buffer[256 * 2];
  float phase = 0.0f;

  const float phaseStep = 2.0f * PI * freq / AUDIO_SAMPLE_RATE;
  const int samplesTotal = (AUDIO_SAMPLE_RATE * duration_ms) / 1000;
  int samplesSent = 0;

  while (samplesSent < samplesTotal) {
    for (int i = 0; i < 256; i++) {
      int16_t s = (int16_t)(sinf(phase) * AUDIO_AMPLITUDE);
      buffer[2*i]     = s;
      buffer[2*i + 1] = s;

      phase += phaseStep;
      if (phase >= 2.0f * PI) phase -= 2.0f * PI;
    }

    size_t written;
    i2s_write(I2S_NUM_0, buffer, sizeof(buffer), &written, portMAX_DELAY);
    samplesSent += 256;
  }
}

// ───────────────────────── Semantic events ─────────────────────────
void audio_playEvent(AudioEvent e) {
  switch (e) {
    case AUDIO_ROUND_START:
      audio_playBeep(1200, 200);
      break;

    case AUDIO_SUCCESS:
      audio_playBeep(1600, 200);
      break;

    case AUDIO_FAIL:
      audio_playBeep(400, 200);
      break;
  }
}
