/**
 * Quectel BC95 modem driver, tested with B656 firmware.
 * 
 * Copyright (c) 2018 Sparkbit Co., Ltd. All rights reserved.
 * 
 * This work is licensed under the terms of the MIT license.  
 * See LICENSE file in the project root for details.
 */

#include "quectel_bc95.h"
#include "debug.h"

static Sparkbit::Debug dbg("BC95");

static const char HEXMAP[] = "0123456789ABCDEF";

// ----------------------------------------
//   Utility Functions
// ----------------------------------------
uint8_t hexCharToInt(const char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    else if (c >= 'A' && c <= 'F') {
        return 10 + c - 'A';
    }
    else if (c >= 'a' && c <= 'f') {
        return 10 + c - 'a';
    }
    else {
        return 0;
    }
}

uint32_t ipv4AddressStringToInt(const char *addrStr) {
    uint8_t oct1, oct2, oct3, oct4;
    
    if (sscanf(
            addrStr, "%u.%u.%u.%u",
            (unsigned int *)&oct1, 
            (unsigned int *)&oct2, 
            (unsigned int *)&oct3, 
            (unsigned int *)&oct4
        ) == 4) 
    {
        return ((uint32_t)oct1 << 24) + ((uint32_t)oct2 << 16) + ((uint32_t)oct3 << 8) + (uint32_t)oct4;
    }

    return 0;
}

// ----------------------------------------
//   QuectelBC95::Modem
// ----------------------------------------
QuectelBC95::Modem::Modem(Stream *stream) {
    _stream = stream;
    _stream->setTimeout(BC95_DEFAULT_STREAM_READ_TIMEOUT);
}

void QuectelBC95::Modem::writeCommand(const char *command) {
  #ifdef BC95_DBG_WRITE_FRAME
    dbg.print("WRITE: ").noTagOnce().println(command);
  #endif
    
    _stream->print(command);
    _stream->print('\r');
}

