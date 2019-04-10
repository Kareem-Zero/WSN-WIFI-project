#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
#define APPLICATION_NAME        "TRANSCEIVER_MODE"
#define APPLICATION_VERSION     "1.1.1"
#define PREAMBLE            1        /* Preamble value 0- short, 1- long */
#define CPU_CYCLES_1MSEC (80*1000)
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
_u8 macAddressVal[SL_MAC_ADDR_LEN];
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
    UART_PRINT("\t\t\t CC3200 %s Application       \n\r", AppName);
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
#define BUFFER_SIZE 1472
typedef struct
{
    _u8 rate;_u8 channel;_i8 rssi;_u8 padding;_u32 timestamp;
} TransceiverRxOverHead_t;
//*****************************************************************************
void random_backoff_delay(void)
{
    srand((macAddressVal[0] * macAddressVal[1]) % RAND_MAX);
    long long i;
    int j;
    long long k = rand() % 500000;
    UART_PRINT("This is the rand backoff \n\r");
    UART_PRINT("%d \n\r", k);
    for (i = 0; i < k; i++)
    { // rand number mod 2 micro seconds
        for (j = 0; j < 8; j++) // 2 microsecond delay
        {

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
    _u8 buffer[1470] = { '\0' };
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
    switch (message_type)
    {
    case 0: //Request
        for (index = 0; index < sizeof(message); index++)
        {
            message[index] = RawData_Ping[index];
        }
        break;
    case 1: //hello
        for (index = 0; index < sizeof(message); index++)
        {
            message[index] = Hello[index];
        }
        break;
    case 2: //ack
        for (index = 0; index < sizeof(message); index++)
        {
            message[index] = ACK[index];
        }
        UART_PRINT("preparaing ACK ");
        break;
    case 3: //data
        for (index = 0; index < sizeof(message); index++)
        {
            message[index] = Data[index];
        }
        break;
    }
    for (index = 4; index < 10; index++)
    {
        message[index] = source_mac[index - 4];
    }
    UART_PRINT("Message Source MAC is : ");
    for (index = 0; index < 6; index++)
    {
        message[index + 16] = macAddressVal[index];
        UART_PRINT("%X", message[index + 16]);
        if (index + 16 < 21)
        {
            UART_PRINT(".");
        }
    }
    UART_PRINT("\r\n");
    iSoc = sl_Socket(SL_AF_RF, SL_SOCK_RAW, iChannel);
    ASSERT_ON_ERROR(iSoc);

//  while loop for recv and backoff
    memset(&buffer[0], 0, sizeof(buffer));
    lRetVal = sl_Recv(iSoc, buffer, 1470, 0);
    UART_PRINT("lRetVal 1 is    ");
    UART_PRINT("%d \n\r", lRetVal);
    while (lRetVal == 0 || lRetVal == SL_EAGAIN || lRetVal == 125|| lRetVal == 78|| lRetVal == 86|| lRetVal == 294 || lRetVal == 117 || lRetVal == 138 || lRetVal == 133|| lRetVal == 99)
    {
        memset(&buffer[0], 0, sizeof(buffer));
        lRetVal = sl_Recv(iSoc, buffer, 1470, 0);
        UART_PRINT("lRetVal loop is    ");
        UART_PRINT("%d", lRetVal);
        random_backoff_delay();
    }

    UART_PRINT("Transmitting data...\r\n");
    for (ulIndex = 0; ulIndex < iNumberOfPackets; ulIndex++)
    {
        lRetVal = sl_Send(
                iSoc, message, sizeof(message),
                SL_RAW_RF_TX_PARAMS(iChannel, rate, iTxPowerLevel, PREAMBLE));
        interpackettiming(NumberOfSeconds);
        if (lRetVal < 0)
        {
            sl_Close(iSoc);
            ASSERT_ON_ERROR(lRetVal);
        }
        //Sleep(dIntervalMiliSec);
        MAP_UtilsDelay(4000000);
    }

    lRetVal = sl_Close(iSoc);
    ASSERT_ON_ERROR(lRetVal);

    UART_PRINT("Transmission complete.\r\n");
    return SUCCESS;
}
//Packet message;
//static int transmit(int iChannel, SlRateIndex_e rate, int iNumberOfPackets,
//                    int iTxPowerLevel, long dIntervalMiliSec,
//                    int NumberOfSeconds, int message_type, _u8 source_mac[6])
//{
//    memset(&message, 0, sizeof(Packet));
//    int index;
//    UART_PRINT("Message Source MAC is : ");
//    for (index = 0; index < 6; index++)
//    {
//        message.mac_src[index] = macAddressVal[index];
//        message.mac_dest[index] = source_mac[index];
//        UART_PRINT("%X", message.mac_src[index]);
//        if (index + 16 < 21)
//            UART_PRINT(".");
//    }
//    UART_PRINT("\r\n");
//
//    int iSoc;
//    long lRetVal = -1;
//    long ulIndex;
//
//    message.mac_packet_id = 6;
//    message.version = 1;
//    message.frame_control = 2;
//
//    switch (message_type)
//    {
//    case 0: //ping
////            for(index=0;index<sizeof(message);index++){
////                message[index]=RawData_Ping[index];
////            }
//        break;
//    case 1: //hello
//        message.mac_dest[0]=0xff;
//        message.mac_dest[1]=0xff;
//        message.mac_dest[2]=0xff;
//        message.mac_dest[3]=0xff;
//        message.mac_dest[4]=0xff;
//        message.mac_dest[5]=0xff;
//        message.ip_hello = 1;
//        message.ip_hello_id = 5;
//        break;
//    case 2: //ack
//        message.mac_ack = 1;
//        break;
//    case 3: //data
//        message.app_data = 66;
//        message.app_timestamp = 5124;
//        break;
//    }
//
//    iSoc = sl_Socket(SL_AF_RF, SL_SOCK_RAW, iChannel);
//    ASSERT_ON_ERROR(iSoc);
////  while loop for recv and backoff
//    Packet buffer;
//    memset(&buffer, 0, sizeof(Packet));
//    lRetVal = sl_Recv(iSoc, (_u8 *) &buffer, sizeof(Packet), 0);
//    UART_PRINT("lRetVal 1 is    ");
//    UART_PRINT("%d \n\r", lRetVal);
//    while (lRetVal == 0 || lRetVal == SL_EAGAIN)
//    {
//        memset(&buffer, 0, sizeof(Packet));
//        lRetVal = sl_Recv(iSoc, (_u8 *) &buffer, sizeof(Packet), 0);
//        UART_PRINT("lRetVal loop is    ");
//        UART_PRINT("%d", lRetVal);
//        random_backoff_delay();
//    }
//    UART_PRINT("Transmitting data...\r\n");
//
//    UART_PRINT("\r\n");
//    int j=0;
//    for(j=0; j< sizeof(Packet)-1; j++){
//                UART_PRINT("%02x\t\t", ((_u8 *)&message)[j]);
//            }
//    UART_PRINT("\r\n");
//    UART_PRINT("\r\n");
//
//    for (ulIndex = 0; ulIndex < iNumberOfPackets; ulIndex++)
//    {
//        lRetVal = sl_Send(
//                iSoc, (_u8 *) &message, sizeof(Packet),
//                SL_RAW_RF_TX_PARAMS(iChannel, rate, iTxPowerLevel, PREAMBLE));
//        interpackettiming(NumberOfSeconds);
//        if (lRetVal < 0)
//        {
//            sl_Close(iSoc);
//            ASSERT_ON_ERROR(lRetVal);
//        }
//        //Sleep(dIntervalMiliSec);
//        MAP_UtilsDelay(4000000);
//    }
//
//    lRetVal = sl_Close(iSoc);
//    ASSERT_ON_ERROR(lRetVal);
//
//    UART_PRINT("Transmission complete.\r\n");
//    return SUCCESS;
//}
//*****************************************************************************
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
            UART_PRINT("%X", (unsigned char) Mac_array[i][j]);
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
void interpackettiming(int NumberOfSeconds)
{
    int j = 0;
    int k = 0;
    UART_PRINT("Interpacket time gap . . .");
    for (j = 0; j < NumberOfSeconds; j++)
    {
        for (k = 0; k < 4000000; k++)
        {
        }
        UART_PRINT("%d ", j + 1);
        UART_PRINT("Seconds elapsed \n");
    }
    UART_PRINT("\r");
}
//*****************************************************************************
int TransceiverModeRx(_u8 c1channel_number, _u8 source_mac[6], int mode_selector){   //  remove the extra condition in the if below ( MAC )
    TransceiverRxOverHead_t *frameRadioHeader = NULL;
    flag_ACK = 0;
    int RxTime, inf;
    int cchannel_number = c1channel_number;
    _u8 buffer[BUFFER_SIZE] = { '\0' };
    _i32 qsocket_handle = -1;
    _i32 recievedBytes = 0;
    qsocket_handle = sl_Socket(SL_AF_RF, SL_SOCK_RAW, cchannel_number);
    switch (mode_selector)
    {
    case 0:
        inf = 1;
        RxTime = 0;
        break;
    case 1:
        RxTime = Seconds_60;
        inf = 0;
        break;
    case 2:
        RxTime = Minutes_10;
        inf = 0;

    case 3:
        RxTime = 10;
        inf = 0;
    };
    int i = 0;
    while (i < (4000000 * RxTime))    //ppkts_to_receive--
    {
        i++;
        memset(&buffer[0], 0, sizeof(buffer));
        recievedBytes = sl_Recv(qsocket_handle, buffer, BUFFER_SIZE, 0);
        frameRadioHeader = (TransceiverRxOverHead_t *) buffer;
        if ((buffer[12] == macAddressVal[0]
                && buffer[13] == macAddressVal[1]
                && buffer[14] == macAddressVal[2]
                && buffer[15] == macAddressVal[3]
                && buffer[16] == macAddressVal[4]
                && buffer[17] == macAddressVal[5]) && (buffer[62] == 0xaa || (buffer[62] == 0xbb && buffer[63] == 0xbb)))
        {
            source_mac[0] = buffer[24];
            source_mac[1] = buffer[25];
            source_mac[2] = buffer[26];
            source_mac[3] = buffer[27];
            source_mac[4] = buffer[28];
            source_mac[5] = buffer[29];
            if (buffer[62] == 0xaa){//recevied ack
                UART_PRINT("ACK Recieved");
            }
            if (buffer[62] == 0xbb && buffer[63] == 0xbb){//recevied data
                UART_PRINT("DATA Recieved");
            }
            flag_ACK = 1;
            sl_Close(qsocket_handle);
            return 1;
        }
    }
    while (inf)    //ppkts_to_receive--
    {
        memset(&buffer[0], 0, sizeof(buffer));
        recievedBytes = sl_Recv(qsocket_handle, buffer, BUFFER_SIZE, 0);
        frameRadioHeader = (TransceiverRxOverHead_t *) buffer;
        if ((buffer[12] == 0xFF && buffer[13] == 0xFF && buffer[14] == 0xFF
                && buffer[15] == 0xFF && buffer[16] == 0xFF
                && buffer[17] == 0xFF && buffer[62] == 0xcc)
                || (buffer[12] == macAddressVal[0]
                        && buffer[13] == macAddressVal[1]
                        && buffer[14] == macAddressVal[2]
                        && buffer[15] == macAddressVal[3]
                        && buffer[16] == macAddressVal[4]
                        && buffer[17] == macAddressVal[5] && (buffer[62] == 0xaa || buffer[62] == 0xdd )))
        {
            UART_PRINT("Received a packet: %02x\n\r",buffer[62]);
            source_mac[0] = buffer[24];
            source_mac[1] = buffer[25];
            source_mac[2] = buffer[26];
            source_mac[3] = buffer[27];
            source_mac[4] = buffer[28];
            source_mac[5] = buffer[29];
            break;
        }
//        if (buffer[12] == 0xd4 || buffer[12] == 0xf4
//                || (buffer[12] == 0xff && buffer[62] == 0xcc))
//        {
//            UART_PRINT(" ===>>> Timestamp: %iuS, Signal Strength: %idB\n\r",
//                       frameRadioHeader->timestamp, frameRadioHeader->rssi);
//            UART_PRINT(
//                    " ===>>> Destination MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n\r",
//                    buffer[12], buffer[13], buffer[14], buffer[15], buffer[16],
//                    buffer[17]);
//            UART_PRINT(" ===>>> Bssid: %02x:%02x:%02x:%02x:%02x:%02x\n\r",
//                       buffer[18], buffer[19], buffer[20], buffer[21],
//                       buffer[22], buffer[23]);
//            UART_PRINT(
//                    " ===>>> Source MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n\r",
//                    buffer[24], buffer[25], buffer[26], buffer[27], buffer[28],
//                    buffer[29]);
//            UART_PRINT(" ===>>> Source IP Address: %d.%d.%d.%d\n\r", buffer[54],
//                       buffer[55], buffer[56], buffer[57]);
//            UART_PRINT(" ===>>> Destination IP Address: %d.%d.%d.%d\n\r",
//                       buffer[58], buffer[59], buffer[60], buffer[61]);
//            UART_PRINT(" ===>>> Message: %02x.%02x.%02x.%02x\n\r\n", buffer[62],
//                       buffer[63], buffer[64], buffer[65]);
//        }
    }
    sl_Close(qsocket_handle);
    return 0;
}
//*****************************************************************************
//void Receive(_u8 c1channel_number, _u8 source_mac[6], int mode_selector)
//{   //  remove the extra condition in the if below ( MAC )
//    TransceiverRxOverHead_t *frameRadioHeader = NULL;
//    flag_ACK = 0;
//    int RxTime, inf;
//    int cchannel_number = c1channel_number;
//
//    _i32 qsocket_handle = -1;
//    _i32 recievedBytes = 0;
//    qsocket_handle = sl_Socket(SL_AF_RF, SL_SOCK_RAW, cchannel_number);
//    switch (mode_selector)
//    {
//    case 0:
//        inf = 1;
//        RxTime = 0;
//        break;
//    case 1:
//        RxTime = Seconds_60;
//        inf = 0;
//        break;
//    case 2:
//        RxTime = Minutes_10;
//        inf = 0;
//    };
//    int i = 0;
//    struct Packet buffer;
//    while(1){
//        memset(&buffer, 0, sizeof(Packet));
//            recievedBytes = sl_Recv(qsocket_handle, (_u8 *) &buffer, sizeof(Packet), 0);
//        frameRadioHeader = (TransceiverRxOverHead_t *) (_u8 *) &buffer;
//        int j=0;
//        int x=0;
//        for(j=0; j< sizeof(Packet)-1; j++){
//            UART_PRINT("%02x\t", ((_u8 *)&buffer)[j]);
//            if (((_u8 *)&buffer)[j] == 0xd4 && ((_u8 *)&buffer)[j+1] == 0x36){
//                UART_PRINT("\n\r\n\r\n\r\n\r");
//                UART_PRINT("///////////////////////////");
//                UART_PRINT("\n\r\n\r\n\r\n\r");
//                x=1;
//            }
//        }
//        UART_PRINT("\n\r\n\r");
//        if (x){
//            return;
//        }
//    }
//    while (i < (4000000 * RxTime))
//    {    //ppkts_to_receive--
//        i++;
//        memset(&buffer, 0, sizeof(Packet));
//        recievedBytes = sl_Recv(qsocket_handle, (_u8 *) &buffer, sizeof(Packet), 0);
//        frameRadioHeader = (TransceiverRxOverHead_t *) (_u8 *) &buffer;
//        if (buffer.mac_dest[0] == macAddressVal[0]
//                && buffer.mac_dest[1] == macAddressVal[1]
//                && buffer.mac_dest[2] == macAddressVal[2]
//                && buffer.mac_dest[3] == macAddressVal[3]
//                && buffer.mac_dest[4] == macAddressVal[4]
//                && buffer.mac_dest[5] == macAddressVal[5]
//                && buffer.mac_ack == 1)
//        {
//            source_mac = buffer.mac_src;
//            UART_PRINT("ACK Recieved");
//            flag_ACK = 1;
//            break;
//        }
//    }
//    while (inf)    //ppkts_to_receive--
//    {
//        memset(&buffer, 0, sizeof(Packet));
//        recievedBytes = sl_Recv(qsocket_handle, (_u8 *) &buffer, sizeof(Packet), 0);
//        UART_PRINT("recievedBytes: %d",recievedBytes);
//        frameRadioHeader = (TransceiverRxOverHead_t *) (_u8 *) &buffer;
//        if ((buffer.mac_dest[0] == 0xFF && buffer.mac_dest[1] == 0xFF
//                && buffer.mac_dest[2] == 0xFF && buffer.mac_dest[3] == 0xFF
//                && buffer.mac_dest[4] == 0xFF && buffer.mac_dest[5] == 0xFF
//                && buffer.ip_hello == 1)
//                || (buffer.mac_dest[0] == macAddressVal[0]
//                        && buffer.mac_dest[1] == macAddressVal[1]
//                        && buffer.mac_dest[2] == macAddressVal[2]
//                        && buffer.mac_dest[3] == macAddressVal[3]
//                        && buffer.mac_dest[4] == macAddressVal[4]
//                        && buffer.mac_dest[5] == macAddressVal[5]
//                        && buffer.mac_ack == 1))
//        {
//            source_mac = buffer.mac_src;
//            break;
//        }
////        if(buffer[12]==0xd4 || buffer[12]==0xf4 || (buffer[12]==0xff && buffer[62]==0xcc)){
////            UART_PRINT(" ===>>> Timestamp: %iuS, Signal Strength: %idB\n\r", frameRadioHeader->timestamp, frameRadioHeader->rssi);
////            UART_PRINT(" ===>>> Destination MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n\r", buffer[12], buffer[13], buffer[14], buffer[15], buffer[16], buffer[17]);
////            UART_PRINT(" ===>>> Bssid: %02x:%02x:%02x:%02x:%02x:%02x\n\r", buffer[18], buffer[19], buffer[20], buffer[21], buffer[22], buffer[23]);
////            UART_PRINT(" ===>>> Source MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n\r", buffer[24], buffer[25], buffer[26], buffer[27], buffer[28], buffer[29]);
////            UART_PRINT(" ===>>> Source IP Address: %d.%d.%d.%d\n\r", buffer[54],  buffer[55], buffer[56],  buffer[57]);
////            UART_PRINT(" ===>>> Destination IP Address: %d.%d.%d.%d\n\r", buffer[58],  buffer[59], buffer[60],  buffer[61]);
////            UART_PRINT(" ===>>> Message: %02x.%02x.%02x.%02x\n\r\n", buffer[62],  buffer[63], buffer[64],  buffer[65]);
////        }
//    }
//    sl_Close(qsocket_handle);
//}
//*****************************************************************************
#define flag_function 1//1: SINK, 2: SOURCE
#define flag_channel 2
#define flag_rate 5
#define flag_packets 10
#define flag_power 15
#define flag_interpackettime 2

int packtets_received_counter = 0;

int main()
{
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
    if (lRetVal < 0)
    {
        if (DEVICE_NOT_IN_STATION_MODE == lRetVal)
            UART_PRINT(
                    "Failed to configure the device in its default state \n\r");
        LOOP_FOREVER()
        ;
    }
    UART_PRINT("Device is configured in default state \n\r");
    CLR_STATUS_BIT_ALL(g_ulStatus);
    // Assumption is that the device is configured in station mode already
    // and it is in its default state
    lRetVal = sl_Start(0, 0, 0);
    int i;
    _u8 macAddressLen = SL_MAC_ADDR_LEN;
    sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &macAddressLen,
                 (unsigned char *) macAddressVal);
    UART_PRINT("MAC Address is : ");
    for (i = 0; i < macAddressLen; i++)
    {
        UART_PRINT("%d", (unsigned char) macAddressVal[i]);
        if (i < macAddressLen - 1)
            UART_PRINT(".");
    }
    UART_PRINT("\n\r");
    if (lRetVal < 0 || ROLE_STA != lRetVal)
    {
        UART_PRINT("Failed to start the device \n\r");
        LOOP_FOREVER()
        ;
    }
    UART_PRINT("Device started as STATION \n\r");
    // reset all network policies
    lRetVal = sl_WlanPolicySet( SL_POLICY_CONNECTION,
                               SL_CONNECTION_POLICY(0, 0, 0, 0, 0), &policyVal,
                               1 /*PolicyValLen*/);
    _u8 source_mac[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    while (iFlag)
    {
        switch (flag_function)
        {
        case (1):    //SINK node;
            UART_PRINT(
                    "\n\r//////////////////////   SINK MODE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\ \n\r\n\r");
            UART_PRINT("Sending Hello\n\r");
            lRetVal = Tx_continuous(flag_channel, flag_rate, 1, flag_power, 0,
                                    flag_interpackettime, 1, source_mac);
            if (lRetVal < 0){
                UART_PRINT("Error during transmission of raw data\n\r");
                LOOP_FOREVER();
            }
            UART_PRINT("Waiting for ACKs\n\r");
            for(i=0; i<3; i++){
                TransceiverModeRx(flag_channel, source_mac, 1);
                UART_PRINT("Recieved Ack No: %d\n\r",i);
                tabulate(source_mac);
            }
            int j;
            for(j=0; j<10; j++){
                for(i=0;i<3;i++){
                    source_mac[0] = Mac_array[i][0];
                    source_mac[1] = Mac_array[i][1];
                    source_mac[2] = Mac_array[i][2];
                    source_mac[3] = Mac_array[i][3];
                    source_mac[4] = Mac_array[i][4];
                    source_mac[5] = Mac_array[i][5];
                    lRetVal = Tx_continuous(flag_channel, flag_rate, 1, flag_power, 0, 0, 0, source_mac);
                    packtets_received_counter += TransceiverModeRx(flag_channel, source_mac, 1);
                    UART_PRINT("entered loop %d\n\r");

                }
                //interpacket timing = 2, 4, 8
                interpackettiming(2);
            }

            break;
        case (2):    //SOURCE node
            UART_PRINT(
                    "\n\r//////////////////////   SOURCE MODE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\ \n\r\n\r");
//            UART_PRINT("size of Packet = %d \n\r",sizeof(Packet));

            TransceiverModeRx(flag_channel, source_mac, 0);//waiting for hello
            UART_PRINT("Recieved Hello\n\r");


            interpackettiming((flag_interpackettime + 1));
            lRetVal = Tx_continuous(flag_channel, flag_rate, 1, flag_power, 0,flag_interpackettime, 2, source_mac);
            UART_PRINT("Sent Ack\n\r");

            while(1){
                UART_PRINT("Waiting for request.\n\r");
                TransceiverModeRx(flag_channel, source_mac, 0);
                UART_PRINT("recieved request, preparing data for transmission \n\r");
//                interpackettiming((flag_interpackettime + 1));
                lRetVal = Tx_continuous(flag_channel, flag_rate, 1, flag_power, 0, 0, 3, source_mac);
                UART_PRINT("Sent request.\n\r");
            }

//            UART_PRINT("Recieved Request\n\r");
//            //waiting for request
//
//            UART_PRINT("Sent Data\n\r");
//            TransceiverModeRx(flag_channel, source_mac, 1);
//            UART_PRINT("Recieved Ack\n\r");
//            //  change UART_PRINT(ACK) to proper location in RX function and Resend data
//            Source_resend_counter = 0;
//            while ((!flag_ACK) && (Source_resend_counter < 5))
//            {
//                lRetVal = Tx_continuous(flag_channel, flag_rate, 1, flag_power,
//                                        0, flag_interpackettime, 2, source_mac);
//                UART_PRINT("Sent Data\n\r");
//                TransceiverModeRx(flag_channel, source_mac, 1);
//                Source_resend_counter++;
//            }
            break;
        }
    }
}
