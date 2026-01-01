/**
  ******************************************************************************
  * @file    app_telemetry.c
  * @author  Wind Turbine Team
  * @brief   Telemetry module implementation with UDP transmission thread
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "app_telemetry.h"
#include "tx_api.h"
#include "nx_api.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "app_netxduo.h"

/* External declarations -----------------------------------------------------*/
extern NX_PACKET_POOL AppPool;

/* Private defines -----------------------------------------------------------*/
#define TELEMETRY_UDP_PORT 5001
#define TELEMETRY_MAX_PACKETS 10
#ifndef TELEMETRY_THREAD_STACK_SIZE
#define TELEMETRY_THREAD_STACK_SIZE (2 * 1024)
#endif
#define TELEMETRY_THREAD_PRIORITY   8

/* Private types -------------------------------------------------------------*/
typedef struct {
  AudioTelemetryPacket_t packets[TELEMETRY_MAX_PACKETS];
  uint32_t write_index;
  uint32_t read_index;
  uint32_t packet_count;
  TX_MUTEX mutex;
} TelemetryBuffer_t;

/* Private variables ---------------------------------------------------------*/
static TelemetryBuffer_t telemetry_buffer;
static NX_UDP_SOCKET udp_socket;
static uint32_t sequence_number = 0;
static uint32_t tx_count = 0;
static uint32_t error_count = 0;
static bool telemetry_initialized = false;
static bool telemetry_started = false;
static ULONG receiver_ip_address = 0;

/* Telemetry thread resources */
static TX_THREAD telemetry_thread;
static uint8_t *telemetry_thread_stack = NULL;
static TX_QUEUE *telemetry_input_queue = NULL;
static TX_BYTE_POOL *telemetry_byte_pool = NULL;

/* Private function prototypes -----------------------------------------------*/
static void Telemetry_BufferInit(void);
static uint8_t Telemetry_BufferPut(const AudioTelemetryPacket_t *packet);
static uint8_t Telemetry_BufferGet(AudioTelemetryPacket_t *packet);
static void Telemetry_ThreadEntry(ULONG thread_input);
static UINT Telemetry_SendUDP(const AudioTelemetryPacket_t *packet);

/**
  * @brief  Initialize telemetry module
  * @param  byte_pool: ThreadX byte pool for memory allocation
  * @param  ip_instance: NetX IP instance (unused, kept for API compatibility)
  * @retval TX_SUCCESS or error code
  */
UINT Telemetry_Init(TX_BYTE_POOL *byte_pool, NX_IP *ip_instance)
{
  UINT status;
  (void)ip_instance;  /* IP instance accessed via extern IpInstance */

  if (telemetry_initialized) {
    return TX_SUCCESS;
  }

  if (!byte_pool) {
    return TX_PTR_ERROR;
  }

  telemetry_byte_pool = byte_pool;

  /* Allocate thread stack */
  status = tx_byte_allocate(byte_pool,
                            (VOID **)&telemetry_thread_stack,
                            TELEMETRY_THREAD_STACK_SIZE,
                            TX_NO_WAIT);
  if (status != TX_SUCCESS) {
    return status;
  }

  Telemetry_BufferInit();

  telemetry_initialized = true;
  return TX_SUCCESS;
}

/**
  * @brief  Start telemetry transmission thread
  * @param  input_queue: Queue from feature extraction (AudioTelemetryPacket_t)
  * @param  receiver_ip: Destination IP (0xFFFFFFFF for broadcast)
  * @retval TX_SUCCESS or error code
  */
