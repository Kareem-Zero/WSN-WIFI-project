#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
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
#include "pin.h"
#include "common.h"

#include "gpio_if.h"
#include "i2c_if.h"
#include "tmp006drv.h"

#ifndef NOTERM
#include "uart_if.h"
#endif
#include "pinmux.h"

int flag_function = 0;//0: SINK, 1: SOURCE
#define flag_channel 2
#define flag_rate 5
#define flag_packets 1
#define flag_power 1
int iSoc;
#define APPLICATION_NAME        (flag_function==0?"SINK NODE 2.0":"SOURCE NODE 2.0")

typedef enum{
    // Choosing -0x7D0 to avoid overlap w/ host-driver's error codes
    TX_CONTINUOUS_FAILED = -0x7D0,
    RX_STATISTICS_FAILED = TX_CONTINUOUS_FAILED - 1,
    DEVICE_NOT_IN_STATION_MODE = RX_STATISTICS_FAILED - 1,
    STATUS_CODE_MAX = -0xBB8
} e_AppStatusCodes;

volatile unsigned long g_ulStatus = 0; //SimpleLink Status
unsigned long g_ulGatewayIP = 0; //Network Gateway IP address
unsigned char g_ucConnectionSSID[SSID_LEN_MAX + 1]; //Connection SSID
unsigned char g_ucConnectionBSSID[BSSID_LEN_MAX]; //Connection BSSID
unsigned char macAddressVal[SL_MAC_ADDR_LEN];
_u8 ipAddressVal[4];
int ip_count = 0;
_u8 Mac_array[6][10];
int i_mac_array = 0;
#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif
//****************************************************************************
static void DisplayBanner(char * AppName);
static void BoardInit(void);
//*****************************************************************************
typedef struct Packet
{
    _u8 version;_u8 frame_control;
    //  mac layer
    _u8 mac_src[6];_u8 mac_dest[6];_u8 mac_ack;_u8 mac_packet_id;
    //  Network layer
    _u8 ip_src[4];_u8 ip_dest[4];_u8 ip_hop_count;_u8 ip_query;_u8 ip_query_id; _u8 ip_reply;
    //  App layer
    _u8 app_req; _u8 app_temp; _u8 app_timestamp;
} Packet;

typedef struct Arp {
  _u8 ip [4];
  _u8 mac [6];
  _u8 used;
}Arp;
Arp table[10];
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
typedef struct{
    _u8 rate;_u8 channel;_i8 rssi;_u8 padding;_u32 timestamp;
} TransceiverRxOverHead_t;


#define delay(x) MAP_UtilsDelay((40000/3)*x)

#define random_backoff_delay() delay((rand() % 5000)/1000)

static void DisplayBanner(char * AppName){
    UART_PRINT("\n\r*************************************************\n\r\t\t %s       \n\r*************************************************", AppName);
}

static void printmessage(_u8 message[], int size){
    int i = 0;
    UART_PRINT("\n\r");
    for(i = 0; i < size; i++)
        UART_PRINT("%02X\t", message[i]);
    UART_PRINT("\n\r*************************************************");
}

int mac_listen(){
    char temp_msg[sizeof(Packet)+8]={NULL};
    int flag_empty=1;
    int i;
    sl_Recv(iSoc, temp_msg, sizeof(Packet) + 8, 0);
            for(i = 0; i < sizeof(Packet); i++)
                if(temp_msg[i] != '0')     flag_empty=0;
    return flag_empty;
}

static void arp_clear_table(){
     int i;
     for(i = 0; i < 10; i++)
         table[i].used=0;
     ip_count = 0;
}

static void arp_insert_ip(Packet *p){
     int i = 0, j = 0, flag_new_ip = 1;
//     for(j=0; j<=ip_count;j++)
//         for(i = 0; i < 4; i++)
//             if(p->ip_src[i] != table[j].ip[i])   flag_new_ip=0;
//     if(!flag_new_ip){
        if(table[ip_count].used == 1 || ip_count == 10)
            return;
        for(i = 0; i<4 ; i++)
            table[ip_count].ip[i] = p->ip_src[i];
        for(i = 0; i<6 ;i++)
            table[ip_count].mac[i] = p->mac_src[i];
        ip_count++;
        table[ip_count].used = 1;

        UART_PRINT("ARP[%d]->IP:",ip_count-1);
        for(i=0;i<4;i++)UART_PRINT("%02x ",table[0].ip[i]);
        Message(" MAC:");
        for(i=0;i<6;i++)UART_PRINT("%02x ",table[0].mac[i]);
        Message("\n\r");
//     }
}

