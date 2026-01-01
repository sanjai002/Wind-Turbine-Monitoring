/**
  ******************************************************************************
  * @file    app_telemetry.h
  * @author  Auto-generated
  * @brief   Header for telemetry module
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __APP_TELEMETRY_H
#define __APP_TELEMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported types ------------------------------------------------------------*/
typedef struct {
  uint32_t seq_number;
  uint32_t timestamp_ms;
  uint32_t uptime_sec;
  float rms_raw;
  float spl_db;
  uint32_t peak_amplitude;
  float zcr_rate;
  uint32_t status_flags;
  uint32_t error_count;
  uint32_t fft_band[8];
} AudioTelemetryPacket_t;

/* Exported functions ------------------------------------------------------- */
void Telemetry_Init(void);
void Telemetry_Start(void);
bool Telemetry_GetLastPacket(AudioTelemetryPacket_t *packet);

#ifdef __cplusplus
}
#endif

#endif /* __APP_TELEMETRY_H */