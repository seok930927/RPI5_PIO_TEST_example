/**
 * Copyright (c) 2021 WIZnet Co.,Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * ----------------------------------------------------------------------------------------------------
 * Includes
 * ----------------------------------------------------------------------------------------------------
 */
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
// #include "port_common.h"
#include "piolib.h"
#include "wizchip_qspi_pio.pio.h"
#include "pio_func.h"
#include "wizchip_conf.h"
#include "wizchip_spi.h"
#include "loopback.h"
#include "wizchip_conf.h"
#include "wizchip_spi.h"

#include "socket.h"

#include "cJSON.h" // JSON handling library
#include "iperf.h"

/**
 * ----------------------------------------------------------------------------------------------------
 * Macros
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
 #define PLL_SYS_KHZ (133 * 1000)
// #define PLL_SYS_KHZ (90 * 1000)

/* Buffer */
#define ETHERNET_BUF_MAX_SIZE (1024 * 32 / 2)

/* Socket */
#define SOCKET_DATA 0
#define SOCKET_CTRL 1

/* Port */
#define PORT_IPERF 5201

#define MAX_RESULT_LEN 1024

/* Cookie size */
#define COOKIE_SIZE 37

/* iperf3 Commands */
#define PARAM_EXCHANGE 9
#define CREATE_STREAMS 10
#define TEST_START 1
#define TEST_RUNNING 2
#define TEST_END 4
#define EXCHANGE_RESULTS 13
#define DISPLAY_RESULTS 14
#define IPERF_DONE 16
// Main While loop 종료 핸들러

static volatile int keep_running = 1;

void signal_handler(int sig) {
    (void)sig;
    printf("\n시그널 받음, 종료 중...\n");
    pio_sm_set_enabled(pio_struct.pio, pio_struct.sm,false);
    keep_running = 0;
}
/**
 * Variables
 * ----------------------------------------------------------------------------------------------------
 * ----------------------------------------------------------------------------------------------------
 */
/* Network */
static wiz_NetInfo g_net_info =
    {
        .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x01}, // MAC address
        .ip = {192, 168, 11, 103},                     // IP address
        .sn = {255, 255, 255, 0},                    // Subnet Mask
        .gw = {192, 168, 11, 1},                     // Gateway
        .dns = {8, 8, 8, 8},                         // DNS server
#if _WIZCHIP_ > W5500  
        .lla = {0xfe, 0x80, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x02, 0x08, 0xdc, 0xff,
                0xfe, 0x57, 0x57, 0x25},             // Link Local Address
        .gua = {0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00},             // Global Unicast Address
        .sn6 = {0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00},             // IPv6 Prefix
        .gw6 = {0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00},             // Gateway IPv6 Address
        .dns6 = {0x20, 0x01, 0x48, 0x60,
                0x48, 0x60, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x88, 0x88},             // DNS6 server
        .ipmode = NETINFO_STATIC_ALL
#else
        .dhcp = NETINFO_STATIC        
#endif
};

/* iperf */
static uint8_t g_iperf_buf[ETHERNET_BUF_MAX_SIZE * 2] = {
    0,
};
static uint8_t cookie[COOKIE_SIZE] = {0};

uint8_t dest_ip[4];
uint16_t destport;

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
static void set_clock_khz(void);
void handle_param_exchange(bool *reverse, bool *udp);
void handle_create_streams(bool udp, uint8_t *dest_ip, uint16_t destport);
void start_iperf_test(Stats *stats, bool reverse, bool udp, uint8_t *dest_ip, uint16_t destport);
void exchange_results(Stats *stats);

/**
 * ----------------------------------------------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------------------------------------------
 */



 

int main()
{

    /* Initialize */
    uint8_t socket_status;
    Stats stats;
    
    bool reverse = false;
    bool udp = false;

    // set_clock_khz();
    /* 강제종료를 막고 안전한 자원해제를 위함 */
    signal(SIGINT, signal_handler); // Ctrl+C 시그널 처리
    signal(SIGTERM, signal_handler);// 종료 시그널 처리 
    
    pio_open_lihan(&pio_struct);
    wizchip_initialize();




    network_initialize(g_net_info);

    /* Get network information */
    print_network_information(g_net_info);

    socket(SOCKET_CTRL, Sn_MR_TCP, PORT_IPERF, 0x20 );
    listen(SOCKET_CTRL);

    while (1)
    {
        iperf_stats_init(&stats, 1000);

        socket_status = getSn_SR(SOCKET_CTRL);

        if (socket_status == SOCK_ESTABLISHED)
        {
            handle_param_exchange(&reverse, &udp);
            handle_create_streams(udp, dest_ip, destport);

            if (reverse)
            {
                memset(g_iperf_buf, 0xAA, ETHERNET_BUF_MAX_SIZE / 2);
            }
            
            start_iperf_test(&stats, reverse, udp, dest_ip, PORT_IPERF);
        } 
        else if (socket_status == SOCK_CLOSE_WAIT)
        {
            disconnect(SOCKET_CTRL);
            disconnect(SOCKET_DATA);
        } 
        else if (socket_status == SOCK_CLOSED)
        {
            socket(SOCKET_CTRL, Sn_MR_TCP, PORT_IPERF, 0);
            listen(SOCKET_CTRL);
        }
    }
}

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */


