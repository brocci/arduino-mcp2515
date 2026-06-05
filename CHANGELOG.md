# Changelog

All notable changes to this project are documented in this file.

## [2.0.0] - 2026-06-05

### Added
- Software TX/RX circular queues to eliminate message loss under high bus traffic
  - `processTxQueue()` to drain queued TX frames in main loop
  - `drainRxBuffers()` to pull all available RX messages into queue
  - `getTxQueueDepth()` and `getRxQueueDepth()` for queue occupancy monitoring
- Improved interrupt handling with single SPI transaction (no nested begin/end)
- ESP8266 IRAM optimization for ISR context (`IRAM_ATTR` on handlers)
- `setBitTiming(cnf1, cnf2, cnf3)` method for direct CNF register control
- Support for 7 new baud rate and crystal combinations:
  - 8 MHz: 83.3 kbps, 95 kbps
  - 16 MHz: 31.25 kbps
  - 20 MHz: 10 kbps, 20 kbps, 31.25 kbps, 95 kbps

### Changed
- Refactored `setBitrate()` from 290-line nested switch to compact lookup tables
- Bumped library version to 2.0.0
- Maintainer changed to brocci/arduino-mcp2515 fork

### Fixed
- 14 incorrect bit timing values from original autowp tables (sample points now 62.5--68.75%)
- All bit timing values recomputed using ACAN2515 algorithm for consistency
- Marked 20 MHz/5 kbps as unsupported (BRP would exceed MCP2515 max of 64)

### Notes
- Public member names follow ALL_CAPS with underscores convention:
  - `SPI_CS` (chip select pin)
  - `SPI_CLOCK` (SPI bus speed)
  - `SPI_PORT` (SPIClass pointer)

---

## [1.3.1] - Upstream Reference

Previous stable version from [autowp/arduino-mcp2515](https://github.com/autowp/arduino-mcp2515).
See upstream repository for full history.
