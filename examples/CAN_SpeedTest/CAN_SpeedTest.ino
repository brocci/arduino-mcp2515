// CAN_SpeedTest — measure CAN bus throughput
// Demonstrates: readMessage() throughput tracking, millis() timing

#include <SPI.h>
#include <mcp2515.h>

struct can_frame canMsg;
MCP2515 mcp2515(10);
int cntr = 0;
unsigned long oldTime = 0;

void setup() {
    Serial.begin(115200);

    mcp2515.reset();
    mcp2515.setBitrate(CAN_125KBPS);
    mcp2515.setOperatingMode(MCP2515::CAN_MODE_NORMAL);

    Serial.println("------- CAN Speedtest ----------");
}

void loop() {
    // Count every successfully received frame
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
        cntr++;
    }

    // Report messages per second
    if ((millis() - oldTime) > 1000) {
        oldTime = millis();
        Serial.print(cntr);
        Serial.println(" msg/sec");
        cntr = 0;
    }
}
