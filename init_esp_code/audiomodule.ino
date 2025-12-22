#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include "AudioFileSourcePROGMEM.h"
#include "samplemp3.h"


AudioFileSourcePROGMEM *in;
AudioGeneratorMP3 *mp3;
AudioOutputI2S *out;

#define AMP_GAIN_PIN 4


void setup() {
  Serial.begin(115200);

  pinMode(AMP_GAIN_PIN, OUTPUT);
  digitalWrite(AMP_GAIN_PIN, HIGH); // HIGH = loud

  in  = new AudioFileSourcePROGMEM(samplemp3, sizeof(samplemp3));
  mp3 = new AudioGeneratorMP3();
  out = new AudioOutputI2S();

  out->SetGain(0.8);
  out->SetPinout(26, 25, 22);  // BCLK, LRCK, DOUT

  mp3->begin(in, out);
}


void loop() {
  if (mp3->isRunning()) {
    if (!mp3->loop()) mp3->stop();
  } else {
    Serial.println("MP3 finished");
    delay(1000);
  }
}
