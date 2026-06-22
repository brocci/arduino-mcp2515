// CAN_interrupt — interrupt-driven CAN receive
// Demonstrates: enableInterrupt(), handleInterrupt(), interrupt-based message reception
//
// Wiring: connect MCP2515 INT pin to Arduino pin 2 (or any interrupt-capable pin).
//   Interrupt-driven receive reduces CPU usage compared to polling.
//
// The library handles the ISR internally. Your callback only calls
// mcp2515.handleInterrupt(), which reads and clears interrupt flags
// in a single SPI transaction. Frames are buffered in the software RX
// queue and retrieved by readMessage() in loop().

#include <SPI.h>
#include <mcp2515.h>

struct can_frame canMsg;
MCP2515 mcp2515(10);

// ISR callback — must be kept short. The library does the heavy lifting.
void canISR(void) {
    mcp2515.handleInterrupt();
}

void setup() {
    Serial.begin(115200);

    mcp2515.reset();
    mcp2515.setBitrate(CAN_125KBPS);
    mcp2515.setOperatingMode(MCP2515::CAN_MODE_NORMAL);

    // Attach the INT pin (pin 2) to the ISR callback.
    // This configures attachInterrupt() internally with FALLING edge.
    mcp2515.enableInterrupt(2, canISR);

    Serial.println("------- CAN Interrupt ----------");
}

void loop() {
    // readMessage() drains the internal RX queue, which was populated
    // by handleInterrupt(). No explicit polling is needed.
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
        Serial.print("ID: 0x");
        Serial.print(canMsg.can_id, HEX);
        Serial.print("  DLC: ");
        Serial.println(canMsg.can_dlc);
    }
}
