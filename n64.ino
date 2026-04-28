// #include <BleGamepad.h>

// BleGamepad gamepad("MyCustomController", "MyCompany", 100);

// void setup() {
//   gamepad.begin();
// }

// void loop() {
//   if (gamepad.isConnected()) {
//     gamepad.press(BUTTON_1);
//     delay(200);
//     gamepad.release(BUTTON_1);
//     delay(200);
//   }
// }



// inline void writeHigh() {
//   writeReg(GPIO_OUT_W1TS_REG, PIN_BIT);
// }

// inline void writeLow() {
//   writeReg(GPIO_OUT_W1TC_REG, PIN_BIT);
// }


// void setOutputMode() {
//   // Disable pull resistors
//   uint32_t cfg = (2);  // FUNC=2, no PU/PD
//   writeReg(IO_MUX_GPIO10_REG, cfg);

//   // Enable output driver
//   writeReg(GPIO_ENABLE_W1TS_REG, PIN_BIT);
// }


// #define IO_MUX_GPIO10_REG   ((volatile &uint32_t)(IO_MUX_BASE + 0x34))
#define PIN 10
#define IO_MUX_FUN_WPD (1 << 9)
#define PIN_BIT (1 << PIN)

inline void writeReg(uint32_t addr, uint32_t val) {
  *((volatile uint32_t*)addr) = val;
}

void setInputPulldown() {
  // Disable output driver
  writeReg(GPIO_ENABLE_W1TC_REG, PIN_BIT);

  // Configure IO_MUX:
  // - function = 2 (GPIO)
  // - pull-down enable (bit 9)
  // - pull-up disable (bit 8)
  uint32_t cfg = (2) | (1 << 9);  // FUNC=2, PD=1, PU=0
  writeReg(IO_MUX_GPIO10_REG, cfg);
}

void setOutputMode() {
  // Disable pull resistors
  uint32_t cfg = (2);  // FUNC=2, no PU/PD
  writeReg(IO_MUX_GPIO10_REG, cfg);

  // Enable output driver
  writeReg(GPIO_ENABLE_W1TS_REG, PIN_BIT);
}


void setup() {
  for(int i = 0; i < 3; i++){
    setOutputMode();
    delay(125);
    setInputPulldown();
    delay(125);
  }
}

void loop() {
  // Enable output driver
  writeReg(GPIO_ENABLE_W1TS_REG, PIN_BIT);
  delay(1);
  // disable output driver
  writeReg(GPIO_ENABLE_W1TC_REG, PIN_BIT);
  delay(1);
}
