#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "simplelink.h"
#include "hw_types.h"
#include "hw_ints.h"
#include "interrupt.h"
#include "utils.h"
#include "uart.h"
#include "hw_memmap.h"
#include "prcm.h"
#include "rom.h"
#include "rom_map.h"
//Common interface includes
#include "common.h"
#ifndef NOTERM
#include "uart_if.h"
#endif
#include "pinmux.h"

#define APPLICATION_VERSION     "1.1.1"
#define PREAMBLE            1        /* Preamble value 0- short, 1- long */
#define CPU_CYCLES_1MSEC (80*1000)


#define flag_function 1//1: SINK, 2: SOURCE
#define flag_channel 2
#define flag_rate 5
#define flag_packets 1
#define flag_power 5
int iSoc;
#define APPLICATION_NAME        (flag_function==1?"SINK MODE":"SOURCE MODE")

// Application specific status/error codes
typedef enum
{
    // Choosing -0x7D0 to avoid overlap w/ host-driver's error codes
    TX_CONTINUOUS_FAILED = -0x7D0,
    RX_STATISTICS_FAILED = TX_CONTINUOUS_FAILED - 1,
    DEVICE_NOT_IN_STATION_MODE = RX_STATISTICS_FAILED - 1,
    STATUS_CODE_MAX = -0xBB8
} e_AppStatusCodes;
typedef struct
{
    int choice;
    int channel;
    int packets;
    SlRateIndex_e rate;
    int Txpower;
    int Message;
} UserIn;
volatile unsigned long g_ulStatus = 0; //SimpleLink Status
unsigned long g_ulGatewayIP = 0; //Network Gateway IP address
unsigned char g_ucConnectionSSID[SSID_LEN_MAX + 1]; //Connection SSID
unsigned char g_ucConnectionBSSID[BSSID_LEN_MAX]; //Connection BSSID
unsigned char macAddressVal[SL_MAC_ADDR_LEN];
int flag_ACK = 0;
#define Seconds_60 5
#define Minutes_10 600
char RawData_Ping[] = {
/*---- wlan header start -----*/
0x88, /* version , type sub type */
                        0x02, /* Frame control flag */
                        0x2C, 0x00, 0x00, 0x23, 0x75, 0x55, 0x55, 0x55, /* destination */
                        0x00, 0x22, 0x75, 0x55, 0x55, 0x55, /* bssid */
                        0x08, 0x00, 0x28, 0x19, 0x02, 0x85, /* source */
                        0x80, 0x42, 0x00, 0x00, 0xAA, 0xAA, 0x03, 0x00, 0x00,
                        0x00, 0x08, 0x00, /* LLC */
                        /*---- ip header start -----*/
                        0x45,
                        0x00, 0x00, 0x54, 0x96, 0xA1, 0x00, 0x00, 0x40, 0x01,
                        0x57, 0xFA, /* checksum */
                        0xc0, 0xa8, 0x01, 0x64, /* src ip */
                        0xc0, 0xa8, 0x01, 0x02, /* dest ip  */
                        /* payload - ping/icmp */
                        0xdd,
                        0xdd, 0xdd, 0xdd, 0x5E, 0x18, 0x00, 0x00, 0x41, 0x08,
                        0xBB, 0x8D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00 };

char Hello[] = {
/*---- wlan header start -----*/
0x88, /* version , type sub type */
                 0x02, /* Frame control flag */
                 0x2C, 0x00, 0x00, 0x23, 0x75, 0x55, 0x55, 0x55, /* destination */
                 0x00, 0x22, 0x75, 0x55, 0x55, 0x55, /* bssid */
                 0x08, 0x00, 0x28, 0x19, 0x02, 0x85, /* source */
                 0x80, 0x42, 0x00, 0x00, 0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00,
                 0x08, 0x00, /* LLC */
                 /*---- ip header start -----*/
                 0x45,
                 0x00, 0x00, 0x54, 0x96, 0xA1, 0x00, 0x00, 0x40, 0x01, 0x57,
                 0xFA, /* checksum */
                 0xc0, 0xa8, 0x01, 0x64, /* src ip */
                 0xc0, 0xa8, 0x01, 0x02, /* dest ip  */
                 /* payload - ping/icmp */
                 0xCC,
                 0xCC, 0xCC, 0xCC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00 };

char ACK[] = {
/*---- wlan header start -----*/
0x88, /* version , type sub type */
               0x02, /* Frame control flag */
               0x2C, 0x00, 0x00, 0x23, 0x75, 0x55, 0x55, 0x55, /* destination */
               0x00, 0x22, 0x75, 0x55, 0x55, 0x55, /* bssid */
               0x08, 0x00, 0x28, 0x19, 0x02, 0x85, /* source */
               0x80, 0x42, 0x00, 0x00, 0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x08,
               0x00, /* LLC */
               /*---- ip header start -----*/
               0x45,
               0x00, 0x00, 0x54, 0x96, 0xA1, 0x00, 0x00, 0x40, 0x01, 0x57, 0xFA, /* checksum */
               0xc0, 0xa8, 0x01, 0x64, /* src ip */
               0xc0, 0xa8, 0x01, 0x02, /* dest ip  */
               /* payload - ping/icmp */
               0xAA,
               0xAA, 0xAA, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00 };

