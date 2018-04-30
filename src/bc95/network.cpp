/**
 * Network adaptation layer for Quectel BC95 modem.
 * 
 * Copyright (c) 2018 Sparkbit Co., Ltd. All rights reserved.
 * 
 * This work is licensed under the terms of the MIT license.  
 * See LICENSE file in the project root for details.
 */

#include "network.h"
#include "debug.h"

static Sparkbit::Debug dbg("NET");

// CoAP message ID
static uint16_t coapMessageId;

static int8_t defaultSocket = -1;

// CoAP message ID table
#ifdef NET_COAP_IGNORE_DUPLICATE_INCOMING_MSG_ID
typedef struct {
    uint32_t address;
    uint16_t port;
    uint16_t messageId;
    unsigned long tsMillis;
} recv_msg_id_t;

static recv_msg_id_t coapMsgTrackingList[NET_COAP_RECEIVED_MSG_ID_LIST_LEN];
static uint8_t msgTrackingNewEntryPos = 0;
#endif

// handlers
static void (*hIncomingUDPPacket)(const char *srcAddrStr, uint16_t srcPort, uint16_t dstPort, const uint8_t *payload, uint16_t payloadLen) = NULL;
static void (*hIncomingCoAPMessage)(const char *srcAddrStr, uint16_t srcPort, uint16_t dstPort, CoapPDU *message) = NULL;

// ----------------------------------------
//   Modem
// ----------------------------------------
#if defined(__AVR__)
    #include <SoftwareSerial.h>
    #warning "Using SoftwareSerial to communicate with the modem (RX=8, TX=9)"
    static SoftwareSerial mdmPort(8, 9);
#elif defined(__SAM3X8E__)
    // Arduino Due
    static HardwareSerial &mdmPort = Serial1;
#elif defined(__SAMD21G18A__)
    // Arduino Zero, MKR *, M0/M0 Pro, Tian
    // Adafruit Circuit Playground Express
    static HardwareSerial &mdmPort = Serial;
#elif defined(ESP32)
    #warning "Using HardwareSerial (UART2) to communicate with the modem (RX=16, TX=17)"
    static HardwareSerial mdmPort(2);
#else
    #error "Please define your modem serial port"
#endif

static QuectelBC95::Modem modem(&mdmPort);

QuectelBC95::Modem *netGetModem() {
    return &modem;
}

// ----------------------------------------
//   Initialization
// ----------------------------------------
void netInit() {
    coapMessageId = random(0xFFFF);
    
    // reset pin is active HIGH
    pinMode(NET_MODEM_RESET_PIN, OUTPUT);
    digitalWrite(NET_MODEM_RESET_PIN, LOW);

    mdmPort.begin(NET_MODEM_SERIAL_BAUD);
}

bool _netResetModem() {
    unsigned long startMillis;

    digitalWrite(NET_MODEM_RESET_PIN, HIGH);
    delay(100);
    digitalWrite(NET_MODEM_RESET_PIN, LOW);

    startMillis = millis();

    while (modem.pingModem() != true) {
        if (millis() - startMillis > NET_MODEM_RESET_TIMEOUT) {
            return false;
        }

        // purge any tx/rx buffer garbages
        mdmPort.print("\r\r\r");
        delay(100);

        while (mdmPort.read() != -1) {
            if (millis() - startMillis > NET_MODEM_RESET_TIMEOUT) {
                return false;
            }
        }
    }

    if (modem.setErrorResponseFormat(0) != true) {
        return false;
    }

    if (modem.configAutoConnect(true) != true) {
        return false;
    }

    return true;
}

