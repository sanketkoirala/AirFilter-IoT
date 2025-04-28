// TCP Library (includes framework only)
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
#include "arp.h"
#include "tcp.h"
#include "timer.h"
#include "ip.h"
#include "socket.h"
#include "mqtt.h"
#include "gpio.h"
#include "uart0.h"
#include "bme680.h"

// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------

#define MAX_TCP_PORTS 4
#define MAX_SYNC_ATTEMPTS 4
#define RED_LED PORTF,1
#define BLUE_LED PORTF,2
#define GREEN_LED PORTF,3
#define PUSH_BUTTON PORTF,4
#define FAN PORTD,3

// added external flag for publishing message
extern volatile bool PUBLISH_MESSAGE;

uint16_t tcpPorts[MAX_TCP_PORTS];
uint8_t tcpPortCount = 0;
uint8_t tcpState[MAX_TCP_PORTS];
bool sendSynAckFlag;
//

extern char* sub_list[5];
extern uint8_t sub_count;
//extern volatile bmeDataRaw* bmeRaw;
//extern volatile bmeData_s* bmeData;
//extern volatile bmeCalibrationData* bmeCal;

// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Set TCP state
void setTcpState(uint8_t instance, uint8_t state)
{
    tcpState[instance] = state;
}

// Get TCP state
uint8_t getTcpState(uint8_t instance)
{
    return tcpState[instance];
}

// Determines whether packet is TCP packet
// Must be an IP packet
bool isTcp(etherHeader* ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*) ip + ipHeaderLength);
    uint16_t tcpLength;
    bool ok;
    uint16_t tmp16 = 0;
    uint32_t sum = 0;
    ok = (ip->protocol == PROTOCOL_TCP);
    if (ok)
    {
        // 32-bit sum over pseudo-header
        sumIpWords(ip->sourceIp, 8, &sum);
        tmp16 = ip->protocol;
        sum += (tmp16 & 0xff) << 8;

        // Compute TCP length
        tcpLength = htons(ntohs(ip->length) - ipHeaderLength);
        sumIpWords(&tcpLength, 2, &sum);

        // Add TCP header and data
        sumIpWords(tcp, ntohs(tcpLength), &sum);

        // Final checksum validation
        ok = (getIpChecksum(sum) == 0);
    }
    return ok;
}

bool isTcpSyn(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*) ip + ipHeaderLength);

    if (ip->protocol != PROTOCOL_TCP)
            return false;
    uint16_t flags = tcp->offsetFields & 0x0FFF;

    return ((flags & SYN));
}

bool isTcpAck(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*) ip + ipHeaderLength);

    if (ip->protocol != PROTOCOL_TCP)
            return false;
    uint16_t flags = tcp->offsetFields & 0x0FFF;

    return ((flags & ACK));
}

//takes the socket and extract the flags
//then prints the status of the statemachine and mqtt connection if needed
//depending on the state stored in tcpState, send required messages and drive state machine
//set sockets flags to 0