char Data[] = {
/*---- wlan header start -----*/
0x88, /* version , type sub type */
                0x02, /* Frame control flag */
                0x2C, 0x00, 0x00, 0x23, 0x75, 0x55, 0x55, 0x55, /* destination */
                0x00, 0x22, 0x75, 0x55, 0x55, 0x55, /* bssid */
                0x08, 0x00, 0x28, 0x19, 0x02, 0x85, /* source */
                0x80, 0x42, 0x00, 0x00, 0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00,
                0x08, 0x00, /* LLC */
                /*---- ip header start -----*/
                0x45,
                0x00, 0x00, 0x54, 0x96, 0xA1, 0x00, 0x00, 0x40, 0x01, 0x57,
                0xFA, /* checksum */
                0xc0, 0xa8, 0x01, 0x64, /* src ip */
                0xc0, 0xa8, 0x01, 0x02, /* dest ip  */
                /* payload - ping/icmp */
                0xbb,
                0xbb, 0xbb, 0xbb, 0x5E, 0x18, 0x00, 0x00, 0x41, 0x08, 0xBB,
                0x8D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00 };
_u8 Mac_array[6][10];
int i_mac_array = 0;
#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif
//****************************************************************************
static int Tx_continuous(int iChannel, SlRateIndex_e rate, int iNumberOfPackets,
                         int iTxPowerLevel, long dIntervalMiliSec,
                         int NumberOfSeconds, int message_type,
                         _u8 source_mac[6]);
