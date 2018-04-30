/**
 * Quectel BC95 modem driver, tested with B656 firmware.
 * 
 * Copyright (c) 2018 Sparkbit Co., Ltd. All rights reserved.
 * 
 * This work is licensed under the terms of the MIT license.  
 * See LICENSE file in the project root for details.
 */

#ifndef QUECTEL_BC95_H
#define QUECTEL_BC95_H

#include <Arduino.h>

// ----------------------------------------
//   Debugging Switches
// ----------------------------------------
// #define BC95_DBG_WRITE_FRAME
// #define BC95_DBG_READ_FRAME
// #define BC95_DBG_READ_TIMEOUT
// ----------------------------------------

#define BC95_DEFAULT_STREAM_READ_TIMEOUT    100
#define BC95_DEFAULT_READ_RESPONSE_TIMEOUT  100
#define BC95_DEFAULT_CFUN_RESPONSE_TIMEOUT  10000
#define BC95_DEFAULT_PING_TIMEOUT           5000
#define BC95_DEFAULT_REBOOT_TIMEOUT         10000

// minimum length that can receive +CME ERROR: message
#define BC95_MIN_RSP_BUF_LEN  16

// BC95::Modem::readResponse() return values
#define BC95_RESPONSE_TYPE_DATA     0
#define BC95_RESPONSE_TYPE_OK       1
#define BC95_RESPONSE_TYPE_ERROR    2
#define BC95_RESPONSE_TYPE_TIMEOUT  3
#define BC95_RESPONSE_TYPE_UNKNOWN  4

// EPS Network Registration Status
#define BC95_NETWORK_STAT_NOT_REGISTERED                         0
#define BC95_NETWORK_STAT_REGISTERED                             1
#define BC95_NETWORK_STAT_SEARCHING                              2
#define BC95_NETWORK_STAT_REGISTRATION_DENIED                    3
#define BC95_NETWORK_STAT_UNKNOWN                                4
#define BC95_NETWORK_STAT_REGISTERED_ROAMING                     5
#define BC95_NETWORK_STAT_REGISTERED_SMS_ONLY                    6
#define BC95_NETWORK_STAT_REGISTERED_SMS_ROAMING                 7
#define BC95_NETWORK_STAT_REGISTERED_EMERGENCY                   8
#define BC95_NETWORK_STAT_REGISTERED_CSFB_NOT_PREFERRED          9
#define BC95_NETWORK_STAT_REGISTERED_CSFB_NOT_PREFERRED_ROAMING  10

// CSCON mode
#define BC95_CSCON_MODE_IDLE       0
#define BC95_CSCON_MODE_CONNECTED  1

// CSCON state
#define BC95_CSCON_STATE_UTRAN_URA_PCH          0
#define BC95_CSCON_STATE_UTRAN_CELL_PCH         1
#define BC95_CSCON_STATE_UTRAN_CELL_FACH        2
#define BC95_CSCON_STATE_UTRAN_CELL_DCH         3
#define BC95_CSCON_STATE_GERAN_CS_CONNECTED     4
#define BC95_CSCON_STATE_GERAN_PS_CONNECTED     5
#define BC95_CSCON_STATE_GERAN_CS_PS_CONNECTED  6
#define BC95_CSCON_STATE_E_UTRAN_CONNECTED      7

// CFUN level
#define BC95_CFUN_MINIMUM  0
#define BC95_CFUN_FULL     1

// NSOST max data length
#define BC95_NSOST_MAX_DATA_LEN  512

// NSOST flags
#define BC95_NSOST_FLAG_NONE                    0
#define BC95_NSOST_FLAG_HIGH_PRIORITY           0x100
#define BC95_NSOST_FLAG_RELEASE_AFTER_NEXT_MSG  0x200
#define BC95_NSOST_FLAG_RELEASE_AFTER_REPLIED   0x400

