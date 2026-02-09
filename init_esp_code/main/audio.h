#pragma once
#include <Arduino.h>

// ───────────────────────── Audio events ─────────────────────────
enum AudioEvent {
  AUDIO_ROUND_START,
  AUDIO_SUCCESS,
  AUDIO_FAIL
};

// ───────────────────────── Public API ─────────────────────────
void audio_init();
void audio_playBeep(uint16_t freq, uint16_t duration_ms);
void audio_playEvent(AudioEvent e);