#ifdef NET_DBG_VERBOSE_MODEM_INFO
void _netPrintModemInfo() {
    char rspBuf[64];

    if (modem.readInternationalMobileStationEquipmentIdentity(rspBuf, sizeof(rspBuf)) == true) {
        dbg
            .print("IMEI: ")
            .noTagOnce()
            .println(rspBuf);
    }

    // IMSI might take some times to be readable
    dbg.print("IMSI: ");
    for (int i = 0 ; i < 30 ; i++) {
        if (modem.readInternationalMobileSubscriberIdentity(rspBuf, sizeof(rspBuf)) == true) {
            dbg.noTagOnce().print('\r').print("IMSI: ").noTagOnce().println(rspBuf);
            break;
        }
        else {
            dbg.noTagOnce().print(".");
        }

        delay(1000);
    }
}
#else
void _netPrintModemInfo() {}
#endif

#ifdef NET_DBG_VERBOSE_NETWORK_INFO
void _netPrintNetworkInfo() {
    QuectelBC95::csq_t csq;
    QuectelBC95::pdn_info_t pdnInfo[4];
    QuectelBC95::pdn_info_t *pdn;
    size_t pdnRspLen;
    size_t i;

    if (modem.readSignalQuality(&csq)) {
        dbg.print("RSSI: ").tagOff().print(csq.rssi.dBm).println(" dBm").tagOn();
    }

    pdnRspLen = modem.readPDNConnectionInfo(pdnInfo, 4);
    for (i = 0 ; i < pdnRspLen ; i++) {
        pdn = &(pdnInfo[i]);
        dbg.print("PDN: ").tagOff().print(pdn->cid).print(", Type: ").print(pdn->type).print(", APN: ").print(pdn->apn).println().tagOn();
    }

    QuectelBC95::pdp_addr_t ipAddr;

    if (modem.readPDPAddress(0, &ipAddr)) {
        dbg.print("IP: ").noTagOnce().println(ipAddr.addr.strVal);
    }
}
#else
void _netPrintNetworkInfo() {}
#endif

bool _netConfigDefaultSocket() {
    // create a default socket, enable data receiving
    defaultSocket = modem.createSocket(NET_DEFAULT_SOCKET_LOCAL_PORT, true);

    return defaultSocket >= 0;
}

bool netInitNetwork() {
    unsigned long startMillis = millis();

  #ifdef NET_DBG_INIT_NETWORK
    dbg.print("Resetting the modem ... ");
  #endif

    // reboot the modem, wait until finish
    if (_netResetModem() == true) {
      #ifdef NET_DBG_INIT_NETWORK
        dbg.noTagOnce().println("OK");
      #endif
    }
    else {
      #ifdef NET_DBG_INIT_NETWORK
        dbg.noTagOnce().println("Failed");
      #endif
        
        return false;
    }

  #if defined(NET_DBG_INIT_NETWORK) && defined(NET_DBG_VERBOSE_MODEM_INFO)
    _netPrintModemInfo();
  #endif
    
  #ifdef NET_DBG_INIT_NETWORK
    dbg.print("Connecting ");
  #endif

    // wait for the network to be ready
    while (1) {
        if (netIsNetworkReady() == true) {
            // TODO workaround, waiting for the modem to be ready
            delay(3000);

          #ifdef NET_DBG_INIT_NETWORK
            dbg.noTagOnce().println(" OK");
          #endif

            break;
        }

        if (labs(millis() - startMillis) >= NET_DEFAULT_INIT_NETWORK_TIMEOUT) {
          #ifdef NET_DBG_INIT_NETWORK
            dbg.noTagOnce().println(" Failed");
          #endif
            
            return false;
        }

      #ifdef NET_DBG_INIT_NETWORK
        dbg.noTagOnce().print(".");
      #endif
        
        delay(1000);
    }

  #if defined(NET_DBG_INIT_NETWORK) && defined(NET_DBG_VERBOSE_NETWORK_INFO)
    _netPrintNetworkInfo();
  #endif

    if (_netConfigDefaultSocket() != true) {
        return false;
    }

    // all done
    return true;
}

bool netIsNetworkReady() {
    return modem.readNetworkRegistrationStatus() == BC95_NETWORK_STAT_REGISTERED;
}

// ----------------------------------------
//   General
// ----------------------------------------
bool netPingHost(const char *ipAddress, unsigned long timeout) {
    QuectelBC95::ping_response_t rsp;

    return modem.pingHost(ipAddress, &rsp, timeout);
}