UINT Telemetry_Start(TX_QUEUE *input_queue, ULONG receiver_ip)
{
  UINT status;
  NX_IP *ip_ptr = &IpInstance;

  if (!telemetry_initialized || !input_queue) {
    return TX_PTR_ERROR;
  }

  /* Store input queue reference */
  telemetry_input_queue = input_queue;
  receiver_ip_address = receiver_ip;

  /* Create UDP socket */
  status = nx_udp_socket_create(ip_ptr, &udp_socket, "Telemetry Socket",
                               NX_IP_NORMAL, NX_FRAGMENT_OKAY,
                               NX_IP_TIME_TO_LIVE, 128);
  if (status != NX_SUCCESS) {
    printf("Telemetry: UDP socket create failed: 0x%02X\n", status);
    return status;
  }

  /* Bind to port */
  status = nx_udp_socket_bind(&udp_socket, TELEMETRY_UDP_PORT, TX_WAIT_FOREVER);
  if (status != NX_SUCCESS) {
    printf("Telemetry: UDP socket bind failed: 0x%02X\n", status);
    return status;
  }

  /* Create telemetry thread to consume from input queue */
  status = tx_thread_create(&telemetry_thread,
                            "Telemetry Thread",
                            Telemetry_ThreadEntry,
                            0,
                            telemetry_thread_stack,
                            TELEMETRY_THREAD_STACK_SIZE,
                            TELEMETRY_THREAD_PRIORITY,
                            TELEMETRY_THREAD_PRIORITY,
                            TX_NO_TIME_SLICE,
                            TX_AUTO_START);
  if (status != TX_SUCCESS) {
    printf("Telemetry: Thread create failed: 0x%02X\n", status);
    return status;
  }

  telemetry_started = true;
  printf("Telemetry: Started on UDP port %d (broadcast)\n", TELEMETRY_UDP_PORT);

  return TX_SUCCESS;
}

/**
  * @brief  Telemetry thread entry - consumes from feature extraction queue
  * @param  thread_input: unused
  * @retval None
  */
static void Telemetry_ThreadEntry(ULONG thread_input)
{
  (void)thread_input;
  AudioTelemetryPacket_t packet;
  UINT status;

  printf("Telemetry thread running\n");

  while (1)
  {
    /* Wait for packet from feature extraction (blocking, 500ms timeout) */
    status = tx_queue_receive(telemetry_input_queue,
                              (VOID *)&packet,
                              500);  /* 500ms timeout */

    if (status == TX_SUCCESS)
    {
      /* Assign sequence number */
      packet.seq_number = sequence_number++;

      /* Send via UDP */
      if (Telemetry_SendUDP(&packet) == NX_SUCCESS)
      {
        tx_count++;
      }
      else
      {
        error_count++;
      }

      /* Store in circular buffer for HTTP access */
      Telemetry_BufferPut(&packet);
    }
    else if (status != TX_QUEUE_EMPTY)
    {
      /* Queue error */
      error_count++;
    }
    /* TX_QUEUE_EMPTY is normal timeout, just continue */
  }
}

/**
  * @brief  Send telemetry packet via UDP broadcast
  * @param  packet: Packet to send
  * @retval NX_SUCCESS or error code
  */
static UINT Telemetry_SendUDP(const AudioTelemetryPacket_t *packet)
{
  NX_PACKET *nx_packet;
  UINT status;

  if (!telemetry_started || !packet) {
    return NX_NOT_ENABLED;
  }

  /* Allocate NetX packet */
  status = nx_packet_allocate(&AppPool, &nx_packet, NX_UDP_PACKET, 100);
  if (status != NX_SUCCESS) {
    return status;
  }

  /* Append telemetry data */
  status = nx_packet_data_append(nx_packet, (void*)packet, sizeof(AudioTelemetryPacket_t),
                                &AppPool, 100);
  if (status != NX_SUCCESS) {
    nx_packet_release(nx_packet);
    return status;
  }

  /* Send UDP broadcast */
  status = nx_udp_socket_send(&udp_socket, nx_packet, 
                              IP_ADDRESS(255,255,255,255), 
                              TELEMETRY_UDP_PORT);
  if (status != NX_SUCCESS) {
    nx_packet_release(nx_packet);
  }

  return status;
}

