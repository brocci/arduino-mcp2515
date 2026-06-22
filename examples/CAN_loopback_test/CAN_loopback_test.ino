// CAN_loopback_test — self-test without connecting to a CAN bus
// Demonstrates: CAN_MODE_LOOPBACK, send then read back
//
// In loopback mode, transmitted frames are immediately received by the
// same controller. Nothing appears on the bus. This is useful for
// testing the MCP2515 and your wiring without a transceiver or bus.

#include <SPI.h>
#include <mcp2515.h>

struct can_frame txMsg;
struct can_frame rxMsg;
MCP2515 mcp2515(10);

void setup() {
    Serial.begin(115200);

    // Configure for loopback — no external bus needed
    mcp2515.reset();
    mcp2515.setBitrate(CAN_125KBPS);
    mcp2515.setOperatingMode(MCP2515::CAN_MODE_LOOPBACK);

    // Prepare a test frame
    txMsg.can_id  = 0x123;
    txMsg.can_dlc = 4;
    txMsg.data[0] = 0xDE;
    txMsg.data[1] = 0xAD;
    txMsg.data[2] = 0xBE;
    txMsg.data[3] = 0xEF;

    Serial.println("------- CAN Loopback Test ----------");
    Serial.println("Sending and receiving on the same controller");
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

    // Short delay allows the MCP2515 to process the loopback
    delay(10);

    // Read back the same frame from the receive path
    if (mcp2515.readMessage(&rxMsg) == MCP2515::ERROR_OK) {
        Serial.print("Received ID: 0x");
        Serial.println(rxMsg.can_id, HEX);
    } else {
        Serial.println("No message received");
    }
}
