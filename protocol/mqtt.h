
#ifndef MQTT_H_
#define MQTT_H_

#include <stdint.h>
#include <stdbool.h>
#include "tcp.h"

typedef struct _mqttFixedHeader
{
    uint8_t controlHeader;
    uint8_t remainLength;
    uint8_t variableHeader[0];
} mqttFixedHeader;

typedef struct _mqttConnectHeader // 20 or more bytes
{
    uint8_t controlHeader;
    uint8_t remainLength;
    uint16_t protocolNameLen;
    uint8_t protocolName[4];
    uint8_t protocolLevel;
    uint8_t flags; // connect flags
    uint16_t keepAliveTime;
    uint8_t data[0];
} mqttConnectHeader;

typedef struct _mqttConnect
{
    uint16_t clientIdLen;
    uint8_t clientId[0];
    uint16_t usernameLen;
    uint8_t username[0];
    uint16_t passwordLen;
    uint8_t password[0];
} mqttConnect;

typedef struct _mqttPublishHeader
{
    uint16_t topicNameLen;
    uint8_t data[0];
} mqttPublishHeader;

typedef struct _mqttSubscribeHeader
{
    uint16_t packetId;
    uint16_t topicNameLen;
    char topicName[0];
} mqttSubscribeHeader;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void connectMqtt(etherHeader *ether);
void disconnectMqtt(etherHeader *ether);
void publishMqtt(char strTopic[], char strData[]);
void subscribeMqtt(char strTopic[]);
void unsubscribeMqtt(char strTopic[]);
void mqttPing();

#endif