// ----------------------------------------
//   UDP
// ----------------------------------------
bool netSendUDPPacket(const char *dstAddrStr, uint16_t dstPort, uint16_t srcPort, const uint8_t *payload, uint16_t payloadLen) {
  #ifdef NET_DBG_UDP_OUTGOING
    dbg
        .print("UDP SEND")
        .tagOff()
        .print(", fromPort=")
        .print(srcPort)
        .print(", to=")
        .print(dstAddrStr)
        .print(":")
        .print(dstPort)
        .print(", payload=")
        .hexDump(payload, payloadLen)
        .tagOn();
  #endif

    // srcPort is not used
    (void)srcPort;

    return modem.sendUDPDatagram(
        defaultSocket,
        dstAddrStr, dstPort,
        payload, payloadLen
    ) == payloadLen;
}

// ----------------------------------------
//   CoAP
// ----------------------------------------
uint16_t netGetNextCoAPMessageId() {
    if (++coapMessageId == 0) {
        coapMessageId = 1;
    }

    return coapMessageId;
}

void netGetRandomCoAPToken(uint8_t *buf, size_t len) {
    for (int i = 0 ;  i < (int)len ; i++) {
        buf[i] = random(0xFF);
    }
}

#ifdef NET_COAP_IGNORE_DUPLICATE_INCOMING_MSG_ID
bool netIsCoAPMessageIdDuplicate(uint32_t srcAddress, uint16_t srcPort, uint16_t messageId) {
    recv_msg_id_t *pEntry;

    // search for an existing entry
    for (int i = 0 ; i < NET_COAP_RECEIVED_MSG_ID_LIST_LEN ; i++) {
        pEntry = &coapMsgTrackingList[i];

        if (srcAddress == pEntry->address && srcPort == pEntry->port && messageId == pEntry->messageId) {
            // check if the entry is still valid
            if (labs(millis() - pEntry->tsMillis) < NET_COAP_RECEIVED_MSG_ID_ENTRY_TIMEOUT) {
                return true;
            }
            else {
                // entry has expired, reset
                memset(pEntry, 0, sizeof(recv_msg_id_t));
            }
        }
    }
    
    // new message id received, add to the list    
    pEntry = &coapMsgTrackingList[msgTrackingNewEntryPos];
    
    pEntry->address = srcAddress;
    pEntry->port = srcPort;
    pEntry->messageId = messageId;
    pEntry->tsMillis = millis();

    // move to the new entry index to the next usable position
    msgTrackingNewEntryPos++;
    msgTrackingNewEntryPos =  msgTrackingNewEntryPos % NET_COAP_RECEIVED_MSG_ID_LIST_LEN;
    
    return false;
}
#endif

bool netSendCoAPMessage(const char *dstAddrStr, uint16_t dstPort, CoapPDU *message) {
    return netSendCoAPMessage(dstAddrStr, dstPort, 0, message);
}

bool netSendCoAPMessage(const char *dstAddrStr, uint16_t dstPort, uint16_t srcPort, CoapPDU *message) {
  #ifdef NET_DBG_COAP_OUTGOING
    int tokenLen = message->getTokenLength();
    int payloadLen = message->getPayloadLength();

    dbg
        .print("CoAP SEND")
        .tagOff()
        .print(", fromPort=")
        .print(srcPort)
        .print(", to=")
        .print(dstAddrStr)
        .print(":")
        .print(dstPort)
        .print(", type=")
        .hexByte(message->getType(), true, false)
        .print(", code=")
        .hexByte(message->getCode(), true, false)
        .print(", mid=")
        .hexShort(message->getMessageID(), true, false);
    
    if (tokenLen > 0) {
        dbg
            .print(", token=")
            .hexString(message->getTokenPointer(), tokenLen, true, false);
    }

    if (payloadLen > 0) {
        dbg
            .print(", payload=")
            .hexDump(message->getPayloadPointer(), payloadLen, 0, false);
    }

    dbg.println().tagOn();
  #endif  /* NET_DBG_COAP_OUTGOING */

    return netSendUDPPacket(dstAddrStr, dstPort, srcPort, message->getPDUPointer(), message->getPDULength());
}