// NSORF receiving chunk
#define BC95_NSORF_CHUNK_LEN      32
#define BC95_NSORF_CHUNK_BUF_LEN  (32 + (BC95_NSORF_CHUNK_LEN * 2))

namespace QuectelBC95 {

typedef struct {
    uint8_t urc;
    uint8_t status;
} cereg_t;

typedef struct {
    uint8_t urc;
    uint8_t mode;
    uint8_t state;
    uint8_t access;
} cscon_t;

typedef struct {
    struct {
        uint8_t value;
        int16_t dBm;
    } rssi;

    uint8_t ber;
} csq_t;

typedef struct {
    uint32_t intVal;
    char strVal[16];
} ipv4_addr_t;

typedef struct {
    uint8_t cid;
    char type[32];
    char apn[96];
    ipv4_addr_t addr;
    bool dataComp;
    bool headerComp;
} pdn_info_t;

typedef struct {
    uint8_t cid;
    ipv4_addr_t addr;
} pdp_addr_t;

typedef struct {
    uint8_t mode;
    uint8_t format;
    char oper[24];
    uint8_t status;
    uint8_t accessTech;
} cops_t;

typedef struct {
    ipv4_addr_t addr;
    uint16_t ttl;
    uint16_t rtt;
} ping_response_t;

typedef struct {
    uint8_t socket;
    uint8_t *dataBuf;
    size_t dataLen;
    ipv4_addr_t remoteAddr;
    uint16_t remotePort;
} udp_rx_data_t;

class Modem {
    private:
        enum class ParserState {
            StartCR,
            StartLF,
            Payload,
            StopLF
        };

        Stream *_stream;

        size_t _sendUDPDatagram(uint8_t socket, const char *remoteHost, uint16_t remotePort, uint16_t flag, const uint8_t *dataBuf, size_t dataLen);
    
    public:
        Modem(Stream *stream);

        void writeCommand(const char *command);
        int readResponse(char *rspBuf, size_t rspBufLen, size_t *rspLen = NULL, unsigned long timeout = BC95_DEFAULT_READ_RESPONSE_TIMEOUT);
        bool readSimpleDataResponse(char *rspBuf, size_t rspBufLen, size_t *rspLen = NULL, unsigned long timeout = BC95_DEFAULT_READ_RESPONSE_TIMEOUT);
        bool waitForOK(unsigned long timeout = BC95_DEFAULT_READ_RESPONSE_TIMEOUT);

        // AT
        bool pingModem();

        // ATI
        // ----- Not Implemented -----
        // ATE
        bool setCommandEcho(bool enabled);
        // AT+CGMI
        bool readManufacturerIdentification(char *rspBuf, size_t rspBufLen);
        // AT+CGMM
        bool readModelIdentification(char *rspBuf, size_t rspBufLen);
        // AT+CGMR
        bool readRevisionIdentification(char *rspBuf, size_t rspBufLen);
        // AT+CGSN=1 (IMEI)
        bool readInternationalMobileStationEquipmentIdentity(char *rspBuf, size_t rspBufLen);
        // AT+CEREG?
        bool readNetworkRegistrationStatus(cereg_t *rsp);
        uint8_t readNetworkRegistrationStatus();
        // AT+CSCON
        bool readRadioConnectionStatus(cscon_t *rsp);
        uint8_t readRadioConnectionStatus();
        // AT+CLAC
        // ----- Not Implemented -----
        // AT+CSQ
        bool readSignalQuality(csq_t *rsp);
        // AT+CGPADDR=<cid>
        bool readPDPAddress(uint8_t cid, pdp_addr_t *rsp);
        // AT+COPS
        bool readPLMNSelection(cops_t *rsp);
        // AT+CGATT=<state> - PS attach or detach
        bool isPSAttached();  // AT+CGATT? - Read PS state
        bool attachPS();  // AT+CGATT=1 - Attach PS
        bool detachPS();  // AT+CGATT=0 - Detach PS
        // AT+CGACT
        // ----- Not Implemented -----
        // AT+CIMI
        bool readInternationalMobileSubscriberIdentity(char *rspBuf, size_t rspBufLen);
        // AT+CGDCONT?
        uint8_t readPDNConnectionInfo(pdn_info_t rsp[], uint8_t rspMaxLen);
        // AT+CFUN
        bool setPhoneFunctionality(uint8_t level, unsigned long timeout = BC95_DEFAULT_CFUN_RESPONSE_TIMEOUT);
        // AT+CMEE=<n>
        bool setErrorResponseFormat(uint8_t n);
        // AT+CCLK
        // ----- Not Implemented -----
        // AT+CPSMS
        // ----- Not Implemented -----
        // AT+CEDRXS
        // ----- Not Implemented -----
        // AT+CEER
        // ----- Not Implemented -----
        // AT+CEDRXRDP
        // ----- Not Implemented -----
        // AT+CTZR
        // ----- Not Implemented -----
        // AT+CIPCA
        // ----- Not Implemented -----
        // AT+CGAPNRC
        // ----- Not Implemented -----

