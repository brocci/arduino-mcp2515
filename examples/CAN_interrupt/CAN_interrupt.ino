#include <SPI.h>
#include <mcp2515.h>

struct can_frame canMsg;
MCP2515 mcp2515(10);

void canISR(void) {
    mcp2515.handleInterrupt();
}

void setup() {
    Serial.begin(115200);

    mcp2515.reset();
    mcp2515.setBitrate(CAN_125KBPS);
    mcp2515.setOperatingMode(MCP2515::CAN_MODE_NORMAL);
    mcp2515.enableInterrupt(2, canISR);

    Serial.println("------- CAN Interrupt ----------");
}

void loop() {
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
        Serial.print("ID: 0x");
        Serial.print(canMsg.can_id, HEX);
        Serial.print("  DLC: ");
        Serial.println(canMsg.can_dlc);
    }
}
