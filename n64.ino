// Requires Arduino-ESP32 (includes esp-idf headers)
#include <Arduino.h>
#include "driver/rmt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

const gpio_num_t IO_PIN = GPIO_NUM_10; // change to your pin
const rmt_channel_t TX_CH = RMT_CHANNEL_0;
const rmt_channel_t RX_CH = RMT_CHANNEL_1;
const int RMT_CLK_DIV = 80; // APB 80MHz / 80 = 1MHz -> 1 tick = 1 us
RingbufHandle_t rb = NULL;

void setup() {
  Serial.begin(115200);
  delay(100);

  gpio_set_direction(IO_PIN, GPIO_MODE_INPUT_OUTPUT_OD);
  gpio_set_pull_mode(IO_PIN, GPIO_FLOATING);

  // TX config
  rmt_config_t tx_cfg = {};
  tx_cfg.rmt_mode = RMT_MODE_TX;
  tx_cfg.channel = TX_CH;
  tx_cfg.gpio_num = IO_PIN;
  tx_cfg.mem_block_num = 1;
  tx_cfg.clk_div = RMT_CLK_DIV;
  tx_cfg.tx_config.loop_en = false;
  tx_cfg.tx_config.carrier_en = false;
  tx_cfg.tx_config.idle_level = RMT_IDLE_LEVEL_HIGH; // release = high
  rmt_config(&tx_cfg);
  rmt_driver_install(tx_cfg.channel, 0, 0);

  gpio_set_direction(IO_PIN, GPIO_MODE_INPUT_OUTPUT_OD);
  gpio_set_pull_mode(IO_PIN, GPIO_FLOATING);


  // RX config
  rmt_config_t rx_cfg = {};
  rx_cfg.rmt_mode = RMT_MODE_RX;
  rx_cfg.channel = RX_CH;
  rx_cfg.gpio_num = IO_PIN;
  rx_cfg.clk_div = RMT_CLK_DIV;
  rx_cfg.mem_block_num = 1;
  rx_cfg.rx_config.filter_en = false;
  rx_cfg.rx_config.filter_ticks_thresh = 0; // filter tiny spikes
  rx_cfg.rx_config.idle_threshold = 10000; // long idle to end
  
  rmt_config(&rx_cfg);
  rmt_driver_install(rx_cfg.channel, 1000, 0);
  rmt_get_ringbuf_handle(rx_cfg.channel, &rb);


  gpio_set_direction(IO_PIN, GPIO_MODE_INPUT_OUTPUT_OD);
  gpio_set_pull_mode(IO_PIN, GPIO_FLOATING);

}

void loop() {
  // Build RMT items for request pattern: bits 000000011 (LSB first for N64)
  // Each bit encoded as low then high durations (in ticks = microseconds)
  // 0 -> 3us low, 1us high ; 1 -> 1us low, 3us high
  // We'll send the 9 bits: 0,0,0,0,0,0,0,1,1 (MSB->LSB depends on your ordering)
  const uint8_t reqBits[9] = {0,0,0,0,0,0,0,1,1};
  const int nItems = 9;
  rmt_item32_t items[nItems];
  for (int i=0;i<nItems;i++){
    if (reqBits[i]==0){
      items[i].level0 = 0; items[i].duration0 = 3; // 3us low
      items[i].level1 = 1; items[i].duration1 = 1; // 1us high
    } else {
      items[i].level0 = 0; items[i].duration0 = 1; // 1us low
      items[i].level1 = 1; items[i].duration1 = 3; // 3us high
    }
  }

  // Transmit request
  rmt_write_items(TX_CH, items, nItems, true); // wait_tx_done = true

  rmt_rx_start(RX_CH, true);
  // Start RX and wait for reply
  rmt_rx_start(RX_CH, true);
  size_t rx_size;
  // wait up to 1ms for reply
  void* chunk = xRingbufferReceive(rb, &rx_size, pdMS_TO_TICKS(5));
  if (chunk) {
    rmt_item32_t* rx_items = (rmt_item32_t*)chunk;
    int item_count = rx_size / sizeof(rmt_item32_t);
    // Decode low/high pairs into bits
    // Each item has duration0 (ticks) for level0 then duration1 for level1
    // We classify by low duration: >=2 -> 3us low => bit 0; <=2 -> 1us low => bit 1
    uint32_t value = 0;
    int bitsDecoded = 0;
    for (int i=0;i<item_count && bitsDecoded<32;i++){
      // ensure the low level is 0 (controller drives low first)
      if (rx_items[i].level0 == 0) {
        uint32_t low = rx_items[i].duration0;
        uint32_t high = rx_items[i].duration1;
        // threshold at 2 ticks (2us)
        int bit = (low <= 2) ? 1 : 0;
        value = (value << 1) | (bit & 1);
        bitsDecoded++;
      }
    }
    Serial.printf("Got %d bits: 0x%08X\n", bitsDecoded, value);
    vRingbufferReturnItem(rb, chunk);
  } else {
    Serial.println("No reply (timeout)");
  }

  rmt_rx_stop(RX_CH);
  delay(1); // poll interval
}
