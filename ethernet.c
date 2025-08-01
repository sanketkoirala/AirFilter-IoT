//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL w/ ENC28J60
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// ENC28J60 Ethernet controller on SPI0
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS (SW controlled) on PA3
//   WOL on PB3
//   INT on PC6

// Pinning for IoT projects with wireless modules:
// N24L01+ RF transceiver
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS on PE0
//   INT on PB2
// Xbee module
//   DIN (UART1TX) on PC5
//   DOUT (UART1RX) on PC4

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "communication/I2C3.h"
#include "tm4c123gh6pm.h"
#include "clock/clock.h"
#include "project-framework/eeprom.h"
#include "gpio/gpio.h"
#include "communication/spi0.h"
#include "communication/uart0.h"
#include "clock/wait.h"
#include "clock/timer.h"
#include "project-framework/eth0.h"
#include "project-framework/arp.h"
#include "project-framework/ip.h"
#include "project-framework/icmp.h"
#include "protocol/udp.h"
#include "protocol/tcp.h"
#include "protocol/mqtt.h"
#include "project-framework/socket.h"
#include "sensor/bme680.h"

// Pins
#define RED_LED PORTF, 1
#define BLUE_LED PORTF, 2
#define GREEN_LED PORTF, 3
#define PUSH_BUTTON PORTF, 4
#define FAN PORTD, 3

// EEPROM Map
#define EEPROM_DHCP 1
#define EEPROM_IP 2
#define EEPROM_SUBNET_MASK 3
#define EEPROM_GATEWAY 4
#define EEPROM_DNS 5
#define EEPROM_TIME 6
#define EEPROM_MQTT 7
#define EEPROM_ERASED 0xFFFFFFFF

// global flags
bool ARP_SENT = false;
bool SYN_NEEDED = false;
bool printAirData = false;

// added flag to publish message is turned on every 15 sec by timer
volatile bool PUBLISH_MESSAGE = false;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// callback for timer added
void publishMessageCallback(void)
{
    PUBLISH_MESSAGE = true;
    return;
}

// Initialize Hardware
void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clocks
    enablePort(PORTF);
    enablePort(PORTD);
    _delay_cycles(3);

    // Configure LED and pushbutton pins
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(GREEN_LED);
    selectPinPushPullOutput(BLUE_LED);
    selectPinPushPullOutput(FAN);
    selectPinDigitalInput(PUSH_BUTTON);
    enablePinPullup(PUSH_BUTTON);
}

void displayConnectionInfo()
{
    uint8_t i;
    char str[20];
    uint8_t mac[6];
    uint8_t ip[4];
    getEtherMacAddress(mac);
    putsUart0("  HW:    ");
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%02" PRIu8, mac[i]);
        putsUart0(str);
        if (i < HW_ADD_LENGTH - 1)
            putcUart0(':');
    }
    putcUart0('\n');
    getIpAddress(ip);
    putsUart0("  IP:    ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%" PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    getIpSubnetMask(ip);
    putsUart0("  SN:    ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%" PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    getIpGatewayAddress(ip);
    putsUart0("  GW:    ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%" PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    getIpDnsAddress(ip);
    putsUart0("  DNS:   ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%" PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    getIpTimeServerAddress(ip);
    putsUart0("  Time:  ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%" PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    getIpMqttBrokerAddress(ip);
    putsUart0("  MQTT:  ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%" PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    if (isEtherLinkUp())
        putsUart0("  Link is up\n");
    else
        putsUart0("  Link is down\n");
}

void readConfiguration()
{
    uint32_t temp;
    uint8_t *ip;

    temp = readEeprom(EEPROM_IP);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t *)&temp;
        setIpAddress(ip);
    }
    temp = readEeprom(EEPROM_SUBNET_MASK);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t *)&temp;
        setIpSubnetMask(ip);
    }
    temp = readEeprom(EEPROM_GATEWAY);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t *)&temp;
        setIpGatewayAddress(ip);
    }
    temp = readEeprom(EEPROM_DNS);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t *)&temp;
        setIpDnsAddress(ip);
    }
    temp = readEeprom(EEPROM_TIME);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t *)&temp;
        setIpTimeServerAddress(ip);
    }
    temp = readEeprom(EEPROM_MQTT);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t *)&temp;
        setIpMqttBrokerAddress(ip);
    }
}

