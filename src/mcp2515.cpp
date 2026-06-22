#include "Arduino.h"
#include "mcp2515.h"

const struct MCP2515::TXBn_REGS MCP2515::TXB[MCP2515::N_TXBUFFERS] = {
    {MCP_TXB0CTRL, MCP_TXB0SIDH, MCP_TXB0DATA},
    {MCP_TXB1CTRL, MCP_TXB1SIDH, MCP_TXB1DATA},
    {MCP_TXB2CTRL, MCP_TXB2SIDH, MCP_TXB2DATA}
};

const struct MCP2515::RXBn_REGS MCP2515::RXB[N_RXBUFFERS] = {
    {MCP_RXB0CTRL, MCP_RXB0SIDH, MCP_RXB0DATA, CANINTF_RX0IF},
    {MCP_RXB1CTRL, MCP_RXB1SIDH, MCP_RXB1DATA, CANINTF_RX1IF}
};

MCP2515::MCP2515(const uint8_t _CS, const uint32_t _SPI_CLOCK, SPIClass * _SPI)
{
    if (_SPI != nullptr) {
        SPI_PORT = _SPI;
    }
    else {
        SPI_PORT = &SPI;
    }

    SPI_CS = _CS;
    SPI_CLOCK = _SPI_CLOCK;

    _rxInterruptPending = false;
    _txInterruptPending = false;
}

void MCP2515::begin() {
    pinMode(SPI_CS, OUTPUT);
    digitalWrite(SPI_CS, HIGH);
    SPI_PORT->begin();
}

void MCP2515_ISR_ATTR MCP2515::startSPI() const {
    SPI_PORT->beginTransaction(SPISettings(SPI_CLOCK, MSBFIRST, SPI_MODE0));
    digitalWrite(SPI_CS, LOW);
}

void MCP2515_ISR_ATTR MCP2515::endSPI() const {
    digitalWrite(SPI_CS, HIGH);
    SPI_PORT->endTransaction();
}

MCP2515::ERROR MCP2515::reset(void)
{
    startSPI();
    SPI_PORT->transfer(INSTRUCTION_RESET);
    endSPI();

    delay(10);

    uint8_t zeros[14];
    memset(zeros, 0, sizeof(zeros));
    setRegisters(MCP_TXB0CTRL, zeros, 14);
    setRegisters(MCP_TXB1CTRL, zeros, 14);
    setRegisters(MCP_TXB2CTRL, zeros, 14);

    setRegister(MCP_RXB0CTRL, 0);
    setRegister(MCP_RXB1CTRL, 0);

    setRegister(MCP_CANINTE, CANINTF_RX0IF | CANINTF_RX1IF | CANINTF_ERRIF | CANINTF_MERRF);

    // receives all valid messages using either Standard or Extended Identifiers that
    // meet filter criteria. RXF0 is applied for RXB0, RXF1 is applied for RXB1
    modifyRegister(MCP_RXB0CTRL,
                   RXBnCTRL_RXM_MASK | RXB0CTRL_BUKT | RXB0CTRL_FILHIT_MASK,
                   RXBnCTRL_RXM_STDEXT | RXB0CTRL_BUKT | RXB0CTRL_FILHIT);
    modifyRegister(MCP_RXB1CTRL,
                   RXBnCTRL_RXM_MASK | RXB1CTRL_FILHIT_MASK,
                   RXBnCTRL_RXM_STDEXT | RXB1CTRL_FILHIT);

    // clear filters and masks
    // do not filter any standard frames for RXF0 used by RXB0
    // do not filter any extended frames for RXF1 used by RXB1
    RXF filters[] = {RXF0, RXF1, RXF2, RXF3, RXF4, RXF5};
    for (int i=0; i<6; i++) {
        bool ext = (i == 1);
        ERROR result = setFilter(filters[i], ext, 0);
        if (result != ERROR_OK) {
            return result;
        }
    }

    MASK masks[] = {MASK0, MASK1};
    for (int i=0; i<2; i++) {
        ERROR result = setFilterMask(masks[i], true, 0);
        if (result != ERROR_OK) {
            return result;
        }
    }

    _txQueue.clear();
    _rxQueue.clear();

    return ERROR_OK;
}

