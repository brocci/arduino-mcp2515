#include <SPI.h>
#include <mcp2515.h>

struct can_frame txMsg;
struct can_frame rxMsg;
MCP2515 mcp2515(10);

void setup() {
    Serial.begin(115200);

    mcp2515.reset();
    mcp2515.setBitrate(CAN_125KBPS);
    mcp2515.setOperatingMode(MCP2515::CAN_MODE_LOOPBACK);

    txMsg.can_id  = 0x123;
    txMsg.can_dlc = 4;
    txMsg.data[0] = 0xDE;
    txMsg.data[1] = 0xAD;
    txMsg.data[2] = 0xBE;
    txMsg.data[3] = 0xEF;

    Serial.println("------- CAN Loopback Test ----------");
}

void loop() {
    delay(1000);

    Serial.print("Sending 0x");
    Serial.print(txMsg.can_id, HEX);
    Serial.println("...");

    if (mcp2515.sendMessage(&txMsg) != MCP2515::ERROR_OK) {
        Serial.println("Send failed");
        return;
    }

    delay(10);

    if (mcp2515.readMessage(&rxMsg) == MCP2515::ERROR_OK) {
        Serial.print("Received ID: 0x");
        Serial.println(rxMsg.can_id, HEX);
    } else {
        Serial.println("No message received");
    }
}