//static int transmit(int iChannel, SlRateIndex_e rate, int iNumberOfPackets,
//                    int iTxPowerLevel, long dIntervalMiliSec,
//                    int NumberOfSeconds, int message_type, _u8 source_mac[6]);
static void DisplayBanner(char * AppName);
static void BoardInit(void);
static void interpackettiming(int);
static void tabulate(_u8 mac_add[6]);
static void random_backoff_delay(void);
static _u8 * Packet_to_Array(Packet);
//*****************************************************************************
typedef struct Packet
{
    _u8 version;_u8 frame_control;
    //  mac layer
    _u8 mac_src[6];_u8 mac_dest[6];_u8 mac_ack;_u32 mac_packet_id;
    //  Network layer
    _u32 ip_src;_u32 ip_dest;_u32 ip_hop_count;_u8 ip_hello;_u32 ip_hello_id;
    //  App layer
    _u32 app_data;_u32 app_timestamp;
} Packet;
//_u8 * Packet_to_Array(Packet packet){
//    _u8 *packet_array = malloc(44 * sizeof(_u8));
//    packet_array[0] = packet.version;
//    packet_array[1] = packet.frame_control;
//
//    packet_array[2] = packet.mac_src[0];
//    packet_array[3] = packet.mac_src[1];
//    packet_array[4] = packet.mac_src[2];
//    packet_array[5] = packet.mac_src[3];
//    packet_array[6] = packet.mac_src[4];
//    packet_array[7] = packet.mac_src[5];
//
//    packet_array[8] = packet.mac_dest[0];
//}
//*****************************************************************************
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{
    switch (pWlanEvent->Event)
    {
    case SL_WLAN_CONNECT_EVENT:
    {
        SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
        // Information about the connected AP (like name, MAC etc) will be
        // available in 'slWlanConnectAsyncResponse_t' - Applications
        // can use it if required
        //  slWlanConnectAsyncResponse_t *pEventData = NULL;
        // pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
        // Copy new connection SSID and BSSID to global parameters
        memcpy(g_ucConnectionSSID,
               pWlanEvent->EventData.STAandP2PModeWlanConnected.ssid_name,
               pWlanEvent->EventData.STAandP2PModeWlanConnected.ssid_len);
        memcpy(g_ucConnectionBSSID,
               pWlanEvent->EventData.STAandP2PModeWlanConnected.bssid,
               SL_BSSID_LENGTH);

        UART_PRINT("[WLAN EVENT] STA Connected to the AP: %s , "
                   "BSSID: %x:%x:%x:%x:%x:%x\n\r",
                   g_ucConnectionSSID, g_ucConnectionBSSID[0],
                   g_ucConnectionBSSID[1], g_ucConnectionBSSID[2],
                   g_ucConnectionBSSID[3], g_ucConnectionBSSID[4],
                   g_ucConnectionBSSID[5]);
    }
        break;
    case SL_WLAN_DISCONNECT_EVENT:
    {
        slWlanConnectAsyncResponse_t* pEventData = NULL;
        CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
        CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);
        pEventData = &pWlanEvent->EventData.STAandP2PModeDisconnected;
        // If the user has initiated 'Disconnect' request,
        //'reason_code' is SL_WLAN_DISCONNECT_USER_INITIATED_DISCONNECTION
        if (SL_WLAN_DISCONNECT_USER_INITIATED_DISCONNECTION
                == pEventData->reason_code)
        {
            UART_PRINT("[WLAN EVENT]Device disconnected from the AP: %s,"
                       " BSSID: %x:%x:%x:%x:%x:%x on application's"
                       " request \n\r",
                       g_ucConnectionSSID, g_ucConnectionBSSID[0],
                       g_ucConnectionBSSID[1], g_ucConnectionBSSID[2],
                       g_ucConnectionBSSID[3], g_ucConnectionBSSID[4],
                       g_ucConnectionBSSID[5]);
        }
        else
        {
            UART_PRINT("[WLAN ERROR]Device disconnected from the AP AP: %s,"
                       " BSSID: %x:%x:%x:%x:%x:%x on an ERROR..!! \n\r",
                       g_ucConnectionSSID, g_ucConnectionBSSID[0],
                       g_ucConnectionBSSID[1], g_ucConnectionBSSID[2],
                       g_ucConnectionBSSID[3], g_ucConnectionBSSID[4],
                       g_ucConnectionBSSID[5]);
        }
        memset(g_ucConnectionSSID, 0, sizeof(g_ucConnectionSSID));
        memset(g_ucConnectionBSSID, 0, sizeof(g_ucConnectionBSSID));
    }
        break;
    default:
    {
        UART_PRINT("[WLAN EVENT] Unexpected event [0x%x]\n\r",
                   pWlanEvent->Event);
    }
        break;
    }
}
//*****************************************************************************
//
//! \brief This function handles network events such as IP acquisition, IP
//!           leased, IP released etc.
//!
//! \param[in]  pNetAppEvent - Pointer to NetApp Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
    switch (pNetAppEvent->Event)
    {
    case SL_NETAPP_IPV4_IPACQUIRED_EVENT:
    {
        SlIpV4AcquiredAsync_t *pEventData = NULL;
        SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);
        //Ip Acquired Event Data
        pEventData = &pNetAppEvent->EventData.ipAcquiredV4;
        //Gateway IP address
        g_ulGatewayIP = pEventData->gateway;
        UART_PRINT(
                "[NETAPP EVENT] IP Acquired: IP=%d.%d.%d.%d , "
                "Gateway=%d.%d.%d.%d\n\r",
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 3),
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 2),
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 1),
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 0),
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 3),
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 2),
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 1),
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 0));
    }
        break;
    default:
    {
        UART_PRINT("[NETAPP EVENT] Unexpected event [0x%x] \n\r",
                   pNetAppEvent->Event);
    }
        break;
    }
}
//*****************************************************************************
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent,
                                  SlHttpServerResponse_t *pHttpResponse)
{
}
//*****************************************************************************
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    // Most of the general errors are not FATAL are are to be handled appropriately by the application
    UART_PRINT("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
               pDevEvent->EventData.deviceEvent.status,
               pDevEvent->EventData.deviceEvent.sender);
}
//*****************************************************************************
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
}
//*****************************************************************************
static void InitializeAppVariables()
{
    g_ulStatus = 0;
    g_ulGatewayIP = 0;
    memset(g_ucConnectionSSID, 0, sizeof(g_ucConnectionSSID));
    memset(g_ucConnectionBSSID, 0, sizeof(g_ucConnectionBSSID));
}
//*****************************************************************************
//! \brief This function puts the device in its default state. It:
//!           - Set the mode to STATION
//!           - Configures connection policy to Auto and AutoSmartConfig
//!           - Deletes all the stored profiles
//!           - Enables DHCP
//!           - Disables Scan policy
//!           - Sets Tx power to maximum
//!           - Sets power policy to normal
//!           - Unregister mDNS services
//!           - Remove all filters
//!
//! \param   none
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static long ConfigureSimpleLinkToDefaultState()
{
    SlVersionFull ver = { 0 };
    _WlanRxFilterOperationCommandBuff_t RxFilterIdMask = { 0 };
    unsigned char ucVal = 1;
    unsigned char ucConfigOpt = 0;
    unsigned char ucConfigLen = 0;
    unsigned char ucPower = 0;
    long lRetVal = -1;
    long lMode = -1;
    lMode = sl_Start(0, 0, 0);
    ASSERT_ON_ERROR(lMode);
    // If the device is not in station-mode, try configuring it in station-mode 
    if (ROLE_STA != lMode)
    {
        if (ROLE_AP == lMode)
        {
            // If the device is in AP mode, we need to wait for this event 
            // before doing anything 
            while (!IS_IP_ACQUIRED(g_ulStatus))
            {
#ifndef SL_PLATFORM_MULTI_THREADED
                _SlNonOsMainLoopTask();
#endif
            }
        }
        // Switch to STA role and restart 
        lRetVal = sl_WlanSetMode(ROLE_STA);
        ASSERT_ON_ERROR(lRetVal);
        lRetVal = sl_Stop(0xFF);
        ASSERT_ON_ERROR(lRetVal);
        lRetVal = sl_Start(0, 0, 0);
        ASSERT_ON_ERROR(lRetVal);
        // Check if the device is in station again 
        if (ROLE_STA != lRetVal)
        {
            // We don't want to proceed if the device is not coming up in STA-mode 
            return DEVICE_NOT_IN_STATION_MODE;
        }
    }
    // Get the device's version-information
    ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
    ucConfigLen = sizeof(ver);
    lRetVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &ucConfigOpt,
                        &ucConfigLen, (unsigned char *) (&ver));
    ASSERT_ON_ERROR(lRetVal);
    UART_PRINT("Host Driver Version: %s\n\r", SL_DRIVER_VERSION);
    UART_PRINT("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r",
               ver.NwpVersion[0], ver.NwpVersion[1], ver.NwpVersion[2],
               ver.NwpVersion[3], ver.ChipFwAndPhyVersion.FwVersion[0],
               ver.ChipFwAndPhyVersion.FwVersion[1],
               ver.ChipFwAndPhyVersion.FwVersion[2],
               ver.ChipFwAndPhyVersion.FwVersion[3],
               ver.ChipFwAndPhyVersion.PhyVersion[0],
               ver.ChipFwAndPhyVersion.PhyVersion[1],
               ver.ChipFwAndPhyVersion.PhyVersion[2],
               ver.ChipFwAndPhyVersion.PhyVersion[3]);
    // Set connection policy to Auto + SmartConfig 
    //      (Device's default connection policy)
    lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION,
                               SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
    ASSERT_ON_ERROR(lRetVal);
    // Remove all profiles
    lRetVal = sl_WlanProfileDel(0xFF);
    ASSERT_ON_ERROR(lRetVal);
    // Device in station-mode. Disconnect previous connection if any
    // The function returns 0 if 'Disconnected done', negative number if already
    // disconnected Wait for 'disconnection' event if 0 is returned, Ignore 
    // other return-codes
    //
    lRetVal = sl_WlanDisconnect();
    if (0 == lRetVal)
    {
        // Wait
        while (IS_CONNECTED(g_ulStatus))
        {
#ifndef SL_PLATFORM_MULTI_THREADED
            _SlNonOsMainLoopTask();
#endif
        }
    }
    // Enable DHCP client
    lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE, 1, 1, &ucVal);
    ASSERT_ON_ERROR(lRetVal);
    // Disable scan
    ucConfigOpt = SL_SCAN_POLICY(0);
    lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN, ucConfigOpt, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);
    // Set Tx power level for station mode
    // Number between 0-15, as dB offset from max power - 0 will set max power
    ucPower = 0;
    lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
    WLAN_GENERAL_PARAM_OPT_STA_TX_POWER,
                         1, (unsigned char *) &ucPower);
    ASSERT_ON_ERROR(lRetVal);
    // Set PM policy to normal
    lRetVal = sl_WlanPolicySet(SL_POLICY_PM, SL_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);
    // Unregister mDNS services
    lRetVal = sl_NetAppMDNSUnRegisterService(0, 0);
    ASSERT_ON_ERROR(lRetVal);
    // Remove  all 64 filters (8*8)
    memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
    lRetVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *) &RxFilterIdMask,
                                 sizeof(_WlanRxFilterOperationCommandBuff_t));
    ASSERT_ON_ERROR(lRetVal);
    lRetVal = sl_Stop(SL_STOP_TIMEOUT);
    ASSERT_ON_ERROR(lRetVal);
    InitializeAppVariables();
    return lRetVal; // Success
}
//*****************************************************************************
static void DisplayBanner(char * AppName)
{
    UART_PRINT("\n\n\n\r");
    UART_PRINT("\t\t *************************************************\n\r");
    UART_PRINT("\t\t\t %s       \n\r", AppName);
    UART_PRINT("\t\t *************************************************\n\r");
    UART_PRINT("\n\n\n\r");
}
//*****************************************************************************
static void BoardInit(void)
{
    /* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
    // Set vector table base
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long) &g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    // Enable Processor
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);
    PRCMCC3200MCUInit();
}
//*****************************************************************************
#define BUFFER_SIZE 300
typedef struct
{
    _u8 rate;_u8 channel;_i8 rssi;_u8 padding;_u32 timestamp;
} TransceiverRxOverHead_t;
//*****************************************************************************
void random_backoff_delay(void)
{

    long long i;
    int j;
    int k = rand() % 500000;//random
//    long long k = random() % 500000;//random
    UART_PRINT("This is the rand backoff \n\r");
    UART_PRINT("%d \n\r", k);
    for (i = 0; i < k; i++){ // rand number mod 2 micro seconds
        for (j = 0; j < 8; j++){ // 2 microsecond delay
        }
    }
}
//*****************************************************************************
static int Tx_continuous(int iChannel, SlRateIndex_e rate, int iNumberOfPackets,
                         int iTxPowerLevel, long dIntervalMiliSec,
                         int NumberOfSeconds, int message_type,
                         _u8 source_mac[6])
{
    int iSoc;
    long lRetVal = -1;
    long ulIndex;
    _u8 buffer[300] = { '\0' };
    char message[] = {
    /*---- wlan header start -----*/
                       0x00, /* version , type sub type */
                       0x00, /* Frame control flag */
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* destination */
                       0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, /* bssid */
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* source */
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, /* LLC */
                       /*---- ip header start -----*/
                       0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, /* checksum */
                       0x00, 0x00, 0x00, 0x00, /* src ip */
                       0x00, 0x08, 0x00, 0x00, /* dest ip  */
                       /* payload - ping/icmp */
                       0xCC,
                       0xCC, 0xCC, 0xCC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00 };
    int index;
    switch (message_type){
    case 0: //Request
        for (index = 0; index < sizeof(message); index++){
            message[index] = RawData_Ping[index];
        }
        break;
    case 1: //hello
        for (index = 0; index < sizeof(message); index++){
            message[index] = Hello[index];
        }
        break;
    case 2: //ack
        for (index = 0; index < sizeof(message); index++){
            message[index] = ACK[index];
        }
        UART_PRINT("preparaing ACK ");
        break;
    case 3: //data
        for (index = 0; index < sizeof(message); index++){
            message[index] = Data[index];
        }
        break;
    }
    for (index = 4; index < 10; index++){
        message[index] = source_mac[index - 4];
    }

