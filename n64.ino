// Requires Arduino-ESP32 (includes esp-idf headers)
#include <Arduino.h>
#include "driver/rmt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

#include "n64.h"

#include <BleGamepad.h>

BleGamepad bleGamepad("N64 Controller", "ESP32-C3", 100);
static N64State state, oldState;

void toggleButton(uint8_t b){
  if(bleGamepad.isPressed(b)){
    bleGamepad.release(b);
  }
  else{
    bleGamepad.press(b);
  }
}

constexpr uint8_t BATT_PIN = A0;

constexpr rmt_channel_t TX_CH = RMT_CHANNEL_0;
constexpr rmt_channel_t RX_CH = RMT_CHANNEL_2;
constexpr gpio_num_t TX_PIN = GPIO_NUM_10;
constexpr gpio_num_t RX_PIN = GPIO_NUM_9;


N64Controller n64(TX_PIN, RX_PIN, TX_CH, RX_CH);

void setup() {
    Serial.begin(115200);
    delay(100);

    BleGamepadConfiguration bleGamepadConfig;
    bleGamepadConfig.setIncludeStart(true);
    bleGamepadConfig.setTXPowerLevel(-10);  // Defaults to 9 if not set. (Range: -12 to 9 dBm)
    bleGamepadConfig.setAxesMin(-32768);
    bleGamepadConfig.setAxesMax(32767);
    bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD);
    bleGamepad.begin(&bleGamepadConfig);
    pinMode(BATT_PIN, INPUT);
}

int readBatteryPercent(){
    constexpr uint8_t NUM_BATT_SAMPLES = 1;
    constexpr float BATTERY_MIN = 3.25;
    constexpr float BATTERY_MAX = 3.9;
    
    // get battery voltage average
    uint32_t batRaw = 0;
    for(int i = 0; i < NUM_BATT_SAMPLES; i++){
        batRaw += analogReadMilliVolts(BATT_PIN);
    }
    
    batRaw /= NUM_BATT_SAMPLES;

    // * 2 to comppensate for V divider
    // * 1000 for mV -> V conversion
    float batV = (batRaw * 2.0) / 1000.0;

    // Get % between min and max
    int percent = ((batV - BATTERY_MIN) / (BATTERY_MAX - BATTERY_MIN)) * 100;

    return percent;
}

void loop() {
  

  // we don't need to do t his EVERY iteration... maybe smart to only do once in a while
  int batteryPercent = readBatteryPercent();
  // int batteryPercent = 69;
  // Serial.printf("%3d\n", batteryPercent);


  if (!bleGamepad.isConnected()) {
    Serial.printf("No connection!\n");
    delay(500);
    return;
  }
  
  uint32_t data = n64.poll();

  state = N64State(data);
  N64State dState = state - oldState;
  oldState = state;

  if(dState.a) toggleButton(BUTTON_1);
  if(dState.b) toggleButton(BUTTON_2);
  if(dState.z) toggleButton(BUTTON_3);
  if(state.start) bleGamepad.pressStart(); else bleGamepad.releaseStart();
  if(dState.l) toggleButton(BUTTON_4);
  if(dState.r) toggleButton(BUTTON_5);
  if(dState.du) toggleButton(BUTTON_6);
  if(dState.dd) toggleButton(BUTTON_7);
  if(dState.dl) toggleButton(BUTTON_8);
  if(dState.dr) toggleButton(BUTTON_9);
  if(dState.cu) toggleButton(BUTTON_10);
  if(dState.cd) toggleButton(BUTTON_11);
  if(dState.cl) toggleButton(BUTTON_14);
  if(dState.cr) toggleButton(BUTTON_13);

  // // Map analog stick to X/Y axes (range -128..127 → -32767..32767)
  int16_t xAxis = (int16_t)state.x * 385.0;  // simple scaling
  int16_t yAxis = -(int16_t)state.y * 385.0;

  bleGamepad.setLeftThumb(xAxis, yAxis);
  bleGamepad.setBatteryLevel(batteryPercent);

  // Send HID report
  bleGamepad.sendReport();

  delay(5); // poll interval
}