int QuectelBC95::Modem::readResponse(char *rspBuf, size_t rspBufLen, size_t *rspLen, unsigned long timeout) {
    ParserState parserState = ParserState::StartCR;
    size_t parsedLen = 0;
    int b;
    
    if (rspLen != NULL) {
        *rspLen = 0;
    }

    unsigned long lastReceivedByteMillis = millis();

    do {
        b = _stream->read();
        if (b == -1) { continue; }

        switch (parserState) {
            case ParserState::StartCR:
                if (b == '\r') {
                  #ifdef BC95_DBG_READ_FRAME
                    dbg.println("READ: FOUND <CR>");
                  #endif
                    
                    parserState = ParserState::StartLF;
                    lastReceivedByteMillis = millis();
                }
                else {
                  #ifdef BC95_DBG_READ_FRAME
                    dbg.print("READ: WAIT <CR>, FOUND [").tagOff().write(b).print("] (").hexByte(b, true, false).println(")").tagOn();
                  #endif
                }
                break;

            case ParserState::StartLF:
                if (b == '\n') {
                  #ifdef BC95_DBG_READ_FRAME
                    dbg.println("READ: FOUND <LF>");
                  #endif

                    parserState = ParserState::Payload;
                    lastReceivedByteMillis = millis();
                }
                else if (b == '\r') {
                  #ifdef BC95_DBG_READ_FRAME
                    dbg.println("READ: FOUND <CR>");
                  #endif
                }
                else {
                  #ifdef BC95_DBG_READ_FRAME
                    dbg.print("READ: INVALID [").tagOff().write(b).print("] (").hexByte(b, true, false).println(")").tagOn();
                  #endif

                    parserState = ParserState::StartCR;
                    parsedLen = 0;
                }
                break;
            
            case ParserState::Payload:
                if (b == '\r') {
                  #ifdef BC95_DBG_READ_FRAME
                    dbg.println("READ: FOUND <CR>");
                  #endif

                    parserState = ParserState::StopLF;
                    lastReceivedByteMillis = millis();
                }
                else if (parsedLen >= rspBufLen-1) {  // excluding null-terminate
                  #ifdef BC95_DBG_READ_FRAME
                    dbg.println("READ: OVERFLOW");
                  #endif

                    parserState = ParserState::StartCR;
                    parsedLen = 0;
                }
                else {
                  #ifdef BC95_DBG_READ_FRAME
                    dbg.print("READ: PYLD [").tagOff().write(b).print("] (").hexByte(b, true, false).println(")").tagOn();
                  #endif

                    rspBuf[parsedLen++] = b;
                    lastReceivedByteMillis = millis();
                }
                break;
            
            case ParserState::StopLF:
                if (b == '\n') {
                    // null-terminate
                    rspBuf[parsedLen] = '\0';

                    // copy parsedLen to rspLen
                    if (rspLen != NULL) {
                        *rspLen = parsedLen;
                    }

                    if (parsedLen == 2 && rspBuf[0] == 'O' && rspBuf[1] == 'K') {
                      #ifdef BC95_DBG_READ_FRAME
                        dbg.println("READ: FOUND <LF>, DONE (type=OK)");
                      #endif

                        return BC95_RESPONSE_TYPE_OK;
                    }
                    else if (strcmp(rspBuf, "ERROR") == 0) {
                      #ifdef BC95_DBG_READ_FRAME
                        dbg.println("READ: FOUND <LF>, DONE (type=ERROR)");
                      #endif

                        return BC95_RESPONSE_TYPE_ERROR;
                    }
                    else if (strncmp(rspBuf, "+CME ERROR: ", 12) == 0) {
                      #ifdef BC95_DBG_READ_FRAME
                        dbg.println("READ: FOUND <LF>, DONE (type=ERROR)");
                      #endif

                        // don't include "+CME ERROR: " prefix
                        parsedLen = parsedLen - 12;
                        // move error code to the begining, including null-terminator
                        memmove(rspBuf, rspBuf+12, parsedLen+1);
                        // update the new rspLen
                        if (rspLen != NULL) {
                            *rspLen = parsedLen;
                        }

                        return BC95_RESPONSE_TYPE_ERROR;
                    }
                    else {
                      #ifdef BC95_DBG_READ_FRAME
                        dbg.print("READ: FOUND <LF>, DONE (type=DATA, len=").tagOff().print(parsedLen).println(")").tagOn();
                      #endif

                        return BC95_RESPONSE_TYPE_DATA;
                    }
                }
                else {
                  #ifdef BC95_DBG_READ_FRAME
                    dbg.print("READ: INVALID [").tagOff().write(b).print("] (").hexByte(b, true, false).println(")").tagOn();
                  #endif

                    parserState = ParserState::StartCR;
                    parsedLen = 0;
                }
                break;
        }

    } while (labs(millis() - lastReceivedByteMillis) < timeout);

  #if defined(BC95_DBG_READ_FRAME) && defined(BC95_DBG_READ_TIMEOUT)
    dbg.println("READ: TOUT");
  #endif

    // nothing copied to the buffer
    return BC95_RESPONSE_TYPE_TIMEOUT;
}

bool QuectelBC95::Modem::readSimpleDataResponse(char *rspBuf, size_t rspBufLen, size_t *rspLen, unsigned long timeout) {
    return readResponse(rspBuf, rspBufLen, rspLen, timeout) == BC95_RESPONSE_TYPE_DATA && waitForOK() == true;
}

bool QuectelBC95::Modem::waitForOK(unsigned long timeout) {
    char rspBuf[BC95_MIN_RSP_BUF_LEN];
    
    return readResponse(rspBuf, sizeof(rspBuf), NULL, timeout) == BC95_RESPONSE_TYPE_OK;
}

// AT
bool QuectelBC95::Modem::pingModem() {
    writeCommand("AT");
    return waitForOK();
}

// AT+CGMI
bool QuectelBC95::Modem::readManufacturerIdentification(char *rspBuf, size_t rspBufLen) {
    writeCommand("AT+CGMI");
    return readSimpleDataResponse(rspBuf, rspBufLen) == true;
}