//    interpackettiming(1000);
    UART_PRINT("Sending to : ");
    for (index = 0; index < 6; index++){
        message[index + 16] = macAddressVal[index];
        UART_PRINT("%X", source_mac[index]);
        if (index + 16 < 21)
            UART_PRINT(".");
    }
    UART_PRINT("\r\n");

    iSoc = sl_Socket(SL_AF_RF, SL_SOCK_RAW, iChannel);
    ASSERT_ON_ERROR(iSoc);

//  while loop for recv and backoff
    memset(&buffer[0], 0, sizeof(buffer));
    lRetVal = sl_Recv(iSoc, buffer, sizeof(buffer), 0);
//    UART_PRINT("lRetVal 1 is    ");
//    UART_PRINT("%d \n\r", lRetVal);
    int number_of_backoffs = 3;
  //  while (number_of_backoffs>=0 &&(lRetVal == 0 || lRetVal == SL_EAGAIN || lRetVal == 125|| lRetVal == 78|| lRetVal == 86|| lRetVal == 294 || lRetVal == 117 || lRetVal == 138 || lRetVal == 133|| lRetVal == 99|| lRetVal == 124|| lRetVal == 61|| lRetVal == 132)){
  //      number_of_backoffs--;
    UART_PRINT("alive before while\n\r");
    while (number_of_backoffs>=0 && lRetVal >=0){
      number_of_backoffs--;
          memset(&buffer[0], 0, sizeof(buffer));
        lRetVal = sl_Recv(iSoc, buffer, sizeof(buffer), 0);
        UART_PRINT("alive in while backoff loop %d\n\r", 3-number_of_backoffs);

        //        UART_PRINT("lRetVal loop is    ");
//        UART_PRINT("%d", lRetVal);
//        random_backoff_delay();
    }

