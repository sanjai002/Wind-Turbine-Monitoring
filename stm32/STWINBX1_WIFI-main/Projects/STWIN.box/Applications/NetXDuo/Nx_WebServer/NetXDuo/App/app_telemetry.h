/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_telemetry.h
  * @author  Wind Turbine Team
  * @brief   UDP telemetry transmission thread
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __APP_TELEMETRY_H
#define __APP_TELEMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "tx_api.h"
#include "nx_api.h"
#include "nxd_dhcp_client.h"
#include "audio_features.h"

/* Defines -------------------------------------------------------------------*/

/**
 * @brief Telemetry thread configuration
 */
#define TELEMETRY_THREAD_PRIORITY     8           /* Same as audio acq */
#define TELEMETRY_THREAD_STACK_SIZE   (3 * 1024)  /* 3 KB stack */

/**
 * @brief UDP transmission parameters
 */
#define TELEMETRY_UDP_PORT_TX         5001        /* Local UDP port (if needed) */
#define TELEMETRY_UDP_PORT_RX         5000        /* Central receiver port (example) */

/* Central receiver IP (configure as needed) */
/* By default: broadcast, but can be set to specific IP */
#define TELEMETRY_DEFAULT_IP_ADDR     0xFFFFFFFF  /* 255.255.255.255 broadcast */

/**
 * @brief Transmit interval
 */
#define TELEMETRY_TX_INTERVAL_MS      2000        /* 2 second interval */

/* Function Prototypes -------------------------------------------------------*/

/**
 * @brief Create telemetry transmission thread
 * @param byte_pool: ThreadX byte pool
 * @param ip_instance: Initialized NX_IP instance
 * @retval TX_SUCCESS on success, error code otherwise
 */
UINT Telemetry_Init(TX_BYTE_POOL *byte_pool, NX_IP *ip_instance);

/**
 * @brief Start telemetry transmission
 * @param input_queue: Queue pointer from feature extraction (AudioTelemetryPacket_t)
 * @param receiver_ip: Destination IP address for packets (network byte order)
 * @retval TX_SUCCESS on success
 */
UINT Telemetry_Start(TX_QUEUE *input_queue, ULONG receiver_ip);

/**
 * @brief Set receiver IP address and port
 * @param ip_addr: IP address in network byte order (e.g., 0xC0A80101 for 192.168.1.1)
 * @param port: UDP destination port
 * @retval TX_SUCCESS on success
 */
UINT Telemetry_SetReceiver(ULONG ip_addr, UINT port);

/**
 * @brief Enable/disable broadcast mode
 * @param enable: 1 to broadcast, 0 for unicast
 * @retval TX_SUCCESS on success
 */
UINT Telemetry_SetBroadcast(uint8_t enable);

/**
 * @brief Get transmitted packet count
 * @retval Number of packets sent
 */
uint32_t Telemetry_GetTxCount(void);

/**
 * @brief Get transmission error count
 * @retval Number of transmission failures
 */
uint32_t Telemetry_GetErrorCount(void);

/**
 * @brief Get a copy of the most recent AudioTelemetryPacket_t.
 * @param out: output buffer to fill
 * @retval 1 if a packet is available, 0 otherwise
 */
uint8_t Telemetry_GetLastPacket(AudioTelemetryPacket_t *out);

/**
 * @brief Check if socket is connected/ready
 * @retval 1 if ready, 0 if not
 */
uint8_t Telemetry_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_TELEMETRY_H */

/************************ (C) COPYRIGHT Wind Turbine Team *****END OF FILE****/