bool netSendCoAPEmptyAckMessage(const char *dstAddrStr, uint16_t dstPort, uint16_t messageId) {
    return netSendCoAPEmptyAckMessage(dstAddrStr, dstPort, 0, messageId);
}

bool netSendCoAPEmptyAckMessage(const char *dstAddrStr, uint16_t dstPort, uint16_t srcPort, uint16_t messageId) {
  #ifdef NET_DBG_COAP_OUTGOING
    dbg
        .print("CoAP SEND [EMPTY ACK]")
        .tagOff()
        .print(", fromPort=")
        .print(srcPort)
        .print(", to=")
        .print(dstAddrStr)
        .print(":")
        .print(dstPort)
        .print(", mid=")
        .hexShort(messageId, true)
        .tagOn();
  #endif
    
    // 4 bytes empty ack message
    uint8_t coapBuf[4];
    CoapPDU ack(coapBuf, sizeof(coapBuf));

    ack.reset();
    ack.setVersion(1);
    ack.setType(CoapPDU::COAP_ACKNOWLEDGEMENT);
    ack.setCode(CoapPDU::COAP_EMPTY);
    ack.setMessageID(messageId);

    return netSendUDPPacket(dstAddrStr, dstPort, srcPort, ack.getPDUPointer(), ack.getPDULength()); 
}

bool netSendCoAPResetMessage(const char *dstAddrStr, uint16_t dstPort, uint16_t messageId) {
    return netSendCoAPResetMessage(dstAddrStr, dstPort, 0, messageId);
}

bool netSendCoAPResetMessage(const char *dstAddrStr, uint16_t dstPort, uint16_t srcPort, uint16_t messageId) {
  #ifdef NET_DBG_COAP_OUTGOING
    dbg
        .print("CoAP SEND [EMPTY RESET]")
        .tagOff()
        .print(", fromPort=")
        .print(srcPort)
        .print(", to=")
        .print(dstAddrStr)
        .print(":")
        .print(dstPort)
        .print(", mid=")
        .hexShort(messageId, true)
        .tagOn();
  #endif
    
    // 4 bytes empty reset message
    uint8_t coapBuf[4];
    CoapPDU rst(coapBuf, sizeof(coapBuf));

    rst.reset();
    rst.setVersion(1);
    rst.setType(CoapPDU::COAP_RESET);
    rst.setCode(CoapPDU::COAP_EMPTY);
    rst.setMessageID(messageId);

    return netSendUDPPacket(dstAddrStr, dstPort, srcPort, rst.getPDUPointer(), rst.getPDULength()); 
}