#define MAX_CHARS 80
char strInput[MAX_CHARS + 1];
char *token;
uint8_t count = 0;

uint8_t asciiToUint8(const char str[])
{
    uint8_t data;
    if (str[0] == '0' && tolower(str[1]) == 'x')
        sscanf(str, "%hhx", &data);
    else
        sscanf(str, "%hhu", &data);
    return data;
}

void processShell()
{
    bool end;
    char c;
    uint8_t i;
    uint8_t ip[IP_ADD_LENGTH];
    uint32_t *p32;
    char *topic, *data;

    if (kbhitUart0())
    {
        c = getcUart0();

        end = (c == 13) || (count == MAX_CHARS);
        if (!end)
        {
            if ((c == 8 || c == 127) && count > 0)
                count--;
            if (c >= ' ' && c < 127)
                strInput[count++] = c;
        }
        else
        {
            strInput[count] = '\0';
            count = 0;
            token = strtok(strInput, " ");
            if (strcmp(token, "mqtt") == 0)
            {
                token = strtok(NULL, " ");
                if (strcmp(token, "connect") == 0)
                {
                    connectMqtt(NULL);
                }
                if (strcmp(token, "disconnect") == 0)
                {
                    disconnectMqtt(NULL);
                }
                if (strcmp(token, "publish") == 0)
                {
                    topic = strtok(NULL, " ");
                    data = strtok(NULL, " ");
                    if (topic != NULL && data != NULL)
                        publishMqtt(topic, data);
                }
                if (strcmp(token, "subscribe") == 0)
                {
                    topic = strtok(NULL, " ");
                    if (topic != NULL)
                        subscribeMqtt(topic);
                }
                if (strcmp(token, "unsubscribe") == 0)
                {
                    topic = strtok(NULL, " ");
                    if (topic != NULL)
                        unsubscribeMqtt(topic);
                }
            }
            if (strcmp(token, "ip") == 0)
            {
                displayConnectionInfo();
            }
            if (strcmp(token, "airdata") == 0)
            {
                printAirData = true;
            }
            if (strcmp(token, "ping") == 0)
            {
                for (i = 0; i < IP_ADD_LENGTH; i++)
                {
                    token = strtok(NULL, " .");
                    ip[i] = asciiToUint8(token);
                }
                // removed from this version to save space: sendPingRequest(ip)
            }
            if (strcmp(token, "reboot") == 0)
            {
                NVIC_APINT_R = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ;
            }
            if (strcmp(token, "status") == 0)
            {
                socket *s = getSocket(0);
                s->PRINT_STATUS = true;
            }
            if (strcmp(token, "set") == 0)
            {
                token = strtok(NULL, " ");
                if (strcmp(token, "ip") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpAddress(ip);
                    p32 = (uint32_t *)ip;
                    writeEeprom(EEPROM_IP, *p32);
                }
                if (strcmp(token, "sn") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpSubnetMask(ip);
                    p32 = (uint32_t *)ip;
                    writeEeprom(EEPROM_SUBNET_MASK, *p32);
                }
                if (strcmp(token, "gw") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpGatewayAddress(ip);
                    p32 = (uint32_t *)ip;
                    writeEeprom(EEPROM_GATEWAY, *p32);
                }
                if (strcmp(token, "dns") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpDnsAddress(ip);
                    p32 = (uint32_t *)ip;
                    writeEeprom(EEPROM_DNS, *p32);
                }
                if (strcmp(token, "time") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpTimeServerAddress(ip);
                    p32 = (uint32_t *)ip;
                    writeEeprom(EEPROM_TIME, *p32);
                }
                if (strcmp(token, "mqtt") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpMqttBrokerAddress(ip);
                    p32 = (uint32_t *)ip;
                    writeEeprom(EEPROM_MQTT, *p32);
                }
            }

            if (strcmp(token, "help") == 0)
            {
                putsUart0("Commands:\n");
                putsUart0("  mqtt ACTION [USER [PASSWORD]]\n");
                putsUart0("    where ACTION = {connect|disconnect|publish TOPIC DATA\n");
                putsUart0("                   |subscribe TOPIC|unsubscribe TOPIC}\n");
                putsUart0("  ip\n");
                putsUart0("  ping w.x.y.z\n");
                putsUart0("  reboot\n");
                putsUart0("  set ip|gw|dns|time|mqtt|sn w.x.y.z\n");
            }
        }
    }
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

// Max packet is calculated as:
// Ether frame header (18) + Max MTU (1500)
#define MAX_PACKET_SIZE 1518

int main(void)
{
    uint8_t buffer[MAX_PACKET_SIZE];
    etherHeader *data = (etherHeader *)buffer;
    socket s;

    // Init controller
    initHw();

    // Setup UART0
    initUart0();
    setUart0BaudRate(115200, 40e6);

    // Init timer
    initTimer();

    // Init sockets
    initSockets();

    // init I2C
    initI2C3();

    // Init ethernet interface (eth0)
    putsUart0("\nStarting eth0\n");
    initEther(ETHER_UNICAST | ETHER_BROADCAST | ETHER_HALFDUPLEX);
    setEtherMacAddress(2, 3, 4, 5, 6, 120);

    // Init EEPROM
    initEeprom();
    readConfiguration();
    startbme();

    // start timer added
    if (!startPeriodicTimer(publishMessageCallback, 10))
    {
        putsUart0("Failed to start publish timer\n");
    }

    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(100000);

    // Main Loop
    // RTOS and interrupts would greatly improve this code,
    // but the goal here is simplicity
    /* Start-up check for stored IP address ********************************/
    uint8_t i = 0;
    uint8_t localAddress[4];
    uint8_t remoteAddress[4];
    getIpAddress(localAddress);
    getIpMqttBrokerAddress(remoteAddress);
    bool empty = true;
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        empty = remoteAddress[i] == 0;
    }
    if (!empty)
    {
        sendArpRequest(data, localAddress, remoteAddress);
    }
    /***********************************************************************/

    /*testing I2C for BME680 *******************************************/
    //    char buff[100];
    //    putsUart0("I2C test \n");
    //    uint8_t test = readDataFromRegI2C3(bme_add, id);
    //    snprintf(buff, 12, "ID: %d\n", test);
    //    putsUart0(buff);

    /*******************************************************************/
    while (true)
    {
        // Terminal processing here
        processShell();
#ifdef DEBUG
//        static int debugCounter = 0;
//            debugCounter++;
//            if(debugCounter > 100000)
//            {
//                debugCounter = 0;
//                updateBmeData();
//                printData();
//            }
#endif
        updateBmeData();
        if (printAirData)
        {
            printData();
            printAirData = false;
        }

        // TCP pending messages
        sendTcpPendingMessages(data);

        // Packet processing
        if (isEtherDataAvailable())
        {
            if (isEtherOverflow())
            {
                setPinValue(BLUE_LED, 1);
                waitMicrosecond(100000);
                setPinValue(BLUE_LED, 0);
            }

            // Get packet
            getEtherPacket(data, MAX_PACKET_SIZE);

            // Handle ARP request
            if (isArpRequest(data))
                sendArpResponse(data);

            // handle arp reponse
            if (isArpResponse(data))
                processTcpArpResponse(data);

            // Handle IP datagram
            if (isIp(data))
            {
                if (isIpUnicast(data))
                {
                    // Handle ICMP ping request
                    if (isPingRequest(data))
                    {
                        sendPingResponse(data);
                    }

                    // Handle TCP datagram
                    if (isTcp(data))
                    {
                        if (isTcpPortOpen(data))
                        {
                            processTcpResponse(data);
                        }
                        else
                            sendTcpResponse(data, &s, ACK | RST);
                    }
                }
            }
        }
    }
}
