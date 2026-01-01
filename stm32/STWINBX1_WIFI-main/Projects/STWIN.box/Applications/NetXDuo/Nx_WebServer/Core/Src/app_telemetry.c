/**
  ******************************************************************************
  * @file    app_telemetry.c
  * @author  Auto-generated
  * @brief   Telemetry module implementation
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "app_telemetry.h"
#include "tx_api.h"
#include "nx_api.h"

/* Private defines -----------------------------------------------------------*/
#define TELEMETRY_UDP_PORT 12345
#define TELEMETRY_MAX_PACKETS 10

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
static bool telemetry_initialized = false;

/* Private function prototypes -----------------------------------------------*/
static void Telemetry_BufferInit(void);
static bool Telemetry_BufferPut(const AudioTelemetryPacket_t *packet);
static bool Telemetry_BufferGet(AudioTelemetryPacket_t *packet);

/**
  * @brief  Initialize telemetry module
  * @param  None
  * @retval None
  */
void Telemetry_Init(void)
{
  if (telemetry_initialized) {
    return;
  }

  Telemetry_BufferInit();
  telemetry_initialized = true;
}

/**
  * @brief  Start telemetry transmission
  * @param  None
  * @retval None
  */
void Telemetry_Start(void)
{
  UINT status;
  NX_IP *ip_ptr = &IpInstance; // From app_netxduo.h

  /* Create UDP socket */
  status = nx_udp_socket_create(ip_ptr, &udp_socket, "Telemetry Socket",
                               NX_IP_NORMAL, NX_FRAGMENT_OKAY,
                               NX_IP_TIME_TO_LIVE, 128);

  if (status != NX_SUCCESS) {
    return;
  }

  /* Bind to telemetry port */
  status = nx_udp_socket_bind(&udp_socket, TELEMETRY_UDP_PORT, TX_WAIT_FOREVER);

  if (status != NX_SUCCESS) {
    nx_udp_socket_delete(&udp_socket);
    return;
  }
}

/**
  * @brief  Get the last telemetry packet
  * @param  packet: Pointer to packet structure to fill
  * @retval true if packet available, false otherwise
  */
bool Telemetry_GetLastPacket(AudioTelemetryPacket_t *packet)
{
  if (!telemetry_initialized || packet == NULL) {
    return false;
  }

  return Telemetry_BufferGet(packet);
}

/**
  * @brief  Add a telemetry packet to the buffer (called by feature extraction)
  * @param  packet: Packet to add
  * @retval true if successful, false if buffer full
  */
bool Telemetry_AddPacket(const AudioTelemetryPacket_t *packet)
{
  if (!telemetry_initialized || packet == NULL) {
    return false;
  }

  AudioTelemetryPacket_t pkt = *packet;
  pkt.seq_number = sequence_number++;

  /* Send via UDP broadcast */
  if (telemetry_initialized) {
    NX_PACKET *nx_packet;
    UINT status;

    status = nx_packet_allocate(&AppPool, &nx_packet, NX_UDP_PACKET, TX_WAIT_FOREVER);
    if (status == NX_SUCCESS) {
      status = nx_packet_data_append(nx_packet, (void*)&pkt, sizeof(AudioTelemetryPacket_t),
                                    &AppPool, TX_WAIT_FOREVER);
      if (status == NX_SUCCESS) {
        /* Broadcast to all hosts on network */
        nx_udp_socket_send(&udp_socket, nx_packet, IP_ADDRESS(255,255,255,255), TELEMETRY_UDP_PORT);
      } else {
        nx_packet_release(nx_packet);
      }
    }
  }

  return Telemetry_BufferPut(&pkt);
}

/**
  * @brief  Initialize telemetry buffer
  * @param  None
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
  * @brief  Put packet in circular buffer
  * @param  packet: Packet to store
  * @retval true if successful, false if buffer full
  */
static bool Telemetry_BufferPut(const AudioTelemetryPacket_t *packet)
{
  bool result = false;

  tx_mutex_get(&telemetry_buffer.mutex, TX_WAIT_FOREVER);

  if (telemetry_buffer.packet_count < TELEMETRY_MAX_PACKETS) {
    telemetry_buffer.packets[telemetry_buffer.write_index] = *packet;
    telemetry_buffer.write_index = (telemetry_buffer.write_index + 1) % TELEMETRY_MAX_PACKETS;
    telemetry_buffer.packet_count++;
    result = true;
  }

  tx_mutex_put(&telemetry_buffer.mutex);

  return result;
}

/**
  * @brief  Get packet from circular buffer
  * @param  packet: Pointer to packet structure to fill
  * @retval true if packet available, false otherwise
  */
static bool Telemetry_BufferGet(AudioTelemetryPacket_t *packet)
{
  bool result = false;

  tx_mutex_get(&telemetry_buffer.mutex, TX_WAIT_FOREVER);

  if (telemetry_buffer.packet_count > 0) {
    *packet = telemetry_buffer.packets[telemetry_buffer.read_index];
    telemetry_buffer.read_index = (telemetry_buffer.read_index + 1) % TELEMETRY_MAX_PACKETS;
    telemetry_buffer.packet_count--;
    result = true;
  }

  tx_mutex_put(&telemetry_buffer.mutex);

  return result;
}