bool netSendCoAPPing(const char *dstAddrStr, uint16_t dstPort, unsigned long timeout) {
    uint8_t coapBuf[4];
    CoapPDU message(coapBuf, sizeof(coapBuf));

    uint8_t rxBuf[32];
    QuectelBC95::udp_rx_data_t rx;

    uint16_t messageId = netGetNextCoAPMessageId();
    unsigned long startMillis;

    message.reset();
    message.setVersion(1);
    message.setType(CoapPDU::COAP_CONFIRMABLE);
    message.setCode(CoapPDU::COAP_EMPTY);
    message.setMessageID(messageId);

  #ifdef NET_DBG_COAP_PING
    dbg
        .print("CoAP Ping")
        .tagOff()
        .print(", to=")
        .print(dstAddrStr)
        .print(":")
        .println(dstPort)
        .tagOn();
  #endif

    if (netSendUDPPacket(dstAddrStr, dstPort, 0, message.getPDUPointer(), message.getPDULength()) != true) {
      #ifdef NET_DBG_COAP_PING
        dbg.println("CoAP Ping, failed to send request");
      #endif
        
        return false;
    }

    startMillis = millis();
    while (labs(millis() - startMillis) < timeout) {
        bool rxOK = modem.receiveUDPDatagram(defaultSocket, rxBuf, sizeof(rxBuf), &rx) > 0;
        
        if  (rxOK && sizeof(coapBuf) >= rx.dataLen) {
            message.reset();
            memcpy(coapBuf, rx.dataBuf, rx.dataLen);

            if (message.validate() && 
                message.getType() == CoapPDU::COAP_RESET &&
                message.getCode() == CoapPDU::COAP_EMPTY &&
                message.getMessageID() == messageId)
            {
              #ifdef NET_DBG_COAP_PING
                dbg
                    .print("CoAP Pong")
                    .tagOff()
                    .print(", from=")
                    .print(rx.remoteAddr.strVal)
                    .print(":")
                    .print(rx.remotePort)
                    .print(", time=")
                    .print(millis() - startMillis)
                    .println(" ms")
                    .tagOn();
              #endif
                
                return true;
            }
        }
    }

  #ifdef NET_DBG_COAP_PING
    dbg.println("CoAP Ping, request timeout");
  #endif
    
    return false;
}

// ----------------------------------------
//   Packet handler
// ----------------------------------------
void netSetIncomingUDPPacketHandler(void (*handler)(const char *srcAddrStr, uint16_t srcPort, uint16_t dstPort, const uint8_t *payload, uint16_t payloadLen)) {
    hIncomingUDPPacket = handler;
}

void netSetIncomingCoAPMessageHandler(void (*handler)(const char *srcAddrStr, uint16_t srcPort, uint16_t dstPort, CoapPDU *message)) {
    hIncomingCoAPMessage = handler;
}

// ----------------------------------------
//   Task processor
// ----------------------------------------
void _handleModemIncomingUDPData(QuectelBC95::udp_rx_data_t *data);
void _dispatchUDPPacket(const char *srcAddrStr, uint16_t srcPort, uint16_t dstPort, const uint8_t *payload, uint16_t payloadLen);
void _dispatchCoAPMessage(const char *srcAddrStr, uint32_t srcAddrInt, uint16_t srcPort, uint16_t dstPort, const uint8_t *udpPayload, uint16_t udpPayloadLen);

void netTaskTick() {
    uint8_t udpDataBuf[NET_UDP_PAYLOAD_MAX_LEN];
    QuectelBC95::udp_rx_data_t udpData;

    // check for any incoming UDP data
    if (modem.receiveUDPDatagram(defaultSocket, udpDataBuf, sizeof(udpDataBuf), &udpData) > 0) {
        _handleModemIncomingUDPData(&udpData);
    }
    
    // TODO process another modem events
}

void _handleModemIncomingUDPData(QuectelBC95::udp_rx_data_t *data) {
    const char *srcAddrStr = data->remoteAddr.strVal;
    uint32_t srcAddrInt = data->remoteAddr.intVal;
    uint16_t srcPort = data->remotePort;
    uint16_t dstPort = NET_DEFAULT_SOCKET_LOCAL_PORT;  // TODO resolve to match the local port associated with the data->sockId
    const uint8_t *udpPayload = data->dataBuf;
    uint16_t udpPayloadLen = data->dataLen;

    _dispatchUDPPacket(srcAddrStr, srcPort, dstPort, udpPayload, udpPayloadLen);

  #ifdef NET_PROCESS_COAP_INCOMING_MESSAGE
    // TODO --WORKAROUND-- for two CoAP messages come together in one UDP packet
    // EMPTY CoAP message must be 4 bytes in length
    if (udpPayloadLen > 4 && udpPayload[1] == CoapPDU::COAP_EMPTY) {
      #ifdef NET_DBG_COAP_INCOMING
        dbg.println("CoAP RECV, WORKAROUND APPLIED!");
      #endif
          
        _dispatchCoAPMessage(srcAddrStr, srcAddrInt, srcPort, dstPort, udpPayload, 4);
        _dispatchCoAPMessage(srcAddrStr, srcAddrInt, srcPort, dstPort, udpPayload+4, udpPayloadLen-4);
      }
      else {
        _dispatchCoAPMessage(srcAddrStr, srcAddrInt, srcPort, dstPort, udpPayload, udpPayloadLen);
      }
  #endif  /* NET_PROCESS_COAP_INCOMING_MESSAGE */
}

