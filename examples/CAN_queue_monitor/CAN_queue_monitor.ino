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
}

void loop() {
    mcp2515.readMessage(&canMsg);

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
    }
}
