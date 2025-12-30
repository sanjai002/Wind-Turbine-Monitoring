/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    feature_extraction.h
  * @author  Wind Turbine Team
  * @brief   Audio feature extraction thread
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __FEATURE_EXTRACTION_H
#define __FEATURE_EXTRACTION_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "tx_api.h"
#include "audio_features.h"
#include "audio_acquisition.h"

/* Defines -------------------------------------------------------------------*/

/**
 * @brief Thread configuration
 */
#define FEATURE_EXTRACT_THREAD_PRIORITY   7           /* Medium-high priority */
#define FEATURE_EXTRACT_THREAD_STACK_SIZE (4 * 1024)  /* 4 KB stack for DSP */

/**
 * @brief Feature packet buffer for aggregation
 * Collects AUDIO_FRAMES_PER_PACKET frames before sending
 */
typedef struct
{
    int16_t           accumulated_samples[AUDIO_SAMPLES_PER_PACKET];
    uint32_t          sample_count;
    uint32_t          start_timestamp_ms;
    uint32_t          frame_count;
} FeatureBuffer_t;

/* Function Prototypes -------------------------------------------------------*/

/**
 * @brief Create feature extraction thread
 * @param byte_pool: ThreadX byte pool for memory allocation
 * @retval TX_SUCCESS on success, error code otherwise
 */
UINT FeatureExtraction_Init(TX_BYTE_POOL *byte_pool);

/**
 * @brief Start feature extraction processing
 * @param input_queue: Queue pointer from audio acquisition
 * @retval TX_SUCCESS on success
 */
UINT FeatureExtraction_Start(TX_QUEUE *input_queue);

/**
 * @brief Get queue that outputs completed feature packets
 * @retval Pointer to TX_QUEUE control block (outputs AudioTelemetryPacket_t)
 */
TX_QUEUE* FeatureExtraction_GetOutputQueue(void);

/**
 * @brief Get packet count generated since start
 * @retval Number of complete packets processed
 */
uint32_t FeatureExtraction_GetPacketCount(void);

/**
 * @brief Get error count during extraction
 * @retval Number of extraction errors
 */
uint32_t FeatureExtraction_GetErrorCount(void);

#ifdef __cplusplus
}
#endif

#endif /* __FEATURE_EXTRACTION_H */

/************************ (C) COPYRIGHT Wind Turbine Team *****END OF FILE****/