void MCP2515::enableInterrupt(int intPin, void (*callback)(void)) {
    pinMode(intPin, INPUT_PULLUP);
    // ESP8266 core v3.x and ESP32 core removed usingInterrupt() from SPIClass.
    // SPI interrupt safety relies on beginTransaction/endTransaction.
#if !defined(ARDUINO_ARCH_ESP8266) && !defined(ARDUINO_ARCH_ESP32)
    SPI_PORT->usingInterrupt(digitalPinToInterrupt(intPin));
#endif
    attachInterrupt(digitalPinToInterrupt(intPin), callback, FALLING);
}

void MCP2515_ISR_ATTR MCP2515::handleInterrupt(void)
{
    startSPI();

    uint8_t canintf = readRegisterRaw(MCP_CANINTF);

    if (canintf & (CANINTF_RX0IF | CANINTF_RX1IF)) {
        modifyRegisterRaw(MCP_CANINTF, CANINTF_RX0IF | CANINTF_RX1IF, 0);
        _rxInterruptPending = true;
    }

    if (canintf & (CANINTF_TX0IF | CANINTF_TX1IF | CANINTF_TX2IF)) {
        modifyRegisterRaw(MCP_CANINTF,
            CANINTF_TX0IF | CANINTF_TX1IF | CANINTF_TX2IF, 0);
        _txInterruptPending = true;
    }

    if (canintf & (CANINTF_ERRIF | CANINTF_MERRF)) {
        uint8_t eflg = readRegisterRaw(MCP_EFLG);
        if (eflg & (EFLG_RX0OVR | EFLG_RX1OVR)) {
            modifyRegisterRaw(MCP_EFLG, eflg & (EFLG_RX0OVR | EFLG_RX1OVR), 0);
        }
        modifyRegisterRaw(MCP_CANINTF, canintf & (CANINTF_ERRIF | CANINTF_MERRF), 0);
    }

    endSPI();
}

uint8_t MCP2515::readRegister(const REGISTER reg) const
{
    startSPI();
    SPI_PORT->transfer(INSTRUCTION_READ);
    SPI_PORT->transfer(reg);
    uint8_t ret = SPI_PORT->transfer(0x00);
    endSPI();

    return ret;
}

void MCP2515::readRegisters(const REGISTER reg, uint8_t values[], const uint8_t n) const
{
    startSPI();
    SPI_PORT->transfer(INSTRUCTION_READ);
    SPI_PORT->transfer(reg);
    // mcp2515 has auto-increment of address-pointer
    for (uint8_t i=0; i<n; i++) {
        values[i] = SPI_PORT->transfer(0x00);
    }
    endSPI();
}

void MCP2515::setRegister(const REGISTER reg, const uint8_t value)
{
    startSPI();
    SPI_PORT->transfer(INSTRUCTION_WRITE);
    SPI_PORT->transfer(reg);
    SPI_PORT->transfer(value);
    endSPI();
}

void MCP2515::setRegisters(const REGISTER reg, const uint8_t values[], const uint8_t n)
{
    startSPI();
    SPI_PORT->transfer(INSTRUCTION_WRITE);
    SPI_PORT->transfer(reg);
    for (uint8_t i=0; i<n; i++) {
        SPI_PORT->transfer(values[i]);
    }
    endSPI();
}

void MCP2515::modifyRegister(const REGISTER reg, const uint8_t mask, const uint8_t data)
{
    startSPI();
    SPI_PORT->transfer(INSTRUCTION_BITMOD);
    SPI_PORT->transfer(reg);
    SPI_PORT->transfer(mask);
    SPI_PORT->transfer(data);
    endSPI();
}

