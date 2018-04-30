/**
 * Network adaptation layer for Quectel BC95 modem.
 * 
 * Copyright (c) 2018 Sparkbit Co., Ltd. All rights reserved.
 * 
 * This work is licensed under the terms of the MIT license.  
 * See LICENSE file in the project root for details.
 */

#ifndef TP_NETWORK_H
#define TP_NETWORK_H

#include "coap/cantcoap.h"
#include "quectel_bc95.h"

// ----------------------------------------
//   Debugging Switches
// ----------------------------------------
#define NET_DBG_INIT_NETWORK
// #define NET_DBG_VERBOSE_MODEM_INFO
// #define NET_DBG_VERBOSE_NETWORK_INFO
// #define NET_DBG_UDP_OUTGOING
// #define NET_DBG_UDP_INCOMING
// #define NET_DBG_COAP_OUTGOING
// #define NET_DBG_COAP_INCOMING
// #define NET_DBG_COAP_PING
// TODO implement logging for CoAP message ID table
// #define NET_DBG_COAP_MSG_ID_TABLE
// #define NET_DBG_COAP_MSG_ID_STATUS
// ----------------------------------------

#define NET_MODEM_SERIAL_BAUD  9600

#define NET_MODEM_RESET_PIN      4
#define NET_MODEM_RESET_TIMEOUT  10000

#define NET_DEFAULT_SOCKET_LOCAL_PORT  56830

// 2 minutes
#define NET_DEFAULT_INIT_NETWORK_TIMEOUT  120000

#if defined(__SAM3X8E__) || defined(__SAMD21G18A__) || defined(ESP32)
    #define NET_UDP_PAYLOAD_MAX_LEN  512
#elif defined (__AVR_ATmega2560__)
    #define NET_UDP_PAYLOAD_MAX_LEN  256
#else
    #define NET_UDP_PAYLOAD_MAX_LEN  100
#endif

// enable/disable incoming CoAP message processing
#define NET_PROCESS_COAP_INCOMING_MESSAGE

// automatically respose incoming confirmable messages with empty ACK
#define NET_COAP_AUTO_RESPONSE_CONFIRMABLE_MSG_WITH_EMPTY_ACK

// automatically ignore incoming message with duplicate message ID
#define NET_COAP_IGNORE_DUPLICATE_INCOMING_MSG_ID

// don't send empty ACK message to netSetIncomingCoAPMessageHandler
#define NET_COAP_IGNORE_INCOMING_EMPTY_ACK_MSG

#ifdef NET_COAP_IGNORE_DUPLICATE_INCOMING_MSG_ID
    #define NET_COAP_RECEIVED_MSG_ID_ENTRY_TIMEOUT  30000

  #if defined(__SAM3X8E__) || defined(__SAMD21G18A__) || defined(ESP32)
    #define NET_COAP_RECEIVED_MSG_ID_LIST_LEN  10
  #elif defined (__AVR_ATmega2560__)
    #define NET_COAP_RECEIVED_MSG_ID_LIST_LEN  5
  #else
    #define NET_COAP_RECEIVED_MSG_ID_LIST_LEN  3
  #endif

#endif


QuectelBC95::Modem *netGetModem();

void netInit();
bool netInitNetwork();
bool netIsNetworkReady();

bool netPingHost(const char *ipAddress, unsigned long timeout);

bool netSendUDPPacket(const char *dstAddrStr, uint16_t dstPort, uint16_t srcPort, const uint8_t *payload, uint16_t payloadLen);

uint16_t netGetNextCoAPMessageId();
void netGetRandomCoAPToken(uint8_t *buf, size_t len);
bool netIsCoAPMessageIdDuplicate(uint32_t srcAddress, uint16_t srcPort, uint16_t messageId);

bool netSendCoAPMessage(const char *dstAddrStr, uint16_t dstPort, CoapPDU *message);
bool netSendCoAPMessage(const char *dstAddrStr, uint16_t dstPort, uint16_t srcPort, CoapPDU *message);
bool netSendCoAPEmptyAckMessage(const char *dstAddrStr, uint16_t dstPort, uint16_t messageId);
bool netSendCoAPEmptyAckMessage(const char *dstAddrStr, uint16_t dstPort, uint16_t srcPort, uint16_t messageId);
bool netSendCoAPResetMessage(const char *dstAddrStr, uint16_t dstPort, uint16_t messageId);
bool netSendCoAPResetMessage(const char *dstAddrStr, uint16_t dstPort, uint16_t srcPort, uint16_t messageId);
bool netSendCoAPPing(const char *dstAddrStr, uint16_t dstPort, unsigned long timeout);

void netSetIncomingUDPPacketHandler(void (*handler)(const char *srcAddrStr, uint16_t srcPort, uint16_t dstPort, const uint8_t *payload, uint16_t payloadLen));
void netSetIncomingCoAPMessageHandler(void (*handler)(const char *srcAddrStr, uint16_t srcPort, uint16_t dstPort, CoapPDU *message));

void netTaskTick();

#endif  /* TP_NETWORK_H */