static void arp_get_dest_mac(Packet *p){
    int i = 0, j = 0, flag_dest_ip;
    for(i = 0, flag_dest_ip = 1; i<ip_count;i++){
        if(p->ip_dest[i] != table[j].ip[i])  flag_dest_ip=0;
        if(flag_dest_ip){
            for(j=0;j<6;j++)
                p->mac_dest[j]=table[i].mac[j];
            return;
        }
    }
}

static void mac_send_to(Packet *p, _u8 dest_mac[6]){
    int i = 0;
    for(i = 0; i < 6; i++){
        p->mac_dest[i] = dest_mac[i];
    }
    mac_send_base(p);
}

static void mac_send_base(Packet *p){
    int i = 0;
    for(i = 0; i < 6; i++)
        p->mac_src[i] = macAddressVal[i];
//    if(!mac_listen())
//        random_backoff_delay();
    sl_Send(iSoc, p, sizeof(Packet)+8,SL_RAW_RF_TX_PARAMS(flag_channel,(SlRateIndex_e)flag_rate, flag_power, 1));
}


//static void app_send_request(_u8 dest_mac[6]){
//    Packet p;
//    memset(&p, 0, sizeof(Packet));
//    p.app_req = 1;
//    mac_send_base(p, dest_mac);
//}
//
//static void app_send_temperature(_u8 dest_mac[6]){
//    Packet p;
//    memset(&p, 0, sizeof(Packet));
//    p.app_temp = 0x49;
//    mac_send_base(p, dest_mac);
//}

_u8 unique_query=7;       //(macAddressVal[5]*macAddressVal[6])%1000;
_u8 sent_query;
static void net_send_query(Packet p, _u8 dest_ip[4]){
    int i;
    for(i=0;i<4;i++){
        p.ip_dest[i]=dest_ip[i];
        p.ip_src[i]=ipAddressVal[i];
    }
    p.ip_query=1;
    p.ip_query_id = unique_query;
//    sent_query = unique_query;      // to check if i sent the query i am recving
    unique_query++;
    _u8 dest_mac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    mac_send_to(&p, dest_mac);
}

static void net_forward_pkt(Packet *p){
    Message("Forwarding...\n\r");
    int i,flag_found_ip,j;
    _u8 dest_mac[]={0xff,0xff,0xff,0xff,0xff,0xff};
    if(p->ip_reply == 1){
        arp_get_dest_mac(p);
        mac_send_base(p);
        return;
    }else{
        for(i=0;i<ip_count;i++){
            flag_found_ip=1;
            for(j=0;j<4;j++){
                if(table[i].ip[j]!=p->ip_dest[j])   flag_found_ip=0;
            }
            if(flag_found_ip){
                for(j=0;j<6;j++)
                    dest_mac[j]=table[i].mac[j];
                mac_send_to(p, dest_mac);
                return;
            }
        }
    }
}
static void net_send_reply(_u8 ip[4]){
    Packet p;
    memset(&p,0,sizeof(Packet));
    p.ip_reply = 1;
    int i=0;
    for(i = 0; i < 4; i++){
        p.ip_dest[i] = ip[i];
        p.ip_src[i] = ipAddressVal[i];
    }
    arp_get_dest_mac(&p);
    mac_send_base(&p);
}
_u8 last_query_id=0;
static int net_handle_pkts(Packet *p){//handles received pkts
    int flag_self_ip = 1, flag_all_ffs = 1, i = 0;
    for(i=0;i<4;i++){
        if(p->ip_dest[i] != ipAddressVal[i])   flag_self_ip = 0;
        if(p->ip_dest[i] != 0xff)              flag_all_ffs = 0;
    }
    if(flag_self_ip){//packet coming to me

        if(p->ip_reply==1){
            arp_insert_ip(p);
            return 1;
        }

    }else if(flag_all_ffs && p->ip_query == 1 && flag_function == 1){//Query coming from anyone, make sure it's not duplicated and forward, plus send a reply
        if (p->ip_query_id == last_query_id){//discard this packet
            return 0;
        }
        arp_insert_ip(p);
        last_query_id = p->ip_query_id;
        //forward
        _u8 dest_mac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
        mac_send_to(p, dest_mac);
        //send reply
        _u8 dest_ip[4];
        for(i=0;i<4;i++)dest_ip[i]=p->ip_src[i];
        net_send_reply(dest_ip);
        return 1;
    }else if(flag_function == 1){//packet going to someone else, just forward
        net_forward_pkt(p);
    }
    return 0;
}