uint8_t MCP2515::getStatus(void) const
{
    startSPI();
    SPI_PORT->transfer(INSTRUCTION_READ_STATUS);
    uint8_t i = SPI_PORT->transfer(0x00);
    endSPI();

    return i;
}

MCP2515::ERROR MCP2515::setConfigMode()
{
    return setMode(CANCTRL_REQOP_CONFIG);
}

MCP2515::ERROR MCP2515::setListenOnlyMode()
{
    return setMode(CANCTRL_REQOP_LISTENONLY);
}

MCP2515::ERROR MCP2515::setSleepMode()
{
    return setMode(CANCTRL_REQOP_SLEEP);
}

MCP2515::ERROR MCP2515::setLoopbackMode()
{
    return setMode(CANCTRL_REQOP_LOOPBACK);
}

MCP2515::ERROR MCP2515::setNormalMode()
{
    return setMode(CANCTRL_REQOP_NORMAL);
}

MCP2515::ERROR MCP2515::setNormalOneShotMode()
{
    return setMode(CANCTRL_REQOP_OSM);
}

MCP2515::ERROR MCP2515::setMode(const CANCTRL_REQOP_MODE mode)
{
    modifyRegister(MCP_CANCTRL, CANCTRL_REQOP | CANCTRL_OSM, mode);

    unsigned long endTime = millis() + 10;
    bool modeMatch = false;
    while (millis() < endTime) {
        uint8_t newmode = readRegister(MCP_CANSTAT);
        newmode &= CANSTAT_OPMOD;

        modeMatch = newmode == mode;

        if (modeMatch) {
            break;
        }
    }

    return modeMatch ? ERROR_OK : ERROR_FAIL;

}

MCP2515::ERROR MCP2515::setBitrate(const CAN_SPEED canSpeed)
{
    return setBitrate(canSpeed, MCP_16MHZ);
}

// ---------------------------------------------------------------------------
// Bit timing lookup tables - ACAN2515 algorithm, corrected values
// CNF1 = ((SJW-1)<<6)|(BRP-1)
// CNF2 = 0x80|(SAM<<6)|((PS1-1)<<3)|(PropSeg-1)  [BTLMODE always set]
// CNF3 = (PS2-1)
// Unsupported entries: {0x00, 0x00, 0x00}  (CNF3=0 means PS2=1 which violates
//   the MCP2515 datasheet minimum PS2>=2, so this sentinel is unambiguous)
// ---------------------------------------------------------------------------

struct BitTimingEntry { uint8_t speed, cfg1, cfg2, cfg3; };

static const BitTimingEntry BIT_TIMING_8MHz[16] = {
    {CAN_5KBPS,    0xDF, 0xFF, 0x07},  // BRP=32 TQ=25 sample=68.0%
    {CAN_10KBPS,   0xCF, 0xFF, 0x07},  // BRP=16 TQ=25 sample=68.0%
    {CAN_20KBPS,   0xC7, 0xFF, 0x07},  // BRP=8  TQ=25 sample=68.0%
    {CAN_31K25BPS, 0xC7, 0xE4, 0x04},  // BRP=8  TQ=16 sample=68.75%
    {CAN_33KBPS,   0xC4, 0xF7, 0x07},  // BRP=5  TQ=24 ~33333bps sample=66.7%
    {CAN_40KBPS,   0xC3, 0xFF, 0x07},  // BRP=4  TQ=25 sample=68.0%
    {CAN_50KBPS,   0xC3, 0xED, 0x06},  // BRP=4  TQ=20 sample=65.0%
    {CAN_80KBPS,   0xC1, 0xFF, 0x07},  // BRP=2  TQ=25 sample=68.0%
    {CAN_83K3BPS,  0xC1, 0xF7, 0x07},  // BRP=2  TQ=24 ~83333bps sample=66.7%
    {CAN_95KBPS,   0xC1, 0xEE, 0x06},  // BRP=2  TQ=21 ~95238bps sample=66.7%
    {CAN_100KBPS,  0xC1, 0xED, 0x06},  // BRP=2  TQ=20 sample=65.0%
    {CAN_125KBPS,  0xC1, 0xE4, 0x04},  // BRP=2  TQ=16 sample=68.75% [CBUS]
    {CAN_200KBPS,  0xC0, 0xAD, 0x06},  // BRP=1  TQ=20 sample=65.0%
    {CAN_250KBPS,  0xC0, 0xA4, 0x04},  // BRP=1  TQ=16 sample=68.75%
    {CAN_500KBPS,  0x40, 0x89, 0x02},  // BRP=1  TQ=8  sample=62.5%
    {CAN_1000KBPS, 0x00, 0x00, 0x00},  // unsupported (PS2<2 at any valid BRP)
};

