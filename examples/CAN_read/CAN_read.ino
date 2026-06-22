// CAN_read — receive and display CAN frames
// Demonstrates: readMessage(), frame inspection, polling pattern

#include <SPI.h>
#include <mcp2515.h>

struct can_frame canMsg;
MCP2515 mcp2515(10);

void setup() {
    Serial.begin(115200);

    mcp2515.reset();
    mcp2515.setBitrate(CAN_125KBPS);
    mcp2515.setOperatingMode(MCP2515::CAN_MODE_NORMAL);

    Serial.println("------- CAN Read ----------");
    Serial.println("ID  DLC   DATA");
}

void loop() {
    // readMessage returns ERROR_OK when a frame is available.
    // In polled mode, call it every loop iteration — it returns
    // ERROR_NOMSG when no data is pending.
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
        Serial.print(canMsg.can_id, HEX);
        Serial.print(" ");
        Serial.print(canMsg.can_dlc, HEX);
        Serial.print(" ");

        for (int i = 0; i < canMsg.can_dlc; i++) {
            Serial.print(canMsg.data[i], HEX);
            Serial.print(" ");
        }

        Serial.println();
    }
}
