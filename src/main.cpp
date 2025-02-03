#include <Arduino.h>
#include <WiFi.h>
#include <MIDI.h>

MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, DIN_MIDI0);

midi::MidiType midiType = midi::PitchBend;

constexpr int INPUT_MIN = 460;
constexpr int OUTPUT_MAX = 1023;
constexpr int OUTPUT_MIN = -8192;
constexpr int OUTPUT_MAX = 8191;


constexpr int HYSTERESIS = 20;
int lastValue = 0;
static unsigned long lastDebounce = 0;
constexpr int minDebounceInterval = 20;

// Averaging
constexpr int AVERAGE_SIZE = 8;
int buffer[AVERAGE_SIZE];
int idx = 0;

// Track stable touches
int consecutiveInRange = 0;
int consecutiveOutOfRange = 0;
constexpr int IN_INPUT_MIN = 3;   // Require 5 consecutive in-range readings to jump from 0
constexpr int OUT_INPUT_MIN = 3;  // Require 5 consecutive out-of-range readings to reset to 0
constexpr int minDelta = 100;

void mapAnalogToMIDI(int analogPin, int midiChannel, int midiControl)
{
  // Debounce
  if (millis() - lastDebounce < minDebounceInterval) return;
  lastDebounce = millis();

  // Read and scale down the raw value
  int rawValue = analogRead(analogPin) / 4; // Scale down to 10-bit range

  // If the raw value is below the INPUT_MIN and last value is not zero, reset to zero
  if (rawValue < INPUT_MIN)
  {
    if (lastValue != 0)
    {
      lastValue = 0;
      DIN_MIDI0.sendPitchBend(0, midiChannel);
    }
    return;
  }

  // Push raw reading into ring buffer
  buffer[idx++] = rawValue;
  if (idx >= AVERAGE_SIZE) idx = 0;

  // Calculate average
  long sum = 0;
  for (int i = 0; i < AVERAGE_SIZE; i++)
    sum += buffer[i];
  int avgValue = sum / AVERAGE_SIZE;

  // Check range with hysteresis
  int lowerRange = INPUT_MIN - HYSTERESIS;
  int upperRange = OUTPUT_MAX + HYSTERESIS;
  bool inRange = (avgValue >= lowerRange && avgValue <= upperRange);

  if (inRange)
  {
    consecutiveOutOfRange = 0;
    consecutiveInRange++;

    // If we are at 0, require enough consecutive in-range before jumping to mapped
    if (lastValue == 0 && consecutiveInRange >= IN_INPUT_MIN)
    {
      int mappedValue = map(avgValue, INPUT_MIN, OUTPUT_MAX, -8192, 8191);
      mappedValue = constrain(mappedValue, -8192, 8191);

      lastValue = mappedValue;
      Serial.print("Finger down -> Mapped: ");
      Serial.println(mappedValue);
      DIN_MIDI0.sendPitchBend(mappedValue, midiChannel);

      // Reset for minor updates after finger is down
      consecutiveInRange = 0;
    }
    else if (lastValue != 0)
    {
      // Already touched: update if it’s changed by at least 100
      int mappedValue = map(avgValue, INPUT_MIN, OUTPUT_MAX, -8192, 8191);
      mappedValue = constrain(mappedValue, -8192, 8191);

      if (abs(mappedValue - lastValue) >= minDelta)
      {
        lastValue = mappedValue;
        Serial.print("Mapped: ");
        Serial.println(mappedValue);
        DIN_MIDI0.sendPitchBend(mappedValue, midiChannel);
      }
    }
  }
  else
  {
    consecutiveInRange = 0;
    consecutiveOutOfRange++;

    // After enough out-of-range readings, reset to 0
    if (lastValue != 0 && consecutiveOutOfRange >= OUT_INPUT_MIN)
    {
      lastValue = 0;
      Serial.println("Finger lifted -> reset to 0");
      DIN_MIDI0.sendPitchBend(0, midiChannel);
    }
  }
}


void setup()
{
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  DIN_MIDI0.begin(MIDI_CHANNEL_OMNI);
  pinMode(36, INPUT_PULLUP);
}

void loop()
{
  mapAnalogToMIDI(36, 1, 1);
}