static const BitTimingEntry BIT_TIMING_16MHz[16] = {
    {CAN_5KBPS,    0xFF, 0xFF, 0x07},  // BRP=64 TQ=25 sample=68.0%
    {CAN_10KBPS,   0xDF, 0xFF, 0x07},  // BRP=32 TQ=25 sample=68.0%
    {CAN_20KBPS,   0xCF, 0xFF, 0x07},  // BRP=16 TQ=25 sample=68.0%
    {CAN_31K25BPS, 0xCF, 0xE4, 0x04},  // BRP=16 TQ=16 sample=68.75%
    {CAN_33KBPS,   0xC9, 0xF7, 0x07},  // BRP=10 TQ=24 ~33333bps sample=66.7%
    {CAN_40KBPS,   0xC7, 0xFF, 0x07},  // BRP=8  TQ=25 sample=68.0%
    {CAN_50KBPS,   0xC7, 0xED, 0x06},  // BRP=8  TQ=20 sample=65.0%
    {CAN_80KBPS,   0xC3, 0xFF, 0x07},  // BRP=4  TQ=25 sample=68.0%
    {CAN_83K3BPS,  0xC3, 0xF7, 0x07},  // BRP=4  TQ=24 ~83333bps sample=66.7%
    {CAN_95KBPS,   0xC3, 0xEE, 0x06},  // BRP=4  TQ=21 ~95238bps sample=66.7%
    {CAN_100KBPS,  0xC3, 0xED, 0x06},  // BRP=4  TQ=20 sample=65.0%
    {CAN_125KBPS,  0xC3, 0xE4, 0x04},  // BRP=4  TQ=16 sample=68.75% [CBUS]
    {CAN_200KBPS,  0xC1, 0xAD, 0x06},  // BRP=2  TQ=20 sample=65.0%
    {CAN_250KBPS,  0xC1, 0xA4, 0x04},  // BRP=2  TQ=16 sample=68.75%
    {CAN_500KBPS,  0xC0, 0xA4, 0x04},  // BRP=1  TQ=16 sample=68.75%
    {CAN_1000KBPS, 0x40, 0x89, 0x02},  // BRP=1  TQ=8  sample=62.5%
};

