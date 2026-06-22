// CAN_write — transmit frames on the CAN bus
// Demonstrates: frame construction, sendMessage(), reset(), setBitrate(), setOperatingMode()

#include <SPI.h>
#include <mcp2515.h>

struct can_frame canMsg1;
struct can_frame canMsg2;

// CS pin is typically 10 on Uno/Nano, check your shield wiring
MCP2515 mcp2515(10);

void setup() {
    // Prepare two CAN frames with different IDs and payloads
    canMsg1.can_id  = 0x0F6;
    canMsg1.can_dlc = 8;
    canMsg1.data[0] = 0x8E;
    canMsg1.data[1] = 0x87;
    canMsg1.data[2] = 0x32;
    canMsg1.data[3] = 0xFA;
    canMsg1.data[4] = 0x26;
    canMsg1.data[5] = 0x8E;
    canMsg1.data[6] = 0xBE;
    canMsg1.data[7] = 0x86;

    canMsg2.can_id  = 0x036;
    canMsg2.can_dlc = 8;
    canMsg2.data[0] = 0x0E;
    canMsg2.data[1] = 0x00;
    canMsg2.data[2] = 0x00;
    canMsg2.data[3] = 0x08;
    canMsg2.data[4] = 0x01;
    canMsg2.data[5] = 0x00;
    canMsg2.data[6] = 0x00;
    canMsg2.data[7] = 0xA0;

    while (!Serial);
    Serial.begin(115200);

    // Reset the MCP2515 to a known state, set bitrate, then enter normal mode
    mcp2515.reset();
    mcp2515.setBitrate(CAN_125KBPS);
    mcp2515.setOperatingMode(MCP2515::CAN_MODE_NORMAL);

    Serial.println("Example: Write to CAN");
}

void loop() {
    // sendMessage() queues the frame if hardware buffers are busy
    mcp2515.sendMessage(&canMsg1);
    mcp2515.sendMessage(&canMsg2);

    Serial.println("Messages sent");

    delay(100);
}