//    UART_PRINT("Transmitting data...\r\n");
    UART_PRINT("alive before send\n\r");
   for (ulIndex = 0; ulIndex < iNumberOfPackets; ulIndex++){
        lRetVal = sl_Send(iSoc, message, sizeof(message),SL_RAW_RF_TX_PARAMS(iChannel, rate, iTxPowerLevel, PREAMBLE));
//        interpackettiming(1);
        UART_PRINT("alive after send\n\r");
        if (lRetVal < 0){
            sl_Close(iSoc);
            ASSERT_ON_ERROR(lRetVal);
        }
        //Sleep(dIntervalMiliSec);
        //MAP_UtilsDelay(4000000);
        MAP_UtilsDelay(4000);
    }

    lRetVal = sl_Close(iSoc);
    ASSERT_ON_ERROR(lRetVal);
    UART_PRINT("alive before return\n\r");


//    UART_PRINT("Transmission complete.\r\n");
    return SUCCESS;
}

void send_base(_u8 dest_mac[6],_u8 data[6]){
    //printmessage(dest_mac, 6);
    int msg_size = 64;
    char msg[msg_size];
    memset(&msg, 0, sizeof(msg));
    int i = 0;
    for(i = 0; i < 6; i++){
        msg[i + 4] = dest_mac[i];
        msg[i + 16] = macAddressVal[i];
        msg[i + 54] = data[i];
    }
    sl_Send(iSoc, msg, sizeof(msg),SL_RAW_RF_TX_PARAMS(flag_channel,(SlRateIndex_e)flag_rate, flag_power, PREAMBLE));
}

void send_hello(){
    _u8 dest_mac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    _u8 data[] = {0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc};
    send_base(dest_mac, data);
}

void send_request(_u8 dest_mac[6]){
    _u8 data[6] = {0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd};
    send_base(dest_mac, data);
}

void send_data(_u8 dest_mac[6]){
    _u8 data[6] = {0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb};
    send_base(dest_mac, data);
}