void sendTcpPendingMessages(etherHeader *ether)
{
    socket* s;
    uint8_t flags;

    //currently only support one socket; for loop is there in case i want to implement multiple later
//    for (i = 0; i < 10; i++)
//    {
        s = getSocket(0);
        flags = s->flags;
        if(s->PRINT_STATUS)
        {
            putsUart0("State Machine Status: ");
            switch(s->state)
            {
                case TCP_SYN_SENT:
                    putsUart0("TCP_SYN_SENT");
                    break;
                case TCP_ESTABLISHED:
                    putsUart0("TCP_ESTABLISHED");
                    break;
                case TCP_CLOSE_WAIT:
                    putsUart0("TCP_CLOSE_WAIT");
                    break;
                case TCP_CLOSED:
                    putsUart0("TCP_CLOSED");
            }
//            putsUart0(s->state);
            putcUart0('\n');
            putsUart0("MQTT Connection Status: ");
            if(s->MQTT_CONNECTED)
            {
                putsUart0("CONNECTED");
            }
            else
            {
                putsUart0("NOT CONNECTED");
            }

            putcUart0('\n');
            char buff[20];
            snprintf(buff, 30, "Number of Subscriptions: %d\n", sub_count);
            putsUart0(buff);
            s->PRINT_STATUS = false;
        }
        switch(tcpState[0])
        {
               case TCP_SYN_SENT:
                   if(flags & (SYN | ACK))
                   {
                       // Move to ESTABLISHED state
                       setTcpState(0, TCP_ESTABLISHED);
                       sendTcpMessage(ether, s, ACK, NULL, 0);
                       s->state = TCP_ESTABLISHED;
                       s->MQTT_CONNECT = true;
                   }
//                   if(flags & (RST | ACK))
//                   {
//                       setTcpState(0, TCP_CLOSED);
//                       s->state = TCP_CLOSED;
//                   }
                   break;
               case TCP_SYN_RECEIVED:
                   if (flags & ACK)
                   {
                       // Move to ESTABLISHED state
                       setTcpState(0, TCP_ESTABLISHED);
                   }
                   break;
               case TCP_ESTABLISHED:
                   // Handle data transfer or closing connection
                   if(s->FIN_NEEDED)
                   {
                       sendTcpMessage(ether, s, ACK, NULL, 0);
                       s->FIN_NEEDED = false;
                       setTcpState(0, TCP_CLOSE_WAIT);
                       s->state = TCP_CLOSE_WAIT;
                       break;
                   }
                   else if(flags & ACK)
                   {
                       sendTcpMessage(ether, s, ACK, NULL, 0);
                       break;
                   }
                   else if (flags & RST)
                   {
                       s->SYN_NEEDED = true;
                       setTcpState(0, TCP_CLOSED);
                       s->state = TCP_CLOSED;
                       break;
                   }
                   else if(s->MQTT_CONNECT)
                   {
                       connectMqtt(ether);
//                       subscribeMqtt("topic1");
                       s->MQTT_CONNECT = false;
                       break;
                   }
                   else if(s->MQTT_DISCONNECT)
                   {
                       disconnectMqtt(ether);
                       s->MQTT_DISCONNECT = false;
                       break;
                   }
                   else if(s->MQTT_PING)
                   {
                       uint8_t buffer[250];
                       mqttConnectHeader* m = (mqttConnectHeader*) buffer;
                       m->controlHeader = 0xC0;
                       m->remainLength = 0;
                       sendTcpMessage(ether, s, PSH | ACK, (uint8_t*) m, (m->remainLength + 2));
                       s->MQTT_PING = false;
                       break;
                   }
                   //added publish message if the flag is on
                   else if (PUBLISH_MESSAGE)
                   {
//                       PUBLISH_MESSAGE = false;
                       char buff2[20];
                       char* temp_;
//                       char temp[] = "bmeTemp";
//                       char hum[] = "bmeHum";
//                       char press[] = "bmePress";
                       char aqi[] = "bmeaqi";
                       bmeData_s* bmeData2 =  getBmeData();
                       temp_ = (char*)&aqi;
                       snprintf(buff2, 15, "%d", (int)bmeData2->AQI);
                       publishMqtt(temp_, buff2);
                       PUBLISH_MESSAGE = false;

                   }

                   break;
               case TCP_CLOSE_WAIT:
                   setTcpState(0, TCP_CLOSED);
                   s->state = TCP_CLOSED;
                   s->MQTT_CONNECTED = false;
                   sendTcpMessage(ether, s, FIN | ACK, NULL, 0);
                   s->localPort = 0;
                   tcpPorts[0] = 0;
                   tcpPortCount--;
                   s->flags = 0;
                   break;
               case TCP_CLOSED:
                   if(s->ARP_NEEDED)
                   {
                       uint8_t i = 0;
                       uint8_t localAddress[4];
                       uint8_t remoteAddress[4];
                       getIpAddress(localAddress);
                       getIpMqttBrokerAddress(remoteAddress);
                       bool empty = true;
                       for(i = 0; i < IP_ADD_LENGTH; i++)
                       {
                           empty = remoteAddress[i] == 0;
                       }
                       if(!empty)
                       {
                           sendArpRequest(ether, localAddress, remoteAddress);
                       }
                       s->ARP_NEEDED = false;
                       break;
                   }
                   else if(s->SYN_NEEDED)
                   {
                       //need more testing to make sure it can connect after a failed attempts
                       //set port if there is none
                       if(s->localPort == 0)
                       {
//                           s->ARP_NEEDED = true;
                           break;
                       }
                       sendTcpMessage(ether, s, SYN, NULL, 0);
                       bool started = startOneshotTimer(tcpSynTimeout, 10);
                       if(!started)
                       {
                           s->SYN_NEEDED = false;
                           return;
                       }
                       setTcpState(0, TCP_SYN_SENT);
                       s->SYN_NEEDED = false;
                   }
                   else if(s->MQTT_CONNECT)
                   {
                       s->ARP_NEEDED = true;
                       s->MQTT_CONNECT = false;
                   }
                   break;
//               default:
//                   sendTcpMessage(ether, s, ACK, NULL, 0);
            }
        s->flags = 0;
//    }
}

