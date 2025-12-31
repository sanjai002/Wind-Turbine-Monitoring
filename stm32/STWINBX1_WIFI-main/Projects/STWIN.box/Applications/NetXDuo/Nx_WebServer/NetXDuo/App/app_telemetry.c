/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_telemetry.c
  * @author  Wind Turbine Team
  * @brief   UDP telemetry transmission thread
  ******************************************************************************
  * Receives AudioTelemetryPacket_t from feature extraction and transmits
  * via UDP to a central receiver. Handles IP connectivity and packet retries.
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_telemetry.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

/* Private defines -----------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/

typedef struct
{
    TX_THREAD              thread;                     /* Thread control block */
    TX_QUEUE              *input_queue;                /* From feature extraction */
    NX_IP                 *ip_instance;                /* NetX IP instance */
    NX_UDP_SOCKET          udp_socket;                 /* UDP socket */
    UINT                   is_ready;                   /* Socket ready flag */
    UINT                   is_running;                 /* Thread active flag */
    
    ULONG                  receiver_ip;                /* Destination IP */
    UINT                   receiver_port;              /* Destination port */
    uint8_t                use_broadcast;              /* Broadcast vs unicast */
    
    uint32_t               tx_count;                   /* Packets sent */
    uint32_t               error_count;                /* Transmission errors */
    
    /* Thread resources */
    uint8_t               *thread_stack;
} Telemetry_Context_t;

/* Private variables ---------------------------------------------------------*/
static Telemetry_Context_t telemetry_ctx = {0};

/* Cache the latest packet for the web dashboard (best-effort, no blocking). */
static AudioTelemetryPacket_t telemetry_last_pkt;
static volatile uint8_t telemetry_last_pkt_valid = 0;

/* Private function prototypes -----------------------------------------------*/
static void Telemetry_ThreadEntry(ULONG thread_input);
static UINT Telemetry_CreateSocket(void);
static UINT Telemetry_TransmitPacket(const AudioTelemetryPacket_t *pkt);

/**
  * @brief  Initialize telemetry transmission subsystem
  * @param  byte_pool: ThreadX byte pool
  * @param  ip_instance: Initialized NX_IP instance
  * @retval TX_SUCCESS or error code
  */
UINT Telemetry_Init(TX_BYTE_POOL *byte_pool, NX_IP *ip_instance)
{
    UINT status;
    
    if (!byte_pool || !ip_instance)
        return TX_PTR_ERROR;
    
    memset(&telemetry_ctx, 0, sizeof(telemetry_ctx));
    
    telemetry_ctx.ip_instance = ip_instance;
    telemetry_ctx.receiver_ip = TELEMETRY_DEFAULT_IP_ADDR;
    telemetry_ctx.receiver_port = TELEMETRY_UDP_PORT_RX;
    telemetry_ctx.use_broadcast = 1;  /* Default to broadcast */
    
    /* Allocate thread stack */
    status = tx_byte_allocate(byte_pool,
                              (VOID **)&telemetry_ctx.thread_stack,
                              TELEMETRY_THREAD_STACK_SIZE,
                              TX_NO_WAIT);
    if (status != TX_SUCCESS)
        return status;
    
    /* Create telemetry transmission thread (suspended) */
    status = tx_thread_create(&telemetry_ctx.thread,
                              "Telemetry TX",
                              Telemetry_ThreadEntry,
                              0,
                              telemetry_ctx.thread_stack,
                              TELEMETRY_THREAD_STACK_SIZE,
                              TELEMETRY_THREAD_PRIORITY,
                              TELEMETRY_THREAD_PRIORITY,
                              TX_NO_TIME_SLICE,
                              TX_DONT_START);
    
    if (status != TX_SUCCESS)
        return status;
    
    telemetry_ctx.is_ready = 0;
    telemetry_ctx.is_running = 0;
    telemetry_ctx.tx_count = 0;
    telemetry_ctx.error_count = 0;
    
    return TX_SUCCESS;
}

/**
  * @brief  Start telemetry transmission
  * @param  input_queue: Feature extraction output queue
  * @param  receiver_ip: Destination IP (network byte order)
  * @retval TX_SUCCESS on success
  */