void handle_param_exchange(bool *reverse, bool *udp) 
{
    char buffer[512] = {0};
    uint8_t cmd;
    uint16_t len = 0;
    uint8_t raw_len[4] = {0};
    int cookie_len;
    cJSON *json;
    cJSON *reverseItem;
    cJSON *udpItem;

    cookie_len = recv(SOCKET_CTRL, cookie, COOKIE_SIZE);
    if (cookie_len != COOKIE_SIZE)
    {
        printf("[iperf] Failed to receive cookie. Received: %d bytes\n", cookie_len);
        return;
    }

#ifdef IPERF_DEBUG
    printf("[iperf] Received cookie: %s\n", cookie);
#endif

    cmd = PARAM_EXCHANGE;
    send(SOCKET_CTRL, &cmd, 1);
    recv(SOCKET_CTRL, raw_len, 4);

    len = (raw_len[0] << 24) | (raw_len[1] << 16) | (raw_len[2] << 8) | raw_len[3];
#ifdef IPERF_DEBUG
    printf("[iperf] Raw length bytes: 0x%02X 0x%02X 0x%02X 0x%02X, Parsed length: %d\n", raw_len[0], raw_len[1], raw_len[2], raw_len[3], len);
#endif

    recv(SOCKET_CTRL, (uint8_t *)buffer, len);
    buffer[len] = '\0'; // Null-terminate

#ifdef IPERF_DEBUG
    printf("[iperf] Received parameters: %s\n", buffer);
#endif

    json = cJSON_Parse(buffer);
    if (json == NULL)
    {
        printf("[iperf] Failed to parse JSON: %s\n", cJSON_GetErrorPtr());
    }
    else
    {
        reverseItem = cJSON_GetObjectItem(json, "reverse");
        udpItem = cJSON_GetObjectItem(json, "udp");

        *reverse = (reverseItem && cJSON_IsBool(reverseItem)) ? reverseItem->valueint : 0;
        *udp = (udpItem && cJSON_IsBool(udpItem)) ? udpItem->valueint : 0;

#ifdef IPERF_DEBUG
        printf("[iperf] Parsed JSON: %s\n", cJSON_Print(json));
        printf("[iperf] Parsed JSON: reverse=%d, udp=%d\n", *reverse, *udp);
#endif
        cJSON_Delete(json);
    }
}

void handle_create_streams(bool udp, uint8_t *dest_ip, uint16_t destport)
{
    uint8_t cmd = CREATE_STREAMS;
    uint8_t received = 0;

    send(SOCKET_CTRL, &cmd, 1);

    if(udp)
    {
        socket(SOCKET_DATA, Sn_MR_UDP, PORT_IPERF, 0);

        uint8_t handshake_buffer[4];
        recvfrom(SOCKET_DATA, handshake_buffer, sizeof(handshake_buffer), dest_ip, &destport);

        printf("[iperf] Received UDP handshake from %d.%d.%d.%d:%d\n",dest_ip[0], dest_ip[1], dest_ip[2], dest_ip[3], destport);

        // 클라이언트에게 응답 전송
        uint8_t handshake_msg[4] = {0x12, 0x34, 0x56, 0x78};
        sendto(SOCKET_DATA, handshake_msg, sizeof(handshake_msg), dest_ip, destport);
    }
    else 
    {
        socket(SOCKET_DATA, Sn_MR_TCP, PORT_IPERF, 0x20);
        listen(SOCKET_DATA);

        // Wait for client to connect to data socket
        while (getSn_SR(SOCKET_DATA) != SOCK_ESTABLISHED)
        {
            if (getSn_SR(SOCKET_DATA) == SOCK_CLOSED)
            {
                printf("[iperf] Data socket closed unexpectedly.\n");
                return;
            }
        }
        received = recv(SOCKET_DATA, cookie, COOKIE_SIZE);
    } 

#ifdef IPERF_DEBUG
    if (received > 0)
    {
        printf("[iperf] Received data cookie: %s\n", cookie);
    }
#endif
}