void tabulate(_u8 mac_add[6])
{
    int j, i;
    for (j = 0; j < 6; j++)
    {
        Mac_array[j][i_mac_array] = mac_add[j];
    }
    i_mac_array++;

    for (j = 0; j < 10; j++)
    {
        UART_PRINT("MAC Address %X : ", j);
        for (i = 0; i < 6; i++)
        {
            UART_PRINT("%02x", (unsigned char) Mac_array[i][j]);
            if (i < 5)
            {
                UART_PRINT(".");
            }
        }
        UART_PRINT("\n\r");
    }
    UART_PRINT("\n\r");
    // check global mac array for repeated mac addresses

}
//*****************************************************************************
void interpackettiming(int NumberOfMilliSeconds)
{
    int j = 0;
    int k = 0;
//    UART_PRINT("Interpacket time gap . . .");
    for (j = 0; j < NumberOfMilliSeconds; j++)
    {
        for (k = 0; k < 40; k++)
        {
        }
//        UART_PRINT("%d ", j + 1);
//        UART_PRINT("Seconds elapsed \n\r");
    }
}
//*****************************************************************************
int TransceiverModeRx(_u8 c1channel_number, _u8 source_mac[6], int mode_selector){   //  remove the extra condition in the if below ( MAC )
    TransceiverRxOverHead_t *frameRadioHeader = NULL;
    flag_ACK = 0;
    int RxTime, inf, extra_var;
    int cchannel_number = c1channel_number;
    _u8 buffer[BUFFER_SIZE] = { '\0' };
    _i32 qsocket_handle = -1;
    _i32 recievedBytes = 0;
    qsocket_handle = sl_Socket(SL_AF_RF, SL_SOCK_RAW, cchannel_number);
    switch (mode_selector){
        case 0://hello
            inf = 1;
            RxTime = 0;
            break;
        case 1://ack and data
            RxTime = 20000;
            inf = 0;
            break;
        case 2://request
            RxTime = 0;
            inf = 1;
            break;
        case 3://data
            RxTime = 1;
            extra_var=1;
            inf = 0;
            break;
    }
//    UART_PRINT("alive after initial\n\r");
    int j = 0;
    int k = 0;
    for (j = 0; j < RxTime; j++)
    {
        for (k = 0; k < 5; k++)
        {
            memset(&buffer[0], 0, sizeof(buffer));
            recievedBytes = sl_Recv(qsocket_handle, buffer, BUFFER_SIZE, 0);
            frameRadioHeader = (TransceiverRxOverHead_t *) buffer;
//            interpackettiming(1000);
//            UART_PRINT("before\n\r");
            if ((buffer[12] == macAddressVal[0]
                    && buffer[13] == macAddressVal[1]
                    && buffer[14] == macAddressVal[2]
                    && buffer[15] == macAddressVal[3]
                    && buffer[16] == macAddressVal[4]
                    && buffer[17] == macAddressVal[5]) && ((buffer[62] == 0xaa && buffer[63] == 0xaa && buffer[64] == 0xaa && buffer[65] == 0xaa)|| (buffer[62] == 0xbb && buffer[63] == 0xbb)))
            {
                UART_PRINT("Recieved data and RxTime is : %d \n\r ",k);
                source_mac[0] = buffer[24];
                source_mac[1] = buffer[25];
                source_mac[2] = buffer[26];
                source_mac[3] = buffer[27];
                source_mac[4] = buffer[28];
                source_mac[5] = buffer[29];


                flag_ACK = 1;
                sl_Close(qsocket_handle);
                return 1;
            }
//            UART_PRINT("looping \n\r");
        }
    }
    while (inf){    //receiving hello and requests
        memset(&buffer[0], 0, sizeof(buffer));
        recievedBytes = sl_Recv(qsocket_handle, buffer, BUFFER_SIZE, 0);
        frameRadioHeader = (TransceiverRxOverHead_t *) buffer;
        if ((buffer[12] == 0xFF && buffer[13] == 0xFF && buffer[14] == 0xFF && buffer[15] == 0xFF && buffer[16] == 0xFF && buffer[17] == 0xFF
                && buffer[62] == 0xcc && buffer[63] == 0xcc && buffer[64] == 0xcc && buffer[65] == 0xcc) ||
                (buffer[12] == macAddressVal[0] && buffer[13] == macAddressVal[1] && buffer[14] == macAddressVal[2] && buffer[15] == macAddressVal[3]
                 && buffer[16] == macAddressVal[4] && buffer[17] == macAddressVal[5] && buffer[62] == 0xdd  && buffer[63] == 0xdd  && buffer[64] == 0xdd  && buffer[65] == 0xdd )){
            UART_PRINT("Received a packet: %02x\n\r", buffer[62]);
            source_mac[0] = buffer[24];
            source_mac[1] = buffer[25];
            source_mac[2] = buffer[26];
            source_mac[3] = buffer[27];
            source_mac[4] = buffer[28];
            source_mac[5] = buffer[29];
            break;
        }
    }
    sl_Close(qsocket_handle);
    return 0;
}
void printmessage(_u8 message[], int size)
{
    int i = 0;
    UART_PRINT("\n\r");
    UART_PRINT("\n\r");
    UART_PRINT("\n\r");
    UART_PRINT("******************************************\n\r");
    for(i = 0; i < size; i++)
    {
        UART_PRINT("%02X\t", (unsigned char) message[i]);
    }
    UART_PRINT("\n\r");
    UART_PRINT("******************************************\n\r");
    UART_PRINT("\n\r");
}

int receive_base(_u8 dest_mac[6], _u8 data[6], int timeout){
    int msg_size = 100;
    char msg[msg_size];
    int j = 0;
    for(j = 0; j < timeout; j++){
        memset(&msg, 0, sizeof(msg));
        sl_Recv(iSoc, msg, sizeof(msg), 0);
//        UART_PRINT("message received is: ");
       // printmessage(msg, msg_size);
        int i = 0, mac_notequal = 0, data_notequal = 0;
        for(i = 0; i < 6; i++){
//            UART_PRINT("mac %d %02X\n\r",i,msg[12 + i]);
            if(msg[12 + i] != dest_mac[i]){
                mac_notequal = 1;
            }
        }
        for(i = 0; i < 4; i++){
//            UART_PRINT("data %d %02X\n\r",i,msg[62 + i]);
            if(msg[62 + i] != data[i]){
                data_notequal = 1;
            }
        }
        if(mac_notequal == 0 && data_notequal == 0){
            return 1;
        }
    }
    return 0;
}