static const BitTimingEntry BIT_TIMING_20MHz[16] = {
    {CAN_5KBPS,    0x00, 0x00, 0x00},  // unsupported (needs BRP=80 > max 64)
    {CAN_10KBPS,   0xE7, 0xFF, 0x07},  // BRP=40 TQ=25 sample=68.0%
    {CAN_20KBPS,   0xCF, 0xFF, 0x07},  // BRP=20 TQ=25 sample=68.0%
    {CAN_31K25BPS, 0xCF, 0xE4, 0x04},  // BRP=20 TQ=16 sample=68.75%
    {CAN_33KBPS,   0xCB, 0xFF, 0x07},  // BRP=12 TQ=25 ~33333bps sample=68.0%
    {CAN_40KBPS,   0xC9, 0xFF, 0x07},  // BRP=10 TQ=25 sample=68.0%
    {CAN_50KBPS,   0xC7, 0xFF, 0x07},  // BRP=8  TQ=25 sample=68.0%
    {CAN_80KBPS,   0xC4, 0xFF, 0x07},  // BRP=5  TQ=25 sample=68.0%
    {CAN_83K3BPS,  0xC4, 0xF7, 0x07},  // BRP=5  TQ=24 ~83333bps sample=66.7%
    {CAN_95KBPS,   0xC4, 0xEE, 0x06},  // BRP=5  TQ=21 ~95238bps sample=66.7%
    {CAN_100KBPS,  0xC3, 0xFF, 0x07},  // BRP=4  TQ=25 sample=68.0%
    {CAN_125KBPS,  0xC4, 0xE4, 0x04},  // BRP=5  TQ=16 sample=68.75% [CBUS]
    {CAN_200KBPS,  0xC1, 0xBF, 0x07},  // BRP=2  TQ=25 sample=68.0%
    {CAN_250KBPS,  0xC1, 0xAD, 0x06},  // BRP=2  TQ=20 sample=65.0%
    {CAN_500KBPS,  0xC0, 0xAD, 0x06},  // BRP=1  TQ=20 sample=65.0%
    {CAN_1000KBPS, 0x40, 0x92, 0x02},  // BRP=1  TQ=10 sample=70.0%
};

MCP2515::ERROR MCP2515::setBitrate(const CAN_SPEED canSpeed, CAN_CLOCK canClock)
{
    ERROR error = setConfigMode();
    if (error != ERROR_OK) {
        return error;
    }

    const BitTimingEntry *table;
    switch (canClock) {
        case MCP_8MHZ:  table = BIT_TIMING_8MHz;  break;
        case MCP_16MHZ: table = BIT_TIMING_16MHz; break;
        case MCP_20MHZ: table = BIT_TIMING_20MHz; break;
        default: return ERROR_FAIL;
    }

    const BitTimingEntry &e = table[canSpeed];
    if (e.speed != canSpeed || (e.cfg1 == 0 && e.cfg2 == 0 && e.cfg3 == 0)) {
        return ERROR_FAIL;
    }

    setRegister(MCP_CNF1, e.cfg1);
    setRegister(MCP_CNF2, e.cfg2);
    setRegister(MCP_CNF3, e.cfg3);
    return ERROR_OK;
}

MCP2515::ERROR MCP2515::setBitTiming(uint8_t cnf1, uint8_t cnf2, uint8_t cnf3)
{
    ERROR error = setConfigMode();
    if (error != ERROR_OK) {
        return error;
    }

    setRegister(MCP_CNF1, cnf1);
    setRegister(MCP_CNF2, cnf2);
    setRegister(MCP_CNF3, cnf3);

    return ERROR_OK;
}

MCP2515::ERROR MCP2515::setClkOut(const CAN_CLKOUT divisor)
{
    if (divisor == CLKOUT_DISABLE) {
	/* Turn off CLKEN */
	modifyRegister(MCP_CANCTRL, CANCTRL_CLKEN, 0x00);

	/* Turn on CLKOUT for SOF */
	modifyRegister(MCP_CNF3, CNF3_SOF, CNF3_SOF);
        return ERROR_OK;
    }

    /* Set the prescaler (CLKPRE) */
    modifyRegister(MCP_CANCTRL, CANCTRL_CLKPRE, divisor);

    /* Turn on CLKEN */
    modifyRegister(MCP_CANCTRL, CANCTRL_CLKEN, CANCTRL_CLKEN);

    /* Turn off CLKOUT for SOF */
    modifyRegister(MCP_CNF3, CNF3_SOF, 0x00);
    return ERROR_OK;
}

