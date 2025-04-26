// MQTT Library (includes framework only)
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: -
// Target uC:       -
// System Clock:    -

// Hardware configuration:
// -

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include "mqtt.h"
#include "timer.h"
#include "uart0.h"

// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------

uint8_t KEEP_ALIVE;
char* sub_list[5];
uint8_t sub_count = 0;
// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void connectMqtt(etherHeader* ether)
{
    if(ether == NULL)
    {
        socket* s = getSocket(0);
        s->MQTT_CONNECT = true;
        return;
    }
    uint8_t buffer[250];
    mqttConnectHeader* m = (mqttConnectHeader*) buffer;
    m->controlHeader = 0x10;
    m->remainLength = 0;
    m->protocolNameLen = htons(4);
    m->protocolLevel = 0x04;
    m->flags = 0x02; // connect flags
    m->keepAliveTime = htons(30);
    KEEP_ALIVE = 30;
    bool started = startPeriodicTimer(mqttPing, KEEP_ALIVE);
    if(!started)
    {
        putsUart0("Failed to Start Timer\n");
        return;
    }

    m->protocolName[0] = 'M';
    m->protocolName[1] = 'Q';
    m->protocolName[2] = 'T';
    m->protocolName[3] = 'T';
    uint8_t client[] = {"client-001"};

    mqttConnect* c = (mqttConnect*) m->data;

    uint8_t* ptr = (uint8_t*) c;

    // Client ID
    uint16_t clientLen = htons(sizeof(client) - 1);
    memcpy(ptr, &clientLen, 2);
    ptr += 2;
    memcpy(ptr, client, sizeof(client) - 1);
    ptr += sizeof(client) - 1;

    socket* s = getSocket(0);
    // checking if port is open
    if(s->localPort == 0)
    {
        //port is closed
        s->localPort = (random32() % (60000 - 50000 + 1)) + 50000;
        s->sequenceNumber = 0;
        s->SYN_NEEDED = true;
        s->MQTT_CONNECT = true;
        return;
    }
    uint16_t varHeaderSize = 2 + 4 + 1 + 1 + 2; // Protocol name len, protocol name, level, flags, keep alive
    uint16_t payloadSize = 2 + (sizeof(client) - 1);
    m->remainLength = varHeaderSize + payloadSize;
    sendTcpMessage(ether, s, PSH | ACK, (uint8_t*) m, (m->remainLength + 2));
}

void disconnectMqtt(etherHeader* ether)
{
    if(ether == NULL)
    {
        socket* s = getSocket(0);
        s->MQTT_DISCONNECT = true;
        return;
    }
    uint8_t buffer[250];
    mqttFixedHeader* m = (mqttFixedHeader*) buffer;
    m->controlHeader = 0xE0;
    m->remainLength = 0;

    bool stopped = stopTimer(mqttPing);
    if(!stopped)
    {
        return;
    }
    socket* s = getSocket(0);
    sendTcpMessage(ether, s, PSH | ACK, (uint8_t*) m, (m->remainLength + 2));
}

void publishMqtt(char strTopic[], char strData[])
{
    uint8_t etherBuffer[1518];
    uint8_t buffer[250];
    memset(buffer, 0, sizeof(buffer));
    mqttFixedHeader* m = (mqttFixedHeader*) buffer;
    etherHeader* ether = (etherHeader*) etherBuffer;
    m->controlHeader = 0x32;
    m->remainLength = 0;
    mqttPublishHeader* p = (mqttPublishHeader*) m->variableHeader;
    uint16_t packetId = htons((random32() % (65000 - 0 + 1)) + 0);
    uint16_t topicLen = strlen(strTopic);
    p->topicNameLen = htons(topicLen);
    //copy topicname
    memcpy(p->data, strTopic, topicLen);

    //copy packetId after topicname
    uint8_t* packetIdptr = (uint8_t *) (p->data + topicLen);
    memcpy(packetIdptr, &packetId, 2);

    //copy payload after the packetId
    uint16_t dataLen = strlen(strData);
    uint8_t* dataPtr = (uint8_t*) (p->data + topicLen + 2);
    memcpy(dataPtr, strData, dataLen);
    m->remainLength = 2 + topicLen + dataLen + 2;

    socket* s = getSocket(0);

    sendTcpMessage(ether, s, PSH | ACK, (uint8_t*) m, (m->remainLength + 2));

}

void subscribeMqtt(char strTopic[])
{
    uint8_t etherBuffer[1518];
    uint8_t buffer[250];
    mqttFixedHeader* m = (mqttFixedHeader*) buffer;
    etherHeader* ether = (etherHeader*) etherBuffer;
    m->controlHeader = 0x82;
    m->remainLength = 0;
    mqttSubscribeHeader* p = (mqttSubscribeHeader*) m->variableHeader;
    p->packetId = (random32() % (65000 - 0 + 1)) + 0;
    uint16_t topicLen = strlen(strTopic);
    //handling topic
    p->topicNameLen = htons(topicLen);
    sub_list[sub_count++] = strTopic;
    memcpy(p->topicName, strTopic, topicLen);
    //handling qos
    uint8_t qos = 2;
    uint8_t* qosPtr = (uint8_t*) (p->topicName + topicLen);
    memcpy(qosPtr, &qos, 1);

    m->remainLength = 2 + 2 + topicLen + 1;
    socket* s = getSocket(0);

    sendTcpMessage(ether, s, PSH | ACK, (uint8_t*) m, (m->remainLength + 2));
}

void unsubscribeMqtt(char strTopic[])
{
    uint8_t etherBuffer[1518];
    uint8_t buffer[250];
    uint8_t i, j;
    mqttFixedHeader* m = (mqttFixedHeader*) buffer;
    etherHeader* ether = (etherHeader*) etherBuffer;
    m->controlHeader = 0xA2;
    m->remainLength = 0;
    mqttSubscribeHeader* p = (mqttSubscribeHeader*) m->variableHeader;
    p->packetId = (random32() % (65000 - 0 + 1)) + 0;
    uint16_t topicLen = strlen(strTopic);
    for (i = 0; i < sub_count; i++)
    {
        if(!memcmp(strTopic, sub_list[i], strlen(strTopic)+1))
        {
            for(j = i; j < sub_count - 1; j++)
            {
                sub_list[j] = sub_list[j+1];
            }
            sub_count--;
        }
    }
    //handling topic
    p->topicNameLen = htons(topicLen);
    memcpy(p->topicName, strTopic, topicLen);

    m->remainLength = 2 + 2 + topicLen;
    socket* s = getSocket(0);

    sendTcpMessage(ether, s, PSH | ACK, (uint8_t*) m, (m->remainLength + 2));
}

void mqttPing()
{

    socket* s = getSocket(0);
    if(s->localPort == 0)
    {
        stopTimer(mqttPing);
        return;
    }
    s->MQTT_PING = true;
}
