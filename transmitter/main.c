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
#include "gpio_if.h"
//Common interface includes
#include "common.h"
#ifndef NOTERM
#include "uart_if.h"
#endif
#include "pinmux.h"

#define APPLICATION_VERSION     "1.1.1"
#define PREAMBLE            1        /* Preamble value 0- short, 1- long */
#define CPU_CYCLES_1MSEC (80*1000)


int flag_function = 0;//1: SINK, 0: SOURCE
#define flag_channel 2
#define flag_rate 5
#define flag_packets 1
#define flag_power 15
int iSoc;
#define APPLICATION_NAME        (flag_function==0?"SINK NODE":"SOURCE NODE")

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
typedef struct
{
    _u8 rate;_u8 channel;_i8 rssi;_u8 padding;_u32 timestamp;
} TransceiverRxOverHead_t;

#define delay(millis) MAP_UtilsDelay((40000/3)*millis)

void random_backoff_delay(void){
    long long i;
    int j;
    int k = rand() % 500000;//random
    UART_PRINT("This is the rand backoff \n\r");
    UART_PRINT("%d \n\r", k);
    for (i = 0; i < k; i++){ // rand number mod 2 micro seconds
        for (j = 0; j < 8; j++){ // 2 microsecond delay
        }
    }
}

//void tabulate(_u8 mac_add[6]){
//    int j, i;
//    for (j = 0; j < 6; j++)
//    {
//        Mac_array[j][i_mac_array] = mac_add[j];
//    }
//    i_mac_array++;
//
//    for (j = 0; j < 10; j++)
//    {
//        UART_PRINT("MAC Address %X : ", j);
//        for (i = 0; i < 6; i++)
//        {
//            UART_PRINT("%02x", (unsigned char) Mac_array[i][j]);
//            if (i < 5)
//            {
//                UART_PRINT(".");
//            }
//        }
//        UART_PRINT("\n\r");
//    }
//    UART_PRINT("\n\r");
//    // check global mac array for repeated mac addresses
//
//}

static void DisplayBanner(char * AppName){
    UART_PRINT("\n\r*************************************************\n\r\t\t %s       \n\r*************************************************", AppName);
}

static void printmessage(_u8 message[], int size){
    int i = 0;
    UART_PRINT("\n\r");
    for(i = 0; i < size; i++)
        UART_PRINT("%02X\t", message[i]);
    UART_PRINT("\n\r*************************************************\n\r\n\r");
}

#define msg_size 100
char msg[msg_size];
_u8 data[6];
_u8 dest_mac[6];

static void send_base(_u8 dest_mac[6],_u8 data[6]){
    memset(&msg, 0, sizeof(msg));
    int i = 0;
    for(i = 0; i < 6; i++){
        msg[i + 4] = dest_mac[i];
        msg[i + 16] = macAddressVal[i];
        msg[i + 54] = data[i];
    }
    sl_Send(iSoc, msg, msg_size,SL_RAW_RF_TX_PARAMS(flag_channel,(SlRateIndex_e)flag_rate, flag_power, PREAMBLE));
}

static void send_hello(){
    int i = 0;
    for(i = 0; i < 6; i++){
        dest_mac[i] = 0xff;
        data[i] = 0xcc;
    }
    send_base(dest_mac, data);
}

static void send_request(_u8 dest_mac[6]){
    int i = 0;
    for(i = 0; i < 6; i++) data[i] = 0xdd;
    send_base(dest_mac, data);
}

static void send_data(_u8 dest_mac[6]){
    int i = 0;
    for(i = 0; i < 6; i++) data[i] = 0xbb;
    send_base(dest_mac, data);
}

static int receive_base(_u8 dest_mac[6], _u8 data[6], int timeout){
    int j = 0, i = 0, mac_notequal = 0, data_notequal = 0;
    for(j = 0; j < timeout; j++){
        memset(&msg, 0, msg_size);
        sl_Recv(iSoc, msg, msg_size, 0);
        for(i = 0, mac_notequal = 0, data_notequal = 0; i < 6; i++){
            if(msg[12 + i] != dest_mac[i]) mac_notequal = 1;
            if(msg[62 + i] != data[i]) data_notequal = 1;
        }
        if(mac_notequal == 0 && data_notequal == 0) return 1;
    }
    return 0;
}