static void app_send_query(){
    Packet p;
    memset(&p, 0, sizeof(Packet));
    _u8 dest_ip[]={0xff, 0xff, 0xff, 0xff};
    net_send_query(p, dest_ip);
}

char temp_msg[sizeof(Packet) + 8];
_u8* pktPtr;
static int mac_receive_base(Packet *p1, int timeout){
    int j = 0, i = 0, mac_notequal = 0;
    pktPtr = (_u8*)p1;
    for(j = 0; j < timeout; j++){
        memset(&temp_msg, 0, sizeof(Packet) + 8);
        sl_Recv(iSoc, temp_msg, sizeof(Packet) + 8, 0);
        for(i = 0; i < sizeof(Packet); i++)
            *(pktPtr + i) = temp_msg[i + 8];
        for(i = 0, mac_notequal = 0 ; i < 6; i++)
            if(p1->mac_dest[i] != macAddressVal[i]) mac_notequal = 1;
        if(mac_notequal == 0 ) return 1;
    }
    return 0;
}

static int mac_receive_packet(Packet *p1){//waits indefinitely for a packet, return: 0: unicast 1: broadcast
    int i = 0, mac_notequal = 0, mac_notequal_ff = 0, mac_not_from_sink = 0;
    _u8 mac_sink[] = {0xD4, 0x36, 0x39, 0x4A, 0x59, 0x6D};
    pktPtr = (_u8*)p1;
    while(1){
        memset(&temp_msg, 0, sizeof(Packet) + 8);
        sl_Recv(iSoc, temp_msg, sizeof(Packet) + 8, 0);
        for(i = 0; i < sizeof(Packet); i++)
            *(pktPtr + i) = temp_msg[i + 8];
        for(i = 0, mac_notequal = 0, mac_notequal_ff = 0, mac_not_from_sink = 0; i < 6; i++){
            if(p1->mac_dest[i] != macAddressVal[i]) mac_notequal = 1;
            if(p1->mac_dest[i] != 0xff) mac_notequal_ff = 1;
            if(p1->mac_src[i] != mac_sink[i]) mac_not_from_sink = 1;
        }

        if(mac_notequal == 0) return 0;
        if(mac_notequal_ff == 0) return 1;

        //uncomment for hardcoded away nodes
//        if(mac_notequal == 0 && mac_not_from_sink == 1) return 0;
//        if(mac_notequal_ff == 0 && mac_not_from_sink == 1) return 1;
    }
}

static int app_receive_data(){
    Packet p;
    memset(&p, 0, sizeof(Packet));
    return mac_receive_base(&p, 3);
}

#define expected_nodes_count 5
static int app_receive_replys(){
    Packet p;
    int i = 0, replys_received = 0;
    for (i = 0; i < expected_nodes_count; i++){
        mac_receive_base(&p, 20);
        replys_received += net_handle_pkts(&p);
    }
    UART_PRINT("Replys received: %d\n\r",replys_received);
    return replys_received;
}