// AT+CGMM
bool QuectelBC95::Modem::readModelIdentification(char *rspBuf, size_t rspBufLen) {
    writeCommand("AT+CGMM");
    return readSimpleDataResponse(rspBuf, rspBufLen) == true;
}

// AT+CGMR
bool QuectelBC95::Modem::readRevisionIdentification(char *rspBuf, size_t rspBufLen) {
    writeCommand("AT+CGMR");
    return readSimpleDataResponse(rspBuf, rspBufLen) == true;
}

// AT+CGSN=1 (IMEI)
bool QuectelBC95::Modem::readInternationalMobileStationEquipmentIdentity(char *rspBuf, size_t rspBufLen) {
    writeCommand("AT+CGSN=1");

    // +CGSN:xxxxxxxxxxxxxxx
    return readSimpleDataResponse(rspBuf, rspBufLen) == true
        && memmove(rspBuf, rspBuf+6, 16);
}

// AT+CEREG?
bool QuectelBC95::Modem::readNetworkRegistrationStatus(cereg_t *rsp) {
    char rspBuf[BC95_MIN_RSP_BUF_LEN];

    writeCommand("AT+CEREG?");

    if (readSimpleDataResponse(rspBuf, sizeof(rspBuf)) == true 
        && sscanf(rspBuf, "+CEREG:%u,%u", (unsigned int *)&(rsp->urc), (unsigned int *)&(rsp->status)) == 2)
    {
        return true;
    }

    return false;
}

uint8_t QuectelBC95::Modem::readNetworkRegistrationStatus() {
    cereg_t rsp;

    if (readNetworkRegistrationStatus(&rsp) == true) {
        return rsp.status;
    }

    // otherwise
    return BC95_NETWORK_STAT_UNKNOWN;
}

// AT+CSCON
bool QuectelBC95::Modem::readRadioConnectionStatus(cscon_t *rsp) {
    char rspBuf[24];

    memset(rsp, 0, sizeof(cscon_t));

    writeCommand("AT+CSCON?");

    if (readSimpleDataResponse(rspBuf, sizeof(rspBuf)) == true
        && sscanf(rspBuf, "+CSCON:%u,%u", (unsigned int *)&(rsp->urc), (unsigned int *)&(rsp->mode)) == 2) 
    {
        // <state> and <access> are not yet supported
        return true;
    }

    return false;
}

uint8_t QuectelBC95::Modem::readRadioConnectionStatus() {
    cscon_t rsp;

    if (readRadioConnectionStatus(&rsp) == true) {
        return rsp.mode;
    }

    // otherwise
    return BC95_CSCON_MODE_IDLE;
}

// AT+CSQ
bool QuectelBC95::Modem::readSignalQuality(csq_t *rsp) {
    char rspBuf[32];
    uint8_t rssi, ber;

    memset(rsp, 0, sizeof(csq_t));

    writeCommand("AT+CSQ");

    if (readSimpleDataResponse(rspBuf, sizeof(rspBuf)) == true && sscanf(rspBuf, "+CSQ:%u,%u", (unsigned int *)&rssi, (unsigned int *)&ber) == 2) {
        rsp->rssi.value = rssi;
        rsp->rssi.dBm = (rsp->rssi.value < 99) ? (-113 + (rsp->rssi.value * 2)) : INT16_MIN;
        rsp->ber = ber;

        return true;
    }

    return false;
}

// AT+CGPADDR=<cid>
bool QuectelBC95::Modem::readPDPAddress(uint8_t cid, pdp_addr_t *rsp) {
    char command[32];
    char rspBuf[32];

    memset(rsp, 0, sizeof(pdp_addr_t));

    sprintf(command, "AT+CGPADDR=%u", cid);
    writeCommand(command);

    if (readSimpleDataResponse(rspBuf, sizeof(rspBuf)) == true) {
        if (sscanf(rspBuf, "+CGPADDR:%u,%s", (unsigned int *)&(rsp->cid), rsp->addr.strVal) == 2) {
            rsp->addr.intVal = ipv4AddressStringToInt(rsp->addr.strVal);
            return true;
        }
    }

    return false;
}