/**
  * @brief  Get the last telemetry packet (for HTTP server)
  * @param  packet: Pointer to packet structure to fill
  * @retval 1 if packet available, 0 otherwise
  */
uint8_t Telemetry_GetLastPacket(AudioTelemetryPacket_t *packet)
{
  if (!telemetry_initialized || packet == NULL) {
    return 0;
  }

  /* Return most recent packet from buffer (peek, not remove) */
  tx_mutex_get(&telemetry_buffer.mutex, TX_WAIT_FOREVER);

  if (telemetry_buffer.packet_count > 0) {
    /* Get most recent packet (write_index - 1, with wraparound) */
    uint32_t last_index = (telemetry_buffer.write_index + TELEMETRY_MAX_PACKETS - 1) % TELEMETRY_MAX_PACKETS;
    *packet = telemetry_buffer.packets[last_index];
    tx_mutex_put(&telemetry_buffer.mutex);
    return 1;
  }

  tx_mutex_put(&telemetry_buffer.mutex);
  return 0;
}

/**
  * @brief  Get transmitted packet count
  * @retval Number of packets sent via UDP
  */
uint32_t Telemetry_GetTxCount(void)
{
  return tx_count;
}

/**
  * @brief  Get transmission error count
  * @retval Number of transmission failures
  */
uint32_t Telemetry_GetErrorCount(void)
{
  return error_count;
}

/**
  * @brief  Check if telemetry is ready
  * @retval 1 if ready, 0 otherwise
  */
uint8_t Telemetry_IsReady(void)
{
  return telemetry_started ? 1 : 0;
}

/**
  * @brief  Initialize telemetry buffer
  * @retval None
  */
static void Telemetry_BufferInit(void)
{
  telemetry_buffer.write_index = 0;
  telemetry_buffer.read_index = 0;
  telemetry_buffer.packet_count = 0;

  tx_mutex_create(&telemetry_buffer.mutex, "Telemetry Mutex", TX_INHERIT);
}

/**
  * @brief  Put packet in circular buffer (overwrites oldest if full)
  * @param  packet: Packet to store
  * @retval 1 on success
  */
static uint8_t Telemetry_BufferPut(const AudioTelemetryPacket_t *packet)
{
  tx_mutex_get(&telemetry_buffer.mutex, TX_WAIT_FOREVER);

  telemetry_buffer.packets[telemetry_buffer.write_index] = *packet;
  telemetry_buffer.write_index = (telemetry_buffer.write_index + 1) % TELEMETRY_MAX_PACKETS;

  if (telemetry_buffer.packet_count < TELEMETRY_MAX_PACKETS) {
    telemetry_buffer.packet_count++;
  } else {
    /* Buffer full, advance read index (overwrite oldest) */
    telemetry_buffer.read_index = (telemetry_buffer.read_index + 1) % TELEMETRY_MAX_PACKETS;
  }

  tx_mutex_put(&telemetry_buffer.mutex);

  return 1;
}

/**
  * @brief  Get packet from circular buffer
  * @param  packet: Pointer to packet structure to fill
  * @retval 1 if packet available, 0 otherwise
  * @note   Currently unused but retained for potential future use
  */
__attribute__((unused))
static uint8_t Telemetry_BufferGet(AudioTelemetryPacket_t *packet)
{
  uint8_t result = 0;

  tx_mutex_get(&telemetry_buffer.mutex, TX_WAIT_FOREVER);

  if (telemetry_buffer.packet_count > 0) {
    *packet = telemetry_buffer.packets[telemetry_buffer.read_index];
    telemetry_buffer.read_index = (telemetry_buffer.read_index + 1) % TELEMETRY_MAX_PACKETS;
    telemetry_buffer.packet_count--;
    result = 1;
  }

  tx_mutex_put(&telemetry_buffer.mutex);

  return result;
}

/************************ (C) COPYRIGHT Wind Turbine Team *****END OF FILE****/