void _dispatchUDPPacket(const char *srcAddrStr, uint16_t srcPort, uint16_t dstPort, const uint8_t *payload, uint16_t payloadLen) {
  #ifdef NET_DBG_UDP_INCOMING
    dbg
        .print("UDP RECV")
        .tagOff()
        .print(", atPort=")
        .print(dstPort)
        .print(", from=")
        .print(srcAddrStr)
        .print(":")
        .print(srcPort)
        .print(", payload=")
        .hexDump(payload, payloadLen)
        .tagOn();
  #endif

    // call UDP packet handler
    if (hIncomingUDPPacket != NULL) {
        hIncomingUDPPacket(srcAddrStr, srcPort, dstPort, payload, payloadLen);
    }
}

void _dispatchCoAPMessage(const char *srcAddrStr, uint32_t srcAddrInt, uint16_t srcPort, uint16_t dstPort, const uint8_t *udpPayload, uint16_t udpPayloadLen) {
    uint8_t coapBuf[NET_UDP_PAYLOAD_MAX_LEN];
    
    // try to parse CoAP frame
    memcpy(coapBuf, udpPayload, udpPayloadLen);
    CoapPDU coap(coapBuf, udpPayloadLen);

    if (coap.validate() != 1) {
        return;
    }

  #ifdef NET_DBG_COAP_INCOMING
    int tokenLen = coap.getTokenLength();
    int payloadLen = coap.getPayloadLength();

    dbg
        .print("CoAP RECV")
        .tagOff()
        .print(", atPort=")
        .print(dstPort)
        .print(", from=")
        .print(srcAddrStr)
        .print(":")
        .print(srcPort)
        .print(", type=")
        .hexByte(coap.getType(), true, false)
        .print(", code=")
        .hexByte(coap.getCode(), true, false)
        .print(", mid=")
        .hexShort(coap.getMessageID(), true, false);
    
    if (tokenLen > 0) {
        dbg
            .print(", token=")
            .hexString(coap.getTokenPointer(), tokenLen, true, false);
    }

    if (payloadLen > 0) {
        dbg
            .print(", payload=")
            .hexDump(coap.getPayloadPointer(), payloadLen, 0, false);
    }

    dbg.println().tagOn();
  #endif  /* NET_DBG_COAP_INCOMING */

  #ifdef NET_COAP_AUTO_RESPONSE_CONFIRMABLE_MSG_WITH_EMPTY_ACK
    // send empty ACK back if needed
    if (coap.getType() == CoapPDU::COAP_CONFIRMABLE) {
        netSendCoAPEmptyAckMessage(srcAddrStr, srcPort, 0, coap.getMessageID());
    }
  #endif

  #ifdef NET_COAP_IGNORE_DUPLICATE_INCOMING_MSG_ID
    // ignore received frame with duplicate message id
    // but after it has been ack'ed
    if (netIsCoAPMessageIdDuplicate(srcAddrInt, srcPort, coap.getMessageID())) {
        return;
    }
  #endif

  #ifdef NET_COAP_IGNORE_INCOMING_EMPTY_ACK_MSG
    bool ignoreMessage = (coap.getType() == CoapPDU::COAP_ACKNOWLEDGEMENT) && (coap.getCode() == CoapPDU::COAP_EMPTY);
  #else
    bool ignoreMessage = false;
  #endif

    // call the CoAP frame handler
    if (!ignoreMessage && hIncomingCoAPMessage != NULL) {
        hIncomingCoAPMessage(srcAddrStr, srcPort, dstPort, &coap);
    }
}