UINT Telemetry_Start(TX_QUEUE *input_queue, ULONG receiver_ip)
{
    UINT status;
    
    if (!input_queue)
        return TX_PTR_ERROR;
    
    telemetry_ctx.input_queue = input_queue;
    if (receiver_ip != 0)
        telemetry_ctx.receiver_ip = receiver_ip;
    
    /* Create and bind UDP socket */
    status = Telemetry_CreateSocket();
    if (status != NX_SUCCESS)
        return status;
    
    telemetry_ctx.is_ready = 1;
    telemetry_ctx.is_running = 1;
    
    /* Resume telemetry thread */
    status = tx_thread_resume(&telemetry_ctx.thread);
    
    return status;
}

/**
  * @brief  Set receiver IP and port
  * @param  ip_addr: IP address in network byte order
  * @param  port: UDP port number
  * @retval TX_SUCCESS on success
  */
UINT Telemetry_SetReceiver(ULONG ip_addr, UINT port)
{
    if (ip_addr == 0)
        return TX_PTR_ERROR;
    
    telemetry_ctx.receiver_ip = ip_addr;
    telemetry_ctx.receiver_port = port;
    
    return TX_SUCCESS;
}

/**
  * @brief  Enable/disable broadcast mode
  * @param  enable: 1 for broadcast, 0 for unicast
  * @retval TX_SUCCESS on success
  */
UINT Telemetry_SetBroadcast(uint8_t enable)
{
    telemetry_ctx.use_broadcast = enable;
    return TX_SUCCESS;
}

/**
  * @brief  Get transmitted packet count
  * @retval Packet count
  */
uint32_t Telemetry_GetTxCount(void)
{
    return telemetry_ctx.tx_count;
}

/**
  * @brief  Get error count
  * @retval Error count
  */
uint32_t Telemetry_GetErrorCount(void)
{
    return telemetry_ctx.error_count;
}

/**
  * @brief  Copy out the latest telemetry packet (for HTTP dashboard).
  * @param  out: destination buffer
  * @retval 1 if a valid packet was copied, 0 if none available yet
  */
uint8_t Telemetry_GetLastPacket(AudioTelemetryPacket_t *out)
{
    if (!out || !telemetry_last_pkt_valid)
        return 0;

    /* Best-effort copy, allow race with telemetry thread. */
    memcpy(out, &telemetry_last_pkt, sizeof(*out));
    return 1;
}

/**
  * @brief  Check if socket is ready
  * @retval 1 if ready, 0 if not
  */
uint8_t Telemetry_IsReady(void)
{
    return telemetry_ctx.is_ready;
}

/**
  * @brief  Telemetry transmission thread entry
  * @param  thread_input: unused
  * @retval None
  * 
  * This thread:
  * 1. Waits for AudioTelemetryPacket_t from feature extraction queue
  * 2. Transmits via UDP socket
  * 3. Handles retries and error conditions
  */
static void Telemetry_ThreadEntry(ULONG thread_input)
{
    (void)thread_input;
    AudioTelemetryPacket_t pkt;
    UINT status;
    /* Reserved for future event-driven refactor (currently unused). */
    
    while (1)
    {
        if (!telemetry_ctx.is_running)
        {
            tx_thread_suspend(&telemetry_ctx.thread);
            continue;
        }
        
        if (!telemetry_ctx.is_ready)
        {
            /* Socket not ready, wait for IP connectivity */
            tx_thread_sleep(500);  /* 500ms wait */
            continue;
        }
        
        /* Wait for telemetry packet from feature extraction (blocking, 100ms timeout) */
        status = tx_queue_receive(telemetry_ctx.input_queue,
                                  (VOID *)&pkt,
                                  100);  /* 100ms timeout */
        
        if (status == TX_QUEUE_EMPTY)
        {
            /* No packet available, continue waiting */
            continue;
        }
        else if (status != TX_SUCCESS)
        {
            telemetry_ctx.error_count++;
            continue;
        }
        
        /* Transmit the packet */
        status = Telemetry_TransmitPacket(&pkt);

    /* Update cached packet for the dashboard (even if TX fails). */
    memcpy(&telemetry_last_pkt, &pkt, sizeof(telemetry_last_pkt));
    telemetry_last_pkt_valid = 1;
        
        if (status == NX_SUCCESS)
        {
            telemetry_ctx.tx_count++;
        }
        else
        {
            telemetry_ctx.error_count++;
            printf("Telemetry TX error: 0x%02X\n", status);
        }
    }
}

