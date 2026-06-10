// Requires Arduino-ESP32 (includes esp-idf headers)
#include <Arduino.h>
#include "driver/rmt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

#include "n64.h"

#include <BleGamepad.h>


BleGamepad bleGamepad("N64 Controller", "ESP32-C3", 100);


const rmt_channel_t TX_CH = RMT_CHANNEL_0;
const rmt_channel_t RX_CH = RMT_CHANNEL_2;
const gpio_num_t TX_PIN = GPIO_NUM_10;
const gpio_num_t RX_PIN = GPIO_NUM_9;

N64Controller n64(TX_PIN, RX_PIN, TX_CH, RX_CH);

void setup() {
    Serial.begin(115200);
    delay(100);

    BleGamepadConfiguration bleGamepadConfig;
    bleGamepadConfig.setIncludeStart(true);
    bleGamepadConfig.setTXPowerLevel(9);  // Defaults to 9 if not set. (Range: -12 to 9 dBm)
    bleGamepadConfig.setAxesMin(-128);
    bleGamepadConfig.setAxesMax(127);
    bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD);
    bleGamepad.begin(&bleGamepadConfig);
}

void loop() {

  if (!bleGamepad.isConnected()) {
    Serial.printf("No connection!\n");
    delay(500);
    return;
  }
  

  uint32_t data = n64.poll();
  // Serial.printf("Got %d bits: 0x%08X\n", 32, data);

  N64State state(data);

  // Adjust bit positions to match your actual decode order
  // juint16_t buttons = (value >> 16) & 0xFFFF;   // example: upper 16 bits
  // jint8_t stickX    = (int8_t)((value >> 8) & 0xFF);
  // jint8_t stickY    = (int8_t)(value & 0xFF);

  // Map N64 buttons to BLE gamepad buttons
  // Example mapping (you can change to taste):
  // bit 0: A, bit 1: B, bit 2: Z, bit 3: Start, etc.
  bleGamepad.resetButtons();


  // MAPPINGS
  if(state.a) bleGamepad.press(BUTTON_1);
  if(state.b) bleGamepad.press(BUTTON_2);
  if(state.z) bleGamepad.press(BUTTON_3);
  if(state.start) bleGamepad.pressStart(); else bleGamepad.releaseStart();

  // // Map analog stick to X/Y axes (range -128..127 → -32767..32767)
  int16_t xAxis = (int16_t)state.x * 1.6;  // simple scaling
  int16_t yAxis = -(int16_t)state.y * 1.6;

  bleGamepad.setLeftThumb(xAxis, yAxis);

  // Send HID report
  bleGamepad.sendReport();
  // Serial.printf("X: 0x%02x\t Y: 0x%02x\t\n", state.x, state.y);
  Serial.printf("RAW: 0x%08x\t X: %d\t Y: %d\txRaw: %d\t yRaw: %d\t\n", data, xAxis, yAxis, state.x, state.y);

  delay(50); // poll interval
}