void MCP2515::prepareId(uint8_t *buffer, const bool ext, const uint32_t id)
{
    uint16_t canid = (uint16_t)(id & 0x0FFFF);

    if (ext) {
        buffer[MCP_EID0] = (uint8_t) (canid & 0xFF);
        buffer[MCP_EID8] = (uint8_t) (canid >> 8);
        canid = (uint16_t)(id >> 16);
        buffer[MCP_SIDL] = (uint8_t) (canid & 0x03);
        buffer[MCP_SIDL] += (uint8_t) ((canid & 0x1C) << 3);
        buffer[MCP_SIDL] |= TXB_EXIDE_MASK;
        buffer[MCP_SIDH] = (uint8_t) (canid >> 5);
    } else {
        buffer[MCP_SIDH] = (uint8_t) (canid >> 3);
        buffer[MCP_SIDL] = (uint8_t) ((canid & 0x07 ) << 5);
        buffer[MCP_EID0] = 0;
        buffer[MCP_EID8] = 0;
    }
}

MCP2515::ERROR MCP2515::setFilterMask(const MASK mask, const bool ext, const uint32_t ulData)
{
    ERROR res = setConfigMode();
    if (res != ERROR_OK) {
        return res;
    }
    
    uint8_t tbufdata[4];
    prepareId(tbufdata, ext, ulData);

    REGISTER reg;
    switch (mask) {
        case MASK0: reg = MCP_RXM0SIDH; break;
        case MASK1: reg = MCP_RXM1SIDH; break;
        default:
            return ERROR_FAIL;
    }

    setRegisters(reg, tbufdata, 4);
    
    return ERROR_OK;
}

MCP2515::ERROR MCP2515::setFilter(const RXF num, const bool ext, const uint32_t ulData)
{
    ERROR res = setConfigMode();
    if (res != ERROR_OK) {
        return res;
    }

    REGISTER reg;

    switch (num) {
        case RXF0: reg = MCP_RXF0SIDH; break;
        case RXF1: reg = MCP_RXF1SIDH; break;
        case RXF2: reg = MCP_RXF2SIDH; break;
        case RXF3: reg = MCP_RXF3SIDH; break;
        case RXF4: reg = MCP_RXF4SIDH; break;
        case RXF5: reg = MCP_RXF5SIDH; break;
        default:
            return ERROR_FAIL;
    }

    uint8_t tbufdata[4];
    prepareId(tbufdata, ext, ulData);
    setRegisters(reg, tbufdata, 4);

    return ERROR_OK;
}

MCP2515::ERROR MCP2515::sendMessage(const TXBn txbn, const struct can_frame *frame)
{
    if (frame->can_dlc > CAN_MAX_DLEN) {
        return ERROR_FAILTX;
    }

    const struct TXBn_REGS *txbuf = &TXB[txbn];

    uint8_t data[13];

    bool ext = (frame->can_id & CAN_EFF_FLAG);
    bool rtr = (frame->can_id & CAN_RTR_FLAG);
    uint32_t id = (frame->can_id & (ext ? CAN_EFF_MASK : CAN_SFF_MASK));

    prepareId(data, ext, id);

    data[MCP_DLC] = rtr ? (frame->can_dlc | RTR_MASK) : frame->can_dlc;

    memcpy(&data[MCP_DATA], frame->data, frame->can_dlc);

    setRegisters(txbuf->SIDH, data, 5 + frame->can_dlc);

    modifyRegister(txbuf->CTRL, TXB_TXREQ, TXB_TXREQ);

    uint8_t ctrl = readRegister(txbuf->CTRL);
    if ((ctrl & (TXB_ABTF | TXB_MLOA | TXB_TXERR)) != 0) {
        return ERROR_FAILTX;
    }
    return ERROR_OK;
}

MCP2515::ERROR MCP2515::sendMessage(const struct can_frame *frame)
{
    // Try to drain any pending queued messages first
    processTxQueue();

    // Try direct hardware transmission
    ERROR result = sendMessageDirect(frame);
    if (result == ERROR_OK) {
        return ERROR_OK;
    }

    // Hardware buffers full - enqueue if room
    noInterrupts();
    bool enqueued = _txQueue.push(*frame);
    interrupts();

    if (enqueued) {
        return ERROR_OK;
    }

    return ERROR_ALLTXBUSY;
}