static int get_data(int nof_loops, int inter_packet_delay, _u8 dest_mac[][6], int devices_count){
    int packtets_received_counter = 0, i = 0;
    Message("\n\rSending Query...\n\r");
    app_send_query();
    Message("\n\rWaiting for Replys...\n\r");
    app_receive_replys();
//    while (nof_loops--){
//        for(i = 0; i < devices_count; i++){
//            if(nof_loops % 50 == 0) Message(".");
//            app_send_request(dest_mac[i]);
//            packtets_received_counter += app_receive_data(); //receive_data()
//        }
//        delay(inter_packet_delay);
//    }
    return packtets_received_counter;
}

#define nof_loops 1000
#define nof_devices 3
#define nof_tests 1
#define nof_trials 50
static void sink_function(){
    int i = 0, j = 0, received_packets = 0;
    int received_packets_counter[nof_tests] = {0, 0, 0, 0};
    int inter_packet_delay[nof_tests] = {1000, 2, 3, 4};
    _u8 dest_mac[nof_devices][6] = {{0xd4, 0x36, 0x39, 0x55, 0xac, 0xac},
                          {0xd4, 0x36, 0x39, 0x55, 0xac, 0x79},
                          {0xf4, 0x5e, 0xab, 0xa1, 0xdc, 0x0f}};
    UART_PRINT("Source nodes available: %d:\n\r", nof_devices);
    UART_PRINT("Samples to take: %d\n\r", nof_loops);
    UART_PRINT("Tests: %d\n\r", nof_tests);
    UART_PRINT("Trials: %d\n\r", nof_trials);
    struct SlTimeval_t timeVal;
    timeVal.tv_sec =  0;             // Seconds
    timeVal.tv_usec = 2000;             // Microseconds. 10000 microseconds resolution
    sl_SetSockOpt(iSoc, SL_SOL_SOCKET,SL_SO_RCVTIMEO, (_u8 *)&timeVal, sizeof(timeVal));    // Enable receive timeout
    for(j = 0; j < nof_trials; j++){
        UART_PRINT("\n\r\n\r>>>>>>>>>>>>>>>>>>>>>>>> Trial #%d:\n\r", j + 1);
        for(i = 0, received_packets = 0; i < nof_tests; i++){
            UART_PRINT("\n\rStarting test #%d:\n\r\t\t", i + 1);
            UART_PRINT("Inter-sample delay: %dms\n\r\t\t", inter_packet_delay[i]);
            Message("0%                                                      100%\n\r\t\t");
            received_packets = get_data(nof_loops, inter_packet_delay[i], dest_mac, nof_devices);
//            received_packets_counter[i] += received_packets;
            Message("\n\r\tTest report:\n\r\t\t");
            UART_PRINT("Packets sent: %d\n\r\t\t", nof_loops * nof_devices);
            UART_PRINT("Packets received: %d\n\r", received_packets);
        }
        delay(1000);
    }
    Message("Final results:\n\r");
    for(i = 0; i < nof_tests; i++){
        UART_PRINT("\tTest #%d:\n\r\t\t", i + 1);
        UART_PRINT("Packets sent: %d\n\r\t\t", nof_loops * nof_devices * nof_trials);
        UART_PRINT("Packets received: %d\n\r", received_packets_counter[i]);
    }
    sl_Close(iSoc);
    UART_PRINT("\n\r\n\rDone.\n\rSink out.");
    while(1);
}

static void source_function(){
    int packets_received_counter = 0, broadcast = 0, query = 0;
    Packet p;
    Message("Waiting for incoming Querys...\n\r");
    while (1){
        broadcast = mac_receive_packet(&p);
        query = net_handle_pkts(&p);
        packets_received_counter += query && broadcast;
        UART_PRINT("Received %d packets\n\r", packets_received_counter);
//        app_send_temperature(p.mac_src);
    }
    sl_Close(iSoc);
}

static void get_my_mac(){
    unsigned char macAddressLen = SL_MAC_ADDR_LEN;
    sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &macAddressLen, (unsigned char *) macAddressVal);
    printmessage(macAddressVal, 6);
}

static void get_my_ip(){
    int i;
    for(i=0;i<4;i++)
        ipAddressVal[i]=macAddressVal[i+2];
    ipAddressVal[0]=rand()%(0x66);
    ipAddressVal[1]=rand()%(0x66);
    printmessage(ipAddressVal, 4);
}

