// Requires Arduino-ESP32 (includes esp-idf headers)
#include <Arduino.h>
#include "driver/rmt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

#include <BleGamepad.h>
#include "n64.h"

BleGamepad bleGamepad("N64 Controller", "ESP32-C3", 100);


const rmt_channel_t TX_CH = RMT_CHANNEL_0;
const rmt_channel_t RX_CH = RMT_CHANNEL_2;
const gpio_num_t TX_PIN = GPIO_NUM_10;
const gpio_num_t RX_PIN = GPIO_NUM_9;

N64Controller n64(TX_PIN, RX_PIN, TX_CH, RX_CH);

void setup() {
    Serial.begin(115200);
    delay(100);
    bleGamepad.begin();
}

void loop() {

  // if (!bleGamepad.isConnected()) {
  //   // Optional: still poll N64, or just wait
  //   delay(50);
  //   return;
  // }
  

  uint32_t data = n64.poll();
  Serial.printf("Got %d bits: 0x%08X\n", 32, data);


  /*
  // Adjust bit positions to match your actual decode order
  uint16_t buttons = (value >> 16) & 0xFFFF;   // example: upper 16 bits
  int8_t stickX    = (int8_t)((value >> 8) & 0xFF);
  int8_t stickY    = (int8_t)(value & 0xFF);

  // Map N64 buttons to BLE gamepad buttons
  // Example mapping (you can change to taste):
  // bit 0: A, bit 1: B, bit 2: Z, bit 3: Start, etc.
  bleGamepad.resetButtons();

  if (buttons & (1 << 0)) bleGamepad.press(BUTTON_1);   // A
  if (buttons & (1 << 1)) bleGamepad.press(BUTTON_2);   // B
  if (buttons & (1 << 2)) bleGamepad.press(BUTTON_3);   // Z
  if (buttons & (1 << 3)) bleGamepad.press(BUTTON_4);   // Start
  // Add D-pad, C buttons, L, R as more BUTTON_n or POV hat

  // Map analog stick to X/Y axes (range -128..127 → -32767..32767)
  int16_t xAxis = (int16_t)stickX * 256;  // simple scaling
  int16_t yAxis = (int16_t)stickY * 256;

  bleGamepad.setLeftThumb(xAxis, yAxis);

  // Send HID report
  bleGamepad.sendReport();
  */

  delay(50); // poll interval
}
