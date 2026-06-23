// CAN_error_handling — inspect and clear MCP2515 error states
// Demonstrates: checkError(), getErrorFlags(), clearRXnOVR()
//
// The MCP2515 tracks error conditions in its EFLG register.
// checkError() returns true if any error flag is set.
// getErrorFlags() returns the raw register value for individual flag tests.
// clearRXnOVR() clears overflow flags and all interrupt flags.
// Individual clears (clearMERR(), clearERRIF()) are also available.

#include <SPI.h>
#include <mcp2515.h>

struct can_frame canMsg;
MCP2515 mcp2515(10);

void setup() {
    Serial.begin(115200);

    mcp2515.reset();
    mcp2515.setBitrate(CAN_125KBPS);
    mcp2515.setOperatingMode(MCP2515::CAN_MODE_NORMAL);

    Serial.println("------- CAN Error Handling ----------");
}

void loop() {
    // Drain any pending messages
    mcp2515.readMessage(&canMsg);

    // checkError() is a quick way to know if any flag is raised
    if (mcp2515.checkError()) {
        uint8_t eflg = mcp2515.getErrorFlags();
        Serial.print("Error flags: 0x");
        Serial.println(eflg, HEX);

        // Test individual flags to identify the condition
        if (eflg & MCP2515::EFLG_RX0OVR || eflg & MCP2515::EFLG_RX1OVR) {
            Serial.println("  RX buffer overflow");
        }
        if (eflg & MCP2515::EFLG_TXBO) {
            Serial.println("  Bus-off");
        }
        if (eflg & MCP2515::EFLG_TXEP || eflg & MCP2515::EFLG_RXEP) {
            Serial.println("  Error-passive");
        }
        if (eflg & MCP2515::EFLG_TXWAR || eflg & MCP2515::EFLG_RXWAR) {
            Serial.println("  Warning threshold reached");
        }

        // Clear overflow flags and interrupt flags
        mcp2515.clearRXnOVR();
        Serial.println("Errors cleared");
    }

    delay(5000);
}