void start_iperf_test(Stats *stats, bool reverse, bool udp, uint8_t *dest_ip, uint16_t destport)
{
    uint8_t cmd = 0;
    uint32_t pack_len = 0;
    uint16_t sent_bytes = 0;
    uint16_t recv_bytes = 0;

    // Start test
    cmd = TEST_START;
    send(SOCKET_CTRL, &cmd, 1);

    // Running test
    cmd = TEST_RUNNING;
    send(SOCKET_CTRL, &cmd, 1);

    iperf_stats_start(stats);

    while (stats->running)
    {
        if (getSn_RX_RSR(SOCKET_CTRL) > 0)
        {
            recv(SOCKET_CTRL, &cmd, 1);
            if (cmd == TEST_END)
            {
                stats->running = false;
                break;
            }
        }

        if (reverse)
        {
            if(udp)
            {
                sent_bytes = sendto(SOCKET_DATA, g_iperf_buf, ETHERNET_BUF_MAX_SIZE / 2, dest_ip, destport);
            }
            else
            {
                sent_bytes = send(SOCKET_DATA, g_iperf_buf, ETHERNET_BUF_MAX_SIZE / 2);
            }
            
            iperf_stats_add_bytes(stats, sent_bytes);
        }
        else
        {
            getsockopt(SOCKET_DATA, SO_RECVBUF, &pack_len);
            if(pack_len > 0)
            {
                if(udp)
                {
                    recv_bytes = recvfrom(SOCKET_DATA, (uint8_t *)g_iperf_buf, ETHERNET_BUF_MAX_SIZE - 1, dest_ip, (uint16_t*)&destport);
                }
                else
                {
                    recv_bytes = recv(SOCKET_DATA, (uint8_t *)g_iperf_buf, ETHERNET_BUF_MAX_SIZE -1 );
                }

                iperf_stats_add_bytes(stats, recv_bytes);
            }
            else if (pack_len == 0)
            {
                iperf_stats_update(stats, false);
            }
            else
            {
                printf("[iperf] Error during data reception\n");
                break;
            }
        }
        iperf_stats_update(stats, false);
    }
    iperf_stats_stop(stats);

    exchange_results(stats);
}

void exchange_results(Stats *stats) 
{
    uint8_t cmd = EXCHANGE_RESULTS;
    uint32_t result_len = 0;
    uint8_t length_bytes[4];
    char buffer[1024];
    char *results_str;
    uint32_t results_len;
    cJSON *results;
    cJSON *streams;
    cJSON *stream;

    // Ask to exchange results
    send(SOCKET_CTRL, &cmd, 1);

    // Receive client results
    recv(SOCKET_CTRL, (uint8_t *)&result_len, 4);
    result_len = (result_len << 24) | ((result_len << 8) & 0x00FF0000) | ((result_len >> 8) & 0x0000FF00) | (result_len >> 24); // Convert to host-endian

    if (result_len > sizeof(buffer))
    {
        printf("[iperf] Received result length exceeds buffer size.\n");
        return;
    }

    recv(SOCKET_CTRL, (uint8_t *)buffer, result_len);
    buffer[result_len] = '\0'; // Null-terminate the received JSON data
#ifdef IPERF_DEBUG
    printf("[iperf] Client results received: %s\n", buffer);
#endif

    // Prepare server results
    results = cJSON_CreateObject();
    cJSON_AddNumberToObject(results, "cpu_util_total", 0);
    cJSON_AddNumberToObject(results, "cpu_util_user", 0);
    cJSON_AddNumberToObject(results, "cpu_util_system", 0);
    cJSON_AddNumberToObject(results, "sender_has_retransmits", 0);

    // Streams object
    streams = cJSON_CreateArray();
    stream = cJSON_CreateObject();
    cJSON_AddNumberToObject(stream, "id", 1);
    cJSON_AddNumberToObject(stream, "bytes", stats->nb0);
    cJSON_AddNumberToObject(stream, "retransmits", 0);
    cJSON_AddNumberToObject(stream, "jitter", 0);
    cJSON_AddNumberToObject(stream, "errors", 0);
    cJSON_AddNumberToObject(stream, "packets", stats->np0);
    cJSON_AddNumberToObject(stream, "start_time", 0);
    cJSON_AddNumberToObject(stream, "end_time", (double)(stats->t3 - stats->t0) / 1000000.0);
    cJSON_AddItemToArray(streams, stream);
    cJSON_AddItemToObject(results, "streams", streams);

    // Serialize JSON to string
    results_str = cJSON_PrintUnformatted(results);
    results_len = strlen(results_str);

    // Send server results
    length_bytes[0] = (results_len >> 24) & 0xFF;
    length_bytes[1] = (results_len >> 16) & 0xFF;
    length_bytes[2] = (results_len >> 8) & 0xFF;
    length_bytes[3] = results_len & 0xFF;

    send(SOCKET_CTRL, length_bytes, 4);
    send(SOCKET_CTRL, (uint8_t *)results_str, results_len);

    cJSON_Delete(results);

    // Ask to display results
    cmd = DISPLAY_RESULTS;
    send(SOCKET_CTRL, &cmd, 1);

    // Wait for IPERF_DONE command
    recv(SOCKET_CTRL, &cmd, 1);
    if (cmd == IPERF_DONE)
    {
        printf("[iperf] Test completed successfully.\n");
    }
    else
    {
        printf("[iperf] Unexpected command received: %d\n", cmd);
    }
}