// AT+COPS
bool QuectelBC95::Modem::readPLMNSelection(cops_t *rsp) {
    char rspBuf[32];

    memset(rsp, 0, sizeof(cops_t));

    writeCommand("AT+COPS?");

    if (readSimpleDataResponse(rspBuf, sizeof(rspBuf)) == true) {
        if (sscanf(rspBuf, "+COPS:%u,%u,\"%[^\"]", (unsigned int *)&(rsp->mode), (unsigned int *)&(rsp->format), rsp->oper) == 3) {
            return true;
        }
    }

    return false;
}

// AT+CGATT=<state> - PS attach or detach
bool QuectelBC95::Modem::isPSAttached() {
    char rspBuf[BC95_MIN_RSP_BUF_LEN];
    uint8_t state;

    writeCommand("AT+CGATT?");

    if (readSimpleDataResponse(rspBuf, sizeof(rspBuf)) == true && sscanf(rspBuf, "+CGATT:%u", (unsigned int *)&state) == 1) {
        return state != 0;
    }

    return false;
}

bool QuectelBC95::Modem::attachPS() {
    writeCommand("AT+CGATT=1");
    return waitForOK();
}

bool QuectelBC95::Modem::detachPS() {
    writeCommand("AT+CGATT=0");
    return waitForOK();
}

// AT+CIMI
bool QuectelBC95::Modem::readInternationalMobileSubscriberIdentity(char *rspBuf, size_t rspBufLen) {
    writeCommand("AT+CIMI");
    return readSimpleDataResponse(rspBuf, rspBufLen) == true;
}

// AT+CGDCONT?
uint8_t QuectelBC95::Modem::readPDNConnectionInfo(pdn_info_t rsp[], uint8_t rspMaxLen) {
    char lineBuf[192];
    size_t lineLen;
    uint8_t rspLen = 0;

    char *tok;

    memset(rsp, 0, sizeof(pdn_info_t) * rspMaxLen);
    writeCommand("AT+CGDCONT?");
    
    // purge the first <CR><LF>
    if (_stream->readBytesUntil('\n', lineBuf, sizeof(lineBuf)) <= 0) {
        return 0;
    }

    for (int i = 0 ; i <= rspMaxLen ; i++) {
        lineLen = _stream->readBytesUntil('\n', lineBuf, sizeof(lineBuf));

        if (lineLen == 1 &&
            _stream->readBytesUntil('\n', lineBuf, sizeof(lineBuf)) == 3 &&
            memcmp(lineBuf, "OK\r", 3) == 0) 
        {
            // <CR><LF>OK<CR><LF> is found
            break;
        }

        // a response line is found
        // +CGDCONT: <cid>,<type>,<apn>,<addr><dataComp>,<headerComp>
        lineBuf[lineLen-1] = '\0';

        // +CGDCONT:
        tok = strtok(lineBuf, ":");

        // <cid>
        tok = strtok(NULL, ",");
        if (tok != NULL) {
            rsp[i].cid = atoi(tok);
        }

        // <type>
        tok = strtok(NULL, ",\"");
        if (tok != NULL) {
            strcpy(rsp[i].type, tok);
        }

        // <apn>
        tok = strtok(NULL, ",\"");
        if (tok != NULL) {
            strcpy(rsp[i].apn, tok);
        }

        // <addr>
        tok = strtok(NULL, ",\"");
        if (tok != NULL) {
            strcpy(rsp[i].addr.strVal, tok);
            rsp[i].addr.intVal = ipv4AddressStringToInt(tok);
        }

        // <dataComp>
        tok = strtok(NULL, ",");
        if (tok != NULL) {
            rsp[i].dataComp = atoi(tok);
        }

        // <headerComp>
        tok = strtok(NULL, ",");
        if (tok != NULL) {
            rsp[i].headerComp = atoi(tok);
        }

        rspLen++;
    }

    return rspLen;
}

// AT+CFUN
bool QuectelBC95::Modem::setPhoneFunctionality(uint8_t level, unsigned long timeout) {
    char command[16];

    sprintf(command, "AT+CFUN=%u", level);
    writeCommand(command);
    return waitForOK(timeout);
}

