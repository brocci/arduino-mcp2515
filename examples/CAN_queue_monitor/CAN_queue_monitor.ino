// CAN_queue_monitor — detect and report RX/TX queue saturation
// Demonstrates: getTxQueueDepth(), getRxQueueDepth(),
//   getRxQueueDropCount(), getRxHardwareOverflowCount()
//
// The library uses software queues to absorb short bursts of traffic.
// If the application cannot consume frames fast enough, the RX queue
// fills and frames are dropped. These counters help you detect that.
//
// Rising drop/overflow counts indicate the system is being overloaded.
// Possible mitigations: increase queue sizes (compile-time defines),
// process frames faster, or use interrupt-driven reception.

#include <SPI.h>
#include <mcp2515.h>

struct can_frame canMsg;
MCP2515 mcp2515(10);

void setup() {
    Serial.begin(115200);

    mcp2515.reset();
    mcp2515.setBitrate(CAN_125KBPS);
    mcp2515.setOperatingMode(MCP2515::CAN_MODE_NORMAL);

    Serial.println("------- CAN Queue Monitor ----------");
    Serial.println("Reporting queue depth and drops every 5 seconds");
}

void loop() {
    // Keep the RX queue drained
    mcp2515.readMessage(&canMsg);

    // Report every 5 seconds
    static unsigned long lastReport = 0;
    if ((millis() - lastReport) > 5000) {
        lastReport = millis();

        uint8_t txDepth  = mcp2515.getTxQueueDepth();
        uint8_t rxDepth  = mcp2515.getRxQueueDepth();
        uint16_t drops   = mcp2515.getRxQueueDropCount();
        uint16_t hwOvf   = mcp2515.getRxHardwareOverflowCount();

        Serial.print("TX queue: ");
        Serial.print(txDepth);
        Serial.print("  RX queue: ");
        Serial.print(rxDepth);
        Serial.print("  SW drops: ");
        Serial.print(drops);
        Serial.print("  HW overflows: ");
        Serial.println(hwOvf);

        // Non-zero drops or overflows indicate message loss
        if (drops > 0 || hwOvf > 0) {
            Serial.println("WARNING: frame loss detected");
        }
    }
}