/**
  * @brief  Create and bind UDP socket
  * @retval NX_SUCCESS on success, error code otherwise
  * 
  * Creates a UDP socket bound to local port TELEMETRY_UDP_PORT_TX
  */
static UINT Telemetry_CreateSocket(void)
{
    UINT status;
    
    if (!telemetry_ctx.ip_instance)
        return NX_PTR_ERROR;
    
    /* Create UDP socket */
    status = nx_udp_socket_create(telemetry_ctx.ip_instance,
                                  &telemetry_ctx.udp_socket,
                                  "Telemetry Socket",
                                  NX_IP_NORMAL,
                                  NX_DONT_FRAGMENT,
                                  NX_IP_TIME_TO_LIVE,
                                  2048);
    
    if (status != NX_SUCCESS)
    {
        printf("UDP socket creation failed: 0x%02X\n", status);
        return status;
    }
    
    /* Bind to local port (optional - can use ephemeral port) */
    status = nx_udp_socket_bind(&telemetry_ctx.udp_socket,
                                TELEMETRY_UDP_PORT_TX,
                                TX_WAIT_FOREVER);
    
    if (status != NX_SUCCESS)
    {
        printf("UDP socket bind failed: 0x%02X\n", status);
        nx_udp_socket_delete(&telemetry_ctx.udp_socket);
        return status;
    }
    
    return NX_SUCCESS;
}

/**
  * @brief  Transmit a telemetry packet via UDP
  * @param  pkt: AudioTelemetryPacket_t to transmit
  * @retval NX_SUCCESS on success, error code otherwise
  * 
  * Packet format:
  * - UDP payload: exactly sizeof(AudioTelemetryPacket_t) = 64 bytes
  * - Destination: telemetry_ctx.receiver_ip : telemetry_ctx.receiver_port
  * - Mode: broadcast or unicast based on configuration
  */
static UINT Telemetry_TransmitPacket(const AudioTelemetryPacket_t *pkt)
{
    UINT status;
    NX_PACKET *packet_ptr;
    
    if (!pkt || !telemetry_ctx.ip_instance)
        return NX_PTR_ERROR;
    
    /* Allocate packet from IP instance's default packet pool */
    /* Note: Must use pool that was created for this IP instance */
    status = nx_packet_allocate(telemetry_ctx.ip_instance->nx_ip_default_packet_pool,
                                &packet_ptr,
                                NX_UDP_PACKET,
                                TX_WAIT_FOREVER);
    
    if (status != NX_SUCCESS)
    {
        telemetry_ctx.error_count++;
        return status;
    }
    
    /* Copy telemetry packet data to payload */
    memcpy(packet_ptr->nx_packet_prepend_ptr,
           (const VOID *)pkt,
           sizeof(AudioTelemetryPacket_t));
    
    packet_ptr->nx_packet_length = sizeof(AudioTelemetryPacket_t);
    packet_ptr->nx_packet_append_ptr = packet_ptr->nx_packet_prepend_ptr + 
                                        packet_ptr->nx_packet_length;
    
    /* Transmit via UDP */
    if (telemetry_ctx.use_broadcast)
    {
        /* Broadcast mode: send to 255.255.255.255 */
        status = nx_udp_socket_send(&telemetry_ctx.udp_socket,
                                    packet_ptr,
                                    0xFFFFFFFF,  /* Broadcast address */
                                    telemetry_ctx.receiver_port);
    }
    else
    {
        /* Unicast mode: send to specific IP */
        status = nx_udp_socket_send(&telemetry_ctx.udp_socket,
                                    packet_ptr,
                                    telemetry_ctx.receiver_ip,
                                    telemetry_ctx.receiver_port);
    }
    
    if (status != NX_SUCCESS)
    {
        printf("UDP send failed: 0x%02X (IP: 0x%08lX, Port: %u)\n", 
               status, telemetry_ctx.receiver_ip, telemetry_ctx.receiver_port);
        nx_packet_release(packet_ptr);
        return status;
    }
    
    return NX_SUCCESS;
}

/************************ (C) COPYRIGHT Wind Turbine Team *****END OF FILE****/