// AT+CMEE=<n>
bool QuectelBC95::Modem::setErrorResponseFormat(uint8_t n) {
    char command[16];

    sprintf(command, "AT+CMEE=%u", n);
    writeCommand(command);
    return waitForOK();
}

// AT+NRB - Reboot the modem
bool QuectelBC95::Modem::reboot(bool waitUntilFinished) {
    char rspBuf[32];

    writeCommand("AT+NRB");

    // response: REBOOTING
    if (readResponse(rspBuf, sizeof(rspBuf)) != BC95_RESPONSE_TYPE_DATA || strcmp(rspBuf, "REBOOTING") != 0) {
        return false;
    }

    if (!waitUntilFinished) {
        return true;
    }

    // response: REBOOT_CAUSE_APPLICATION_AT
    if (readResponse(rspBuf, sizeof(rspBuf), NULL, BC95_DEFAULT_REBOOT_TIMEOUT) != BC95_RESPONSE_TYPE_DATA || strcmp(rspBuf, "REBOOT_CAUSE_APPLICATION_AT") != 0) {
        return false;
    }

    // response: OK
    return waitForOK();
}

// AT+NSOCR=<type>,<protocol>,<listen port>[,<receive control>] - Create a socket
// For BC95, only type=DGRAM and protocol=17 are supported.
int8_t QuectelBC95::Modem::createSocket(uint16_t port, bool recvMsg) {
    char command[32];
    char rspBuf[BC95_MIN_RSP_BUF_LEN];

    sprintf(command, "AT+NSOCR=DGRAM,17,%u,%u", port, (recvMsg ? 1 : 0));
    writeCommand(command);

    if (readSimpleDataResponse(rspBuf, sizeof(rspBuf)) == true) {
        return atoi(rspBuf);
    }

    return -1;
}

// AT+NSOST=<socket>,<remote_addr>,<remote_port>,<length>,<data> - Send UDP datagram
size_t QuectelBC95::Modem::_sendUDPDatagram(uint8_t socket, const char *remoteHost, uint16_t remotePort, uint16_t flag, const uint8_t *dataBuf, size_t dataLen) {
    const char *command;
    char rspBuf[BC95_MIN_RSP_BUF_LEN];
    size_t i;
    uint8_t cH, cL;
    size_t bytesSent;
    
    char pBuf[40] = {0};
    
    if (!flag) {
        command = "AT+NSOST=";
        sprintf(pBuf, "%u,%s,%u,%u,", socket, remoteHost, remotePort, dataLen);
    }
    else {
        command = "AT+NSOSTF=";
        sprintf(pBuf, "%u,%s,%u,0x%03X,%u,", socket, remoteHost, remotePort, flag, dataLen);
    }

    // command and parameters
  #ifdef BC95_DBG_WRITE_FRAME
    dbg.print("WRITE: ").tagOff().print(command).print(pBuf);
  #endif
    _stream->print(command);
    _stream->print(pBuf);

    // data
    for (i = 0 ; i < dataLen ; i++) {
        cH = HEXMAP[(dataBuf[i] & 0xF0) >> 4];
        cL = HEXMAP[dataBuf[i] & 0x0F];
        
      #ifdef BC95_DBG_WRITE_FRAME
        dbg.write(cH);
        dbg.write(cL);
      #endif
        _stream->write(cH);
        _stream->write(cL);
    }
    
    // end
  #ifdef BC95_DBG_WRITE_FRAME
    dbg.println().tagOn();
  #endif
    _stream->write('\r');
    _stream->flush();

    if (readSimpleDataResponse(rspBuf, sizeof(rspBuf)) == true && sscanf(rspBuf, "%*u,%u", (unsigned int *)&bytesSent) == 1) {
        return bytesSent;
    }

    return 0;
}

size_t QuectelBC95::Modem::sendUDPDatagram(uint8_t socket, const char *remoteHost, uint16_t remotePort, const char *msg) {
    return _sendUDPDatagram(socket, remoteHost, remotePort, BC95_NSOST_FLAG_NONE, (const uint8_t *)msg, strlen(msg));
}

