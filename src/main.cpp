#include <Arduino.h>
#include <WiFi.h>
#include <MIDI.h>

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, DIN_MIDI0);

int lastValue = 0;

void mapAnalogToMIDI(int analogPin, int midiChannel, int midiControl)
{
  int value = analogRead(analogPin) / 8;
  if (value != lastValue)
  {
    lastValue = value;
    DIN_MIDI0.sendControlChange(midiControl, value, midiChannel);
  }
}

void setup()
{
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  DIN_MIDI0.begin(MIDI_CHANNEL_OMNI);
  pinMode(A0, INPUT);
}

void loop()
{
  mapAnalogToMIDI(A0, 1, 1);
}