static int receive_data(){
    int i;
    for(i = 0; i < 6; i++){
        dest_mac[i] = macAddressVal[i];
        data[i] = 0xbb;
    }
    return receive_base(dest_mac, data, 3);
}

static int receive_request(){
    int i;
    for(i = 0; i < 6; i++){
        dest_mac[i] = macAddressVal[i];
        data[i] = 0xdd;
    }
    return receive_base(dest_mac, data, 1000);
}

static int get_data(int nof_loops, int inter_packet_delay, _u8 dest_mac[][6], int devices_count){
    int packtets_received_counter = 0, i = 0;
    while (nof_loops--){
        for(i = 0; i < devices_count; i++){
            if(nof_loops % 50 == 0) Message(".");
            send_request(dest_mac[i]);
            packtets_received_counter += receive_data();
        }
        delay(inter_packet_delay);
    }
    return packtets_received_counter;
}

#define nof_loops 1000
#define nof_devices 3
#define nof_tests 4
#define nof_trials 5
static void sink_function(){
    int i = 0, j = 0, received_packets = 0;
    int received_packets_counter[nof_tests] = {0, 0, 0, 0};
    int inter_packet_delay[nof_tests] = {2000, 4000, 6000, 8000};
    _u8 dest_mac[nof_devices][6] = {{0xd4, 0x36, 0x39, 0x55, 0xac, 0xac},
                          {0xd4, 0x36, 0x39, 0x55, 0xac, 0x79},
                          {0xf4, 0x5e, 0xab, 0xa1, 0xdc, 0x0f}};
    UART_PRINT("Source nodes available: %d:\n\r", nof_devices);
    UART_PRINT("Samples to take: %d\n\r", nof_loops);
    UART_PRINT("Tests: %d\n\r", nof_tests);
    UART_PRINT("Trials: %d\n\r", nof_trials);
    iSoc = sl_Socket(SL_AF_RF, SL_SOCK_RAW, flag_channel);
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
            received_packets_counter[i] += received_packets;
            Message("\n\r\tTest report:\n\r\t\t");
            UART_PRINT("Packets sent: %d\n\r\t\t", nof_loops * nof_devices);
            UART_PRINT("Packets received: %d\n\r", received_packets);
        }
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
    int packets_received_counter = 0;
    iSoc = sl_Socket(SL_AF_RF, SL_SOCK_RAW, flag_channel);
    _u8 dest_mac[6] = {0xf4, 0xb8, 0x5e, 0x00, 0xfe, 0x27};
    while (1){
        if(packets_received_counter % 100 == 0 && packets_received_counter > 0) UART_PRINT("Received %d packets\n\r", packets_received_counter);
        packets_received_counter += receive_request();
        send_data(dest_mac);
    }
    sl_Close(iSoc);
}

static void get_my_mac(){
    unsigned char macAddressLen = SL_MAC_ADDR_LEN;
    sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &macAddressLen, (unsigned char *) macAddressVal);
    printmessage(macAddressVal, 6);
}


int ReadDeviceConfiguration(){//check P018 (GPIO28)
    unsigned int uiGPIOPort;
    unsigned char pucGPIOPin;
    GPIO_IF_GetPortNPin(28, &uiGPIOPort, &pucGPIOPin);
    if(GPIO_IF_Get(28, uiGPIOPort, pucGPIOPin) == 1)//Sink mode
        return 0;
    return 1;//Source mode
}

int main(){
    BoardInit();    // Initialize Board configuration
    PinMuxConfig();    //Pin muxing
    InitTerm();    // Configuring UART
//    InitializeAppVariables();
//    ConfigureSimpleLinkToDefaultState();
//    CLR_STATUS_BIT_ALL(g_ulStatus);
    sl_Start(0, 0, 0);
//    unsigned char policyVal;
//    sl_WlanPolicySet(SL_POLICY_CONNECTION,SL_CONNECTION_POLICY(0, 0, 0, 0, 0), &policyVal,1 /*PolicyValLen*/);// reset all network policies

    Message("\33[2J\r");
    UART_PRINT("%c[H", 27);
    flag_function = ReadDeviceConfiguration();
    DisplayBanner(APPLICATION_NAME);
    get_my_mac();
    srand((macAddressVal[0] * macAddressVal[1] * macAddressVal[2] * macAddressVal[3] * macAddressVal[4] * macAddressVal[5]) % RAND_MAX);
    if (flag_function) source_function();
    else sink_function();
}