size_t QuectelBC95::Modem::sendUDPDatagram(uint8_t socket, const char *remoteHost, uint16_t remotePort, const uint8_t *dataBuf, size_t dataLen) {
    return _sendUDPDatagram(socket, remoteHost, remotePort, BC95_NSOST_FLAG_NONE, dataBuf, dataLen);
}

// AT+NSORF=<socket>,<req_length> - Receive UDP datagram
size_t QuectelBC95::Modem::receiveUDPDatagram(uint8_t socket, uint8_t *dataBuf, size_t dataBufLen, udp_rx_data_t *rsp) {
    // clear dataBuf and response
    memset(dataBuf, 0, dataBufLen);
    memset(rsp, 0, sizeof(udp_rx_data_t));
    rsp->dataBuf = dataBuf;

    char command[16];
    char chunkBuf[BC95_NSORF_CHUNK_BUF_LEN];
    size_t remaining = BC95_NSORF_CHUNK_LEN;

    char *payload;
    size_t payloadLen, i;
    int payloadPos;

    sprintf(command, "AT+NSORF=%u,%u", socket, BC95_NSORF_CHUNK_LEN);

    while (remaining > 0) {
        writeCommand(command);

        if (readResponse(chunkBuf, BC95_NSORF_CHUNK_BUF_LEN) != BC95_RESPONSE_TYPE_DATA) {
            return 0;
        }

        payloadPos = 0;

        if (sscanf(chunkBuf, "%u,%[^,],%u,%u,%n", 
                (unsigned int *)&(rsp->socket),
                rsp->remoteAddr.strVal,
                (unsigned int *)&(rsp->remotePort),
                (unsigned int *)&payloadLen,
                &payloadPos
            ) !=  4 || payloadPos == 0)
        {
            return 0;
        }

        if (waitForOK() != true) {
            return 0;
        }

        payload = chunkBuf + payloadPos;

        for (i = 0 ; i < payloadLen ; i++) {
            if (rsp->dataLen >= dataBufLen) {
                break;
            }

            dataBuf[rsp->dataLen] = hexCharToInt(payload[i*2]) * 16 + hexCharToInt(payload[i*2+1]);
            rsp->dataLen++;
        }
        
        remaining = atoi(payload + (2 * payloadLen) + 1);  // skips the last comma
    }

    return rsp->dataLen;
}

// AT+NSOCL=<socket> - Close a socket
bool QuectelBC95::Modem::closeSocket(uint8_t socket) {
    char command[16];

    sprintf(command, "AT+NSOCL=%u", socket);
    writeCommand(command);
    return waitForOK();
}

// AT+NPING=<ip>,<p_size>,<timeout>
bool QuectelBC95::Modem::pingHost(const char *ipAddressStr, ping_response_t *rsp, unsigned long timeout) {
    char command[64];
    char rspBuf[64];

    memset(rsp, 0, sizeof(ping_response_t));
    sprintf(command, "AT+NPING=%s,16,%lu", ipAddressStr, timeout);
    writeCommand(command);

    if (waitForOK() == true) {
        if (readResponse(rspBuf, sizeof(rspBuf), NULL, timeout+1000) == BC95_RESPONSE_TYPE_DATA) {
            if (strncmp(rspBuf, "+NPINGERR", 9) == 0) {
                return false;
            }

            if (sscanf(rspBuf, "+NPING:%[^,],%u,%u", rsp->addr.strVal, (unsigned int *)&(rsp->ttl), (unsigned int *)&(rsp->rtt)) == 3) {
                rsp->addr.intVal = ipv4AddressStringToInt(rsp->addr.strVal);
                return true;
            }
        }
    }

    return false;
}

// AT+NCONFIG=AUTOCONNECT,<enabled>
bool QuectelBC95::Modem::configAutoConnect(bool enabled) {
    if (enabled) {
        writeCommand("AT+NCONFIG=AUTOCONNECT,TRUE");
    }
    else {
        writeCommand("AT+NCONFIG=AUTOCONNECT,FALSE");
    }

    return waitForOK();
}