MCP2515::ERROR MCP2515::sendMessageDirect(const struct can_frame *frame)
{
    if (frame->can_dlc > CAN_MAX_DLEN) {
        return ERROR_FAILTX;
    }

    TXBn txBuffers[N_TXBUFFERS] = {TXB0, TXB1, TXB2};

    for (int i=0; i<N_TXBUFFERS; i++) {
        const struct TXBn_REGS *txbuf = &TXB[txBuffers[i]];
        uint8_t ctrlval = readRegister(txbuf->CTRL);
        if ( (ctrlval & TXB_TXREQ) == 0 ) {
            return sendMessage(txBuffers[i], frame);
        }
    }

    return ERROR_ALLTXBUSY;
}

bool MCP2515::processTxQueue(void)
{
    struct can_frame frame;
    while (_txQueue.peek(frame)) {
        ERROR result = sendMessageDirect(&frame);
        if (result != ERROR_OK) {
            return false;
        }
        _txQueue.pop(frame);
    }
    return true;
}

MCP2515::ERROR MCP2515::readMessage(const RXBn rxbn, struct can_frame *frame)
{
    const struct RXBn_REGS *rxb = &RXB[rxbn];

    uint8_t tbufdata[5];

    readRegisters(rxb->SIDH, tbufdata, 5);

    uint32_t id = (tbufdata[MCP_SIDH]<<3) + (tbufdata[MCP_SIDL]>>5);

    if ( (tbufdata[MCP_SIDL] & TXB_EXIDE_MASK) ==  TXB_EXIDE_MASK ) {
        id = (id<<2) + (tbufdata[MCP_SIDL] & 0x03);
        id = (id<<8) + tbufdata[MCP_EID8];
        id = (id<<8) + tbufdata[MCP_EID0];
        id |= CAN_EFF_FLAG;
    }

    uint8_t dlc = (tbufdata[MCP_DLC] & DLC_MASK);
    if (dlc > CAN_MAX_DLEN) {
        return ERROR_FAIL;
    }

    uint8_t ctrl = readRegister(rxb->CTRL);
    if (ctrl & RXBnCTRL_RTR) {
        id |= CAN_RTR_FLAG;
    }

    frame->can_id = id;
    frame->can_dlc = dlc;

    readRegisters(rxb->DATA, frame->data, dlc);

    modifyRegister(MCP_CANINTF, rxb->CANINTF_RXnIF, 0);

    return ERROR_OK;
}

MCP2515::ERROR MCP2515::readMessage(struct can_frame *frame)
{
    // If interrupt flagged, drain HW buffers into queue
    if (_rxInterruptPending) {
        _rxInterruptPending = false;
        drainRxBuffers();
    }

    // If TX buffer freed, drain software TX queue into hardware
    if (_txInterruptPending) {
        _txInterruptPending = false;
        while (processTxQueue());
    }

    // Read from software queue first
    noInterrupts();
    bool dequeued = _rxQueue.pop(*frame);
    interrupts();

    if (dequeued) {
        return ERROR_OK;
    }

    // Queue empty - try hardware directly
    uint8_t stat = getStatus();
    if ( stat & STAT_RX0IF ) {
        return readMessage(RXB0, frame);
    } else if ( stat & STAT_RX1IF ) {
        return readMessage(RXB1, frame);
    }

    return ERROR_NOMSG;
}

bool MCP2515::checkReceive(void)
{
    // Check software queue first
    if (_rxQueue.getCount() > 0) {
        return true;
    }

    // Check interrupt flag
    if (_rxInterruptPending) {
        return true;
    }

    // Check hardware buffers
    uint8_t res = getStatus();
    return (res & STAT_RXIF_MASK) != 0;
}

bool MCP2515::checkError(void)
{
    uint8_t eflg = getErrorFlags();

    if ( eflg & EFLG_ERRORMASK ) {
        return true;
    } else {
        return false;
    }
}