int receive_data(){
    _u8 dest_mac[6];
    int i;
    for(i = 0; i < 6; i++){
       dest_mac[i] = macAddressVal[i];
   }
    _u8 data[6] = {0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb};
    return receive_base(dest_mac, data, 1000);
}

int receive_request(){
    _u8 dest_mac[6];
    int i;
    for(i = 0; i < 6; i++){
       dest_mac[i] = macAddressVal[i];
   }
    _u8 data[6] = {0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd};
    return receive_base(dest_mac, data, 1000000);
}

int packtets_received_counter = 0;
void sink_function(){
    while (1){
        UART_PRINT("Sending request\n\r");
        _u8 dest_mac[6] = {0xd4, 0x36, 0x39, 0x55, 0xac, 0xac};
        iSoc = sl_Socket(SL_AF_RF, SL_SOCK_RAW, flag_channel);
        send_request(dest_mac);
        packtets_received_counter += receive_data();
        sl_Close(iSoc);
        UART_PRINT("In= %d\n\r",packtets_received_counter);
    }
}

void source_function(){
    while (1){
        UART_PRINT("Waiting for request\n\r\n\r");
        _u8 dest_mac[6] = {0xf4, 0xb8, 0x5e, 0x00, 0xfe, 0x27};
        iSoc = sl_Socket(SL_AF_RF, SL_SOCK_RAW, flag_channel);
        receive_request();
        send_data(dest_mac);
        sl_Close(iSoc);
    }
}



#define flag_interpackettime 1000
#include <time.h>