int ReadDeviceConfiguration(){//check P018 (GPIO28)
    unsigned int uiGPIOPort;
    unsigned char pucGPIOPin;
    GPIO_IF_GetPortNPin(28, &uiGPIOPort, &pucGPIOPin);
    if(GPIO_IF_Get(28, uiGPIOPort, pucGPIOPin) == 1)//Sink mode
        return 0;
    return 1;//Source mode
}

void print_temp(){
    float fCurrentTemp;
    TMP006DrvGetTemp(&fCurrentTemp);
    UART_PRINT("\n\rCurrent Temprature: %.1f Celsius",fCurrentTemp);
}

//// unit test ARP
//static void unit_test_arp(){
//    Packet p;
//    int i;
//    memset(&p,0,sizeof(Packet));
//    for(i=0;i<4;i++){
//        p.ip_src[i]=0x55;
//        p.ip_dest[i]=0x99;
//       p.mac_src[i]=0x66;
//    }
//    // recved a packet
//    arp_insert_ip(p);
//    UART_PRINT("this is the ip in cell 0 in ARP table\t");
//   for(i=0;i<4;i++)
//        UART_PRINT("%02x",table[0].ip[i]);
//    UART_PRINT("\n\r\n\r");
//    UART_PRINT("this is the mac in cell 0 in ARP table\t");
//    for(i=0;i<4;i++)
//        UART_PRINT("%02x",table[0].mac[i]);
//    UART_PRINT("\n\r\n\r");
//    //////////////////
//    memset(&p,0,sizeof(Packet));
//    for(i=0;i<4;i++){
//            p.ip_src[i]=0x99;
//            p.ip_dest[i]=0x55;
//        }
//    //sending a packet
//    arp_get_dest_mac(&p);
//    UART_PRINT("this is the mac injected, read from the packet\t");
//    for(i=0;i<4;i++)
//        UART_PRINT("%02x",p.mac_dest[i]);
//    UART_PRINT("\n\r\n\r");
//}

void unit_test_backoff(){
    Message("\n\r////////Testing backoff//////////\n\r");
    struct SlTimeval_t timeVal;
    timeVal.tv_sec =  0;             // Seconds
    timeVal.tv_usec = 1;             // Microseconds. 10000 microseconds resolution
    sl_SetSockOpt(iSoc, SL_SOL_SOCKET,SL_SO_RCVTIMEO, (_u8 *)&timeVal, sizeof(timeVal));
    int res = 0, i = 0;
    for(i = 0; i < 10; i++){
        res = mac_listen();
        UART_PRINT("\n\r%d",res);
//        if(!res){
//            random_backoff_delay();
//        }
    }
    Message("\n\r\n\rdone test.");
}

int main(){
    BoardInit();    // Initialize Board configuration
    PinMuxConfig();    //Pin muxing
    PinConfigSet(PIN_58, PIN_STRENGTH_2MA|PIN_STRENGTH_4MA ,PIN_TYPE_STD_PD);
    InitTerm();    // Configuring UART
    InitializeAppVariables();
    I2C_IF_Open(I2C_MASTER_MODE_STD);
    TMP006DrvOpen();
    ConfigureSimpleLinkToDefaultState();
    CLR_STATUS_BIT_ALL(g_ulStatus);
    sl_Start(0, 0, 0);
    unsigned char policyVal;
    sl_WlanPolicySet(SL_POLICY_CONNECTION,SL_CONNECTION_POLICY(0, 0, 0, 0, 0), &policyVal,1 /*PolicyValLen*/);// reset all network policies
    srand((macAddressVal[0] * macAddressVal[1] * macAddressVal[2] * macAddressVal[3] * macAddressVal[4] * macAddressVal[5]) % RAND_MAX);

    Message("\33[2J\r");
    UART_PRINT("%c[H", 27);
    flag_function = ReadDeviceConfiguration();
    DisplayBanner(APPLICATION_NAME);
    get_my_mac();
    get_my_ip();
    print_temp();

    iSoc = sl_Socket(SL_AF_RF, SL_SOCK_RAW, flag_channel);
//    unit_test_backoff();
    if (flag_function) source_function();
    else sink_function();
}