uint8_t MCP2515::drainRxBuffers(void)
{
    uint8_t drained = 0;
    struct can_frame frame;

    while (true) {
        noInterrupts();
        bool queueFull = _rxQueue.isFull();
        interrupts();

        if (queueFull) break;

        uint8_t stat = getStatus();
        ERROR rc = ERROR_NOMSG;

        if (stat & STAT_RX0IF) {
            rc = readMessage(RXB0, &frame);
        } else if (stat & STAT_RX1IF) {
            rc = readMessage(RXB1, &frame);
        }

        if (rc != ERROR_OK) break;

        noInterrupts();
        if (_rxQueue.push(frame)) {
            drained++;
        } else {
            interrupts();
            break;
        }
        interrupts();
    }

    return drained;
}

uint8_t MCP2515::getErrorFlags(void) const
{
    return readRegister(MCP_EFLG);
}

uint8_t MCP2515::getControlRegister(void) 
{
    return readRegister(MCP_CANCTRL);
}

void MCP2515::clearRXnOVRFlags(void)
{
	modifyRegister(MCP_EFLG, EFLG_RX0OVR | EFLG_RX1OVR, 0);
}

uint8_t MCP2515::getInterrupts(void)
{
    return readRegister(MCP_CANINTF);
}

void MCP2515::clearInterrupts(void)
{
    setRegister(MCP_CANINTF, 0);
}

uint8_t MCP2515::getInterruptMask(void)
{
    return readRegister(MCP_CANINTE);
}

void MCP2515::clearRXnOVR(void)
{
	uint8_t eflg = getErrorFlags();
	if (eflg != 0) {
		clearRXnOVRFlags();
		clearInterrupts();
		//modifyRegister(MCP_CANINTF, CANINTF_ERRIF, 0);
	}
	
}

void MCP2515::clearMERR()
{
	//modifyRegister(MCP_EFLG, EFLG_RX0OVR | EFLG_RX1OVR, 0);
	//clearInterrupts();
	modifyRegister(MCP_CANINTF, CANINTF_MERRF, 0);
}

void MCP2515::clearERRIF()
{
    //modifyRegister(MCP_EFLG, EFLG_RX0OVR | EFLG_RX1OVR, 0);
    //clearInterrupts();
    modifyRegister(MCP_CANINTF, CANINTF_ERRIF, 0);
}

uint8_t MCP2515::errorCountRX(void) const
{
    return readRegister(MCP_REC);
}

uint8_t MCP2515::errorCountTX(void) const
{
    return readRegister(MCP_TEC);
}

//
// ISR-context register access (SPI transaction must be active)
//

uint8_t MCP2515::readRegisterRaw(const REGISTER reg) {
    SPI_PORT->transfer(INSTRUCTION_READ);
    SPI_PORT->transfer(reg);
    return SPI_PORT->transfer(0x00);
}

void MCP2515::readRegistersRaw(const REGISTER reg, uint8_t values[], const uint8_t n) {
    SPI_PORT->transfer(INSTRUCTION_READ);
    SPI_PORT->transfer(reg);
    for (uint8_t i = 0; i < n; i++)
        values[i] = SPI_PORT->transfer(0x00);
}

void MCP2515::setRegistersRaw(const REGISTER reg, const uint8_t values[], const uint8_t n) {
    SPI_PORT->transfer(INSTRUCTION_WRITE);
    SPI_PORT->transfer(reg);
    for (uint8_t i = 0; i < n; i++)
        SPI_PORT->transfer(values[i]);
}

void MCP2515::modifyRegisterRaw(const REGISTER reg, const uint8_t mask, const uint8_t data) {
    SPI_PORT->transfer(INSTRUCTION_BITMOD);
    SPI_PORT->transfer(reg);
    SPI_PORT->transfer(mask);
    SPI_PORT->transfer(data);
}

uint8_t MCP2515::getStatusRaw(void) {
    SPI_PORT->transfer(INSTRUCTION_READ_STATUS);
    return SPI_PORT->transfer(0x00);
}