int packtets_sent_counter = 0;
int available_sources = 0;
int source=0;
int main()
{
    int trials=0;
    int msec[4];
    clock_t before;
    clock_t difference;
    int iFlag = 1;
    int Source_resend_counter = 0;
    long lRetVal = -1;
    char cChar;
    unsigned char policyVal;
    BoardInit();    // Initialize Board configuration
    PinMuxConfig();    //Pin muxing
    InitTerm();    // Configuring UART
    DisplayBanner(APPLICATION_NAME);
    InitializeAppVariables();
    // Following function configure the device to default state by cleaning the persistent settings stored in NVMEM (viz. connection profiles & policies, power policy etc)
    // Applications may choose to skip this step if the developer is sure that the device is in its default state at start of applicaton
    // Note that all profiles and persistent settings that were done on the device will be lost
    lRetVal = ConfigureSimpleLinkToDefaultState();
    CLR_STATUS_BIT_ALL(g_ulStatus);
    // Assumption is that the device is configured in station mode already
    // and it is in its default state
    lRetVal = sl_Start(0, 0, 0);
    int i;
    unsigned char macAddressLen = SL_MAC_ADDR_LEN;
    sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &macAddressLen, (unsigned char *) macAddressVal);
    // reset all network policies
    lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION,SL_CONNECTION_POLICY(0, 0, 0, 0, 0), &policyVal,1 /*PolicyValLen*/);
    UART_PRINT("MAC Address is : ");
    for (i = 0; i < macAddressLen; i++){
        UART_PRINT("%02X", (unsigned char) macAddressVal[i]);
        if (i < macAddressLen - 1)
            UART_PRINT(".");
    }
    UART_PRINT("\n\r");
    srand((macAddressVal[0] * macAddressVal[1] * macAddressVal[2] * macAddressVal[3] * macAddressVal[4] * macAddressVal[5]) % RAND_MAX);

    if (flag_function == 1){
        sink_function();
    }else{
        source_function();
    }
}
//int old_main(){
//    _u8 source_mac[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
//    int timeslice=0;
//    int nofpackets=2;
//    double pdr[4];
//
//        switch (flag_function)
//        {
//        case (1):    //SINK node;
//
//                   UART_PRINT(
//                    "\n\r//////////////////////   SINK MODE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\ \n\r\n\r");
////            UART_PRINT("Sending Hello\n\r");
////            lRetVal = Tx_continuous(flag_channel, flag_rate, 1, flag_power, 0,0, 1, source_mac);
////            random_backoff_delay();
////            if (lRetVal < 0){
////                UART_PRINT("Error during transmission of raw data\n\r");
////                LOOP_FOREVER();
////            }
////            UART_PRINT("Waiting for ACKs\n\r");
////            available_sources=0;
////            for(i=0; i<2; i++){
////                source = TransceiverModeRx(flag_channel, source_mac, 1);
////                UART_PRINT("source: %d\n\r", source);
////                if (source==1){
////                    available_sources++;
////                    UART_PRINT("Received Ack No: %d\n\r",i);
////                    tabulate(source_mac);
////                }
////            }
//        available_sources = 1;
////        Mac_array[0][0] = 0xd4;
////        Mac_array[1][0] = 0x36;
////        Mac_array[2][0] = 0x39;
////        Mac_array[3][0] = 0x55;
////        Mac_array[4][0] = 0xac;
////        Mac_array[5][0] = 0x79;
//
//        Mac_array[0][0] = 0xd4;
//        Mac_array[1][0] = 0x36;
//        Mac_array[2][0] = 0x39;
//        Mac_array[3][0] = 0x55;
//        Mac_array[4][0] = 0xac;
//        Mac_array[5][0] = 0xac;
//         //   if(available_sources == 0)continue;
//            int j;
//        for(trials=0; trials<4; trials++)
//        {
//            packtets_sent_counter = 0;
//            packtets_received_counter = 0;
//
//            before = clock();
//            timeslice+=2;
//            for(j=0; j < nofpackets; j++)
//            {
//                for(i=0;i<available_sources;i++)
//                {
//                    source_mac[0] = Mac_array[0][i];
//                    source_mac[1] = Mac_array[1][i];
//                    source_mac[2] = Mac_array[2][i];
//                    source_mac[3] = Mac_array[3][i];
//                    source_mac[4] = Mac_array[4][i];
//                    source_mac[5] = Mac_array[5][i];
////                    int kk;
////                    UART_PRINT("Sending to : ");
////                    for (kk=0; kk<6; kk++){
////                        UART_PRINT("%x",source_mac[kk]);
////                        if(kk<6)
////                            UART_PRINT(".");
////                    }
////                    UART_PRINT("\n\r");
//                    lRetVal = Tx_continuous(flag_channel, flag_rate, 1, flag_power, 0, 0, 0, source_mac);
//                    if(!lRetVal){
//                        UART_PRINT("sent data \n\r");
//                    }UART_PRINT("1 \n\r");
//                    packtets_sent_counter++;
//                    UART_PRINT("2 \n\r");
//                    //interpackettiming(1);
//                    UART_PRINT("3 \n\r");
//                    lRetVal=TransceiverModeRx(flag_channel, source_mac, 3);
//                    UART_PRINT("4 \n\r");
//                    if(lRetVal)
//                    packtets_received_counter +=1 ;
//                }
//                UART_PRINT("done \n\r");
//                //interpacket timing = 2, 4, 8
//                UART_PRINT("Loop #%d out: %d in: %d\n\r\n\r", j, packtets_sent_counter, packtets_received_counter);
//                interpackettiming(timeslice);
//            }
//            difference = clock() - before;
//            UART_PRINT("alive after difference\n\r");
//            msec[trials] = difference * 1000 / CLOCKS_PER_SEC;
//            UART_PRINT("live after msec\n\r");
//            pdr[trials] = packtets_received_counter * 100.0 / packtets_sent_counter;
//            UART_PRINT("alive after pdr calc\n\r");
//
//        }
//            UART_PRINT("alive after 4 trials \n\r");
//
//                for(trials=0; trials<4; trials++)
//                    UART_PRINT("for inter-time=%d, PDR= %f, time taken = %d", 2 * (trials + 1), pdr[trials], msec[trials]);
//
//            UART_PRINT("///////////////////////////  Done with time-slice \n\r\n\r\n\r");
//
//            break;
//        case (2):    //SOURCE node
//            UART_PRINT(
//                    "\n\r//////////////////////   SOURCE MODE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\ \n\r\n\r");
////            UART_PRINT("size of Packet = %d \n\r",sizeof(Packet));
//
////            TransceiverModeRx(flag_channel, source_mac, 0);//waiting for hello
//////            UART_PRINT("Recieved Hello\n\r");
////
////
////            interpackettiming((flag_interpackettime + 1));
////            random_backoff_delay();
////            lRetVal = Tx_continuous(flag_channel, flag_rate, 1, flag_power, 0,0, 2, source_mac);
////            UART_PRINT("Sent Ack\n\r");
//
//            while(1){
//                UART_PRINT("Waiting for request.\n\r");
//                TransceiverModeRx(flag_channel, source_mac, 2);
//                UART_PRINT("received request, preparing data for transmission \n\r");
//                interpackettiming((flag_interpackettime + 1000));
////                random_backoff_delay();
//
//                lRetVal = Tx_continuous(flag_channel, flag_rate, 1, flag_power, 0, 0, 3, source_mac);
//                UART_PRINT("Sent data.\n\r");
//            }
//
////            UART_PRINT("Recieved Request\n\r");
////            //waiting for request
////
////            UART_PRINT("Sent Data\n\r");
////            TransceiverModeRx(flag_channel, source_mac, 1);
////            UART_PRINT("Recieved Ack\n\r");
////            //  change UART_PRINT(ACK) to proper location in RX function and Resend data
////            Source_resend_counter = 0;
////            while ((!flag_ACK) && (Source_resend_counter < 5))
////            {
////                lRetVal = Tx_continuous(flag_channel, flag_rate, 1, flag_power,
////                                        0, flag_interpackettime, 2, source_mac);
////                UART_PRINT("Sent Data\n\r");
////                TransceiverModeRx(flag_channel, source_mac, 1);
////                Source_resend_counter++;
////            }
//            break;
//
//        }
//
//
//    return 0;
//}