        // AT+CSMS
        // ----- Not Implemented -----
        // AT+CNMA
        // ----- Not Implemented -----
        // AT+CSCA
        // ----- Not Implemented -----
        // AT+CMGS
        // ----- Not Implemented -----
        // AT+CMGC
        // ----- Not Implemented -----
        // AT+CSODCP
        // ----- Not Implemented -----
        // AT+CRTDCP
        // ----- Not Implemented -----
        
        // AT+NRB - Reboot the modem
        bool reboot(bool waitUntilFinished = true);
        // AT+NUESTATS
        // ----- Not Implemented -----
        // AT+NEARFCN
        // ----- Not Implemented -----
        // AT+NSOCR=<type>,<protocol>,<listen port>[,<receive control>] - Create a socket
        // For BC95, only type=DGRAM and protocol=17 are supported.
        int8_t createSocket(uint16_t port, bool recvMsg = true);
        // AT+NSOST=<socket>,<remote_addr>,<remote_port>,<length>,<data> - Send UDP datagram
        // AT+NSOSTF=<socket>,<remote_addr>,<remote_port>,<flag>,<length>,<data> - Send UDP datagram with flags
        size_t sendUDPDatagram(uint8_t socket, const char *remoteHost, uint16_t remotePort, const char *msg);
        size_t sendUDPDatagram(uint8_t socket, const char *remoteHost, uint16_t remotePort, const uint8_t *dataBuf, size_t dataLen);
        // AT+NSORF=<socket>,<req_length> - Receive UDP datagram
        size_t receiveUDPDatagram(uint8_t socket, uint8_t *dataBuf, size_t dataBufLen, udp_rx_data_t *rsp);
        // AT+NSOCL=<socket> - Close a socket
        bool closeSocket(uint8_t socket);
        // +NSONMI
        // ----- Not Implemented -----
        // AT+NPING=<ip>,<p_size>,<timeout>
        bool pingHost(const char *ipAddressStr, ping_response_t *rsp, unsigned long timeout = BC95_DEFAULT_PING_TIMEOUT);
        // AT+NBAND
        // ----- Not Implemented -----
        // AT+NLOGLEVEL
        // ----- Not Implemented -----
        // AT+NCONFIG
        bool configAutoConnect(bool enabled);
        // AT+NATSPEED
        // ----- Not Implemented -----
        // AT+NCCID
        // ----- Not Implemented -----
        // AT+NFWUPD
        // ----- Not Implemented -----
        // AT+NPOWERCLASS
        // ----- Not Implemented -----
        // AT+NPSMR
        // ----- Not Implemented -----
        // AT+NPTWEDRXS
        // ----- Not Implemented -----
};

}  // namespace QuectelBC95

#endif /* QUECTEL_BC95_H */