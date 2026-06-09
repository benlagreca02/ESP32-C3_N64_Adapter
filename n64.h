#pragma once

#include <Arduino.h>
#include "driver/rmt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

// To represent a controller's state
class N64State {
    static constexpr uint32_t A_MASK = 1 << 31;
    static constexpr uint32_t B_MASK = 1 << 30;
    static constexpr uint32_t Z_MASK = 1 << 29;
    static constexpr uint32_t START_MASK = 1 << 28;
    static constexpr uint32_t DPAD_UP_MASK = 1 << 27;
    static constexpr uint32_t DPAD_DOWN_MASK = 1 << 26;
    static constexpr uint32_t DPAD_LEFT_MASK = 1 << 25;
    static constexpr uint32_t DPAD_RIGHT_MASK = 1 << 24;
public:
    bool a;
    bool b;
    bool z;
    bool start;
    bool l;
    bool r;
    N64State(uint32_t packed):
            a(packed & A_MASK),
            b(packed & B_MASK),
            z(packed & Z_MASK),
            start(packed & START_MASK)
    {}
};


// To represent the controller
class N64Controller {
private:
    RingbufHandle_t rb = NULL;

    static const int RMT_CLK_DIV = 80;  // 80Mhz clock / 80 clk div => 1us
    static const int REQUEST_LENGTH = 9;
    static const int NUM_RESPONSE_BITS = 32;
    static constexpr uint8_t reqBits[REQUEST_LENGTH] = {0,0,0,0,0,0,0,1,1};

    // Technically the same every time. Could be static
    // Probably should be static
    rmt_item32_t request[REQUEST_LENGTH];

    rmt_channel_t txChannel;
    rmt_channel_t rxChannel;

    void constructRequest(){
        // Build RMT items for request pattern: bits 000000011 (LSB first for N64)
        // Each bit encoded as low then high durations (in ticks = microseconds)
        // 0 -> 3us low, 1us high ; 1 -> 1us low, 3us high
        // We'll send the 9 bits: 0,0,0,0,0,0,0,1,1 (MSB->LSB depends on your ordering)
        for (int i=0;i<REQUEST_LENGTH;i++){
            if (reqBits[i]==0){
                request[i].level0 = 0; request[i].duration0 = 3; // 3us low
                request[i].level1 = 1; request[i].duration1 = 1; // 1us high
            } else {
                request[i].level0 = 0; request[i].duration0 = 1; // 1us low
                request[i].level1 = 1; request[i].duration1 = 3; // 3us high
            }
        }  
    }

public:
    // Constructor initalizes
    N64Controller(
            gpio_num_t txPin, gpio_num_t rxPin,
            rmt_channel_t txChannel, rmt_channel_t rxChannel) : 
            txChannel(txChannel),
            rxChannel(rxChannel)
        {

        rmt_config_t tx_cfg = {};
        tx_cfg.rmt_mode = RMT_MODE_TX;
        tx_cfg.channel = txChannel;
        tx_cfg.gpio_num = txPin;
        tx_cfg.mem_block_num = 1;
        tx_cfg.clk_div = RMT_CLK_DIV;
        tx_cfg.tx_config.loop_en = false;
        tx_cfg.tx_config.carrier_en = false;
        tx_cfg.tx_config.idle_level = RMT_IDLE_LEVEL_HIGH; // release = high
        rmt_config(&tx_cfg);
        rmt_driver_install(tx_cfg.channel, 0, 0);

        // RX config
        rmt_config_t rx_cfg = {};
        rx_cfg.rmt_mode = RMT_MODE_RX;
        rx_cfg.channel = rxChannel;
        rx_cfg.gpio_num = rxPin;
        rx_cfg.clk_div = RMT_CLK_DIV/2;  // Recv 2x faster than we need to
        rx_cfg.mem_block_num = 1;
        rx_cfg.rx_config.filter_en = false;
        // May not need these lines
        rx_cfg.rx_config.filter_ticks_thresh = 2; // filter tiny spikes
        rx_cfg.rx_config.idle_threshold = 1000; // long idle to end

        rmt_config(&rx_cfg);
        rmt_driver_install(rx_cfg.channel, 1024, 0);
        rmt_get_ringbuf_handle(rx_cfg.channel, &rb);

        constructRequest();
    }

    uint32_t poll(){
        // Start RX (we receive the inital 9 bits we transmit, BUT we shift them out later)
        rmt_rx_start(rxChannel, true);

        // Transmit request
        rmt_write_items(txChannel, request, REQUEST_LENGTH, true); // wait_tx_done = true

        size_t rx_size;

        // wait up to 1ms for reply. Should never time out, we will at least
        // recv the TX'd reqest
        void* chunk = xRingbufferReceive(rb, &rx_size, pdMS_TO_TICKS(1));

        if (!chunk) {
            // BAD!
            Serial.println("No reply (timeout)");
            return 0;
        }

        rmt_item32_t* rx_items = (rmt_item32_t*)chunk;
        int item_count = rx_size / sizeof(rmt_item32_t);

        // Decode low/high pairs into bits
        // Each item has duration0 (ticks) for level0 then duration1 for level1
        // We classify by low duration: >=half -> 3us low => bit 0; <=half -> 1us low => bit 1
        uint32_t packedBits = 0;
        int numBitsDecoded = 0;

        for (int i=0; i<item_count && numBitsDecoded < REQUEST_LENGTH + NUM_RESPONSE_BITS ;i++){
            /*
            // Skip the first few bits, we don't care about the request
            if (numBitsDecoded < REQUEST_LENGTH){
                numBitsDecoded++;
                continue;
            }
            */

            // ensure the low level is 0 (controller drives low first)
            if (rx_items[i].level0 == 0) {
                uint32_t low = rx_items[i].duration0;
                uint32_t high = rx_items[i].duration1;
                // threshold at 2 ticks (2us)
                int bit = (low <= (low+high)/2) ? 1 : 0;
                packedBits = (packedBits << 1) | (bit & 1);
                numBitsDecoded++;
            }
        }
        
        vRingbufferReturnItem(rb, chunk);
        rmt_rx_stop(rxChannel);
        return packedBits;
    }
};