//extracts the flags from the tcp packet
//checks if port is open, if not we ignore the packet
//handle mqtt packets
//take the tcp flags and put them into the socket
//then depending on the flags, update the socket seqNum and AckNum
void processTcpResponse(etherHeader *ether)
{
    //variables
    uint16_t flags;
    socket *s = NULL;

    //grabbing ip and tcp headers
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*) ip + ipHeaderLength);

    //extract flags
    flags = ntohs(tcp->offsetFields) & 0x01FF;

    bool ok = isTcpPortOpen(ether);
    if(!ok)
    {
        return;
    }

    uint16_t tcpHeaderLen = ((tcp->offsetFields >> 4) & 0x0F) * 4;
    uint16_t ipLen = ntohs(ip->length);
    uint16_t tcpPayloadLen = ipLen - ipHeaderLength - tcpHeaderLen;

    //get socket info
    s = getSocket(0);
    //handling MQTT packets
    mqttFixedHeader* m = (mqttFixedHeader*)tcp->data;
    if(m->controlHeader == 0x20 && m != NULL)
    {
        uint8_t* ptr = (uint8_t*) m;
        ptr = ptr + 3;
        uint8_t returncode;
        memcpy(&returncode, ptr, 1);
        if(returncode == 0)
        {
            s->MQTT_CONNECTED = true;
        }
    }
    else if(m->controlHeader == 0x32 && m != NULL)
    {
        char buffer[100];
        uint8_t length = m->remainLength; // needs to be properly decoded
        uint8_t* ptr = m->variableHeader;
        uint8_t* ptr2;
        uint16_t topicLen;
        memcpy(&topicLen, ptr, 2);
        topicLen = ntohs(topicLen);
        ptr = ptr + 2; // move to topicname

        //variables for topic1
        memcpy(buffer, ptr, topicLen);
        char topic1[] = "6purifier";
        char on[] = "on";
        char off[] = "off";
        uint8_t* pon = (uint8_t*) &on;
        uint8_t* poff = (uint8_t*) &off;
        uint8_t* ptopic1 = (uint8_t*) &topic1;

//        putsUart0("Received: ");
//        putsUart0(topic1);
//        putcUart0('\n');
        ptr2 = ptr + strlen(topic1) + 2; //move over topic and message id
        //variables for topic2
        char topic2[] = "topic2";
        uint8_t* ptopic2 = (uint8_t*) topic2;

        //checking if it is topic1
        if(!memcmp(ptopic1, ptr, strlen(topic1)))
        {
            putsUart0("Received: ");
            putsUart0(topic1);
            putcUart0('\n');
            ptr = ptr + strlen(topic1) + 2; //move over topic and message id
            if(!memcmp(pon, ptr2, strlen(on)))
            {
                setPinValue(BLUE_LED, 1);
                setPinValue(FAN, 1);
            }
            else if(!memcmp(poff, ptr2, strlen(on)))
            {
                setPinValue(BLUE_LED, 0);
                setPinValue(FAN, 0);
            }
        }

        //checking if it is topic2
        else if(!memcmp(ptopic2, ptr, strlen(topic2)))
        {
            putsUart0("Received: ");
            putsUart0(topic2);
            putcUart0('\n');
            uint8_t msgLen = length - 2 - 2 - topicLen; //remaininglength - messageId - topiclengthfield - length of topicname
            ptr = ptr + topicLen + 2;
            char msg[50];
            memcpy(&msg, ptr, msgLen);
            msg[msgLen] = '\0';
            putsUart0(msg);
            putcUart0('\n');

        }
    }

    //put flags into socket
    s->flags = flags;
    if(flags & RST)
    {
        s->flags = 0;
        s->SYN_NEEDED = true;
        return;
    }
//    else if((flags & ACK) & (s->state == TCP_LAST_ACK))
//    {
//        setTcpState(0, TCP_CLOSED);
//        s->state = TCP_CLOSED;
//    }
    else if(flags & FIN)
    {
        s->acknowledgementNumber = ntohl(tcp->sequenceNumber) + 1;
        s->FIN_NEEDED = true;
    }
    else if(flags & PSH)
    {
        s->acknowledgementNumber = ntohl(tcp->sequenceNumber) + tcpPayloadLen;
    }
    else if(flags & (SYN))
    {
        bool found = stopTimer(tcpSynTimeout);
        if(!found)
        {
//            uint8_t i =0;
        }
        s->acknowledgementNumber = ntohl(tcp->sequenceNumber) + 1;
    }
    else
        s->acknowledgementNumber = ntohl(tcp->sequenceNumber) + tcpPayloadLen;
}

//takes an arp packet and checks if it is from the mqtt ip address
//then creates a new socket and fills in the information
//then sets the SYN_NEEDED flag to true so a sync is sent
void processTcpArpResponse(etherHeader *ether)
{
    arpPacket *arp = (arpPacket*)ether->data;
    bool ok = false;
    uint8_t i = 0;
    uint8_t mqttIpAddress[IP_ADD_LENGTH];
    getIpMqttBrokerAddress(mqttIpAddress);
    for(i = 0; i < IP_ADD_LENGTH; i++)
    {
        ok = mqttIpAddress[i] == arp->sourceIp[i];
    }
    if(!ok)
        return;
    //create new socket to store info
    socket* s = newSocket();
    //get info from response and store into socket
    getSocketInfoFromArpResponse(ether, s);
    s->localPort = (random32() % (60000 - 50000 + 1)) + 50000;
    tcpPorts[0] = s->localPort;
    tcpPortCount++;
    s->remotePort = 1883;
    s->sequenceNumber = 0;
    s->SYN_NEEDED = true;
}

void setTcpPortList(uint16_t ports[], uint8_t count)
{
    uint8_t i = 0;
    for(i = 0; i < count; i++)
    {
        ports[i] = 0;
    }
    return;
}

bool isTcpPortOpen(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*) ip + ipHeaderLength);

    uint8_t i = 0;
    bool ok = false;
    if (ip->protocol != PROTOCOL_TCP)
            return false;
    for(i = 0; i < MAX_TCP_PORTS; i++)
    {
        if(tcp->destPort == tcpPorts[i])
            ok = true;
    }
    return ok;
}

void sendTcpResponse(etherHeader *ether, socket* s, uint16_t flags)
{
}

// Send TCP message
void sendTcpMessage(etherHeader *ether, socket *s, uint16_t flags, uint8_t data[], uint16_t dataSize)
{
    uint8_t i;
    uint16_t j;
    uint32_t sum;
    uint16_t tmp16;
    uint8_t tcpHeaderLength;
    uint16_t tcpLength;
    uint8_t *copyData;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];

    // Ether frame
    getEtherMacAddress(localHwAddress);
    getIpAddress(localIpAddress);
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = s->remoteHwAddress[i];
        ether->sourceAddress[i] = localHwAddress[i];
    }
    ether->frameType = htons(TYPE_IP);

    // IP header
    ipHeader* ip = (ipHeader*)ether->data;
    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
//    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = s->remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeaderLength = sizeof(tcpHeader);
    tcpLength = tcpHeaderLength + dataSize;
    ip->length = htons(ipHeaderLength + tcpLength);
    calcIpChecksum(ip);
    //TCP header
    tcpHeader* tcp = (tcpHeader*)((uint8_t*) ip + (ip->size * 4));
    tcp->sourcePort = htons(s->localPort);
    tcp->destPort = htons(s->remotePort);
    tcp->offsetFields = htons(flags | (tcpHeaderLength >> 2) << 12);

    if(flags & SYN)
    {
        tcp->sequenceNumber = 0;
        tcp->acknowledgementNumber = 0;
        s->sequenceNumber = 1;
    }
    else if (flags & (ACK | PSH))
    {
          tcp->sequenceNumber = htonl(s->sequenceNumber);
          tcp->acknowledgementNumber = htonl(s->acknowledgementNumber);
          s->sequenceNumber += dataSize;
        }
    else if (flags & ACK)
    {
        tcp->sequenceNumber = htonl(s->sequenceNumber);  // Use stored sequence number
        tcp->acknowledgementNumber = htonl(s->acknowledgementNumber);
    }

    tcp->windowSize = htons(1500);
    tcp->urgentPointer = 0;
    tcp->checksum = 0;
    copyData = tcp->data;
    for (j = 0; j < dataSize; j++)
        copyData[j] = data[j];
//    tcp->acknowledgementNumber = htonl(tcp->acknowledgementNumber);

    //calculation of checksum
    sum = 0;
    sumIpWords(ip->sourceIp, 8, &sum);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    uint16_t tmp = htons(tcpLength);
    sumIpWords(&tmp, 2, &sum);
    sumIpWords(tcp, tcpLength, &sum);
    tcp->checksum = getIpChecksum(sum);

    if(!isTcpPortOpen(ether))
    {
//        for(i = 0; i < MAX_TCP_PORTS; i++)
//        {
//            if (tcpPorts[i] == 0)
//            {
                tcpPorts[0] = tcp->sourcePort;
                tcpPortCount++;
//                break;
//            }
//        }
    }
    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);
}

void tcpSynTimeout()
{
    socket* s = getSocket(0);
    s->syncAttempts++;
    if(s->syncAttempts > MAX_SYNC_ATTEMPTS + 1)
    {
        s->localPort = 0;
        return;
    }
    else
    {
        bool started = startOneshotTimer(tcpSynTimeout, 10);
        if(!started)
        {
            return;
        }
        s->SYN_NEEDED = true;
    }
    return;
}
