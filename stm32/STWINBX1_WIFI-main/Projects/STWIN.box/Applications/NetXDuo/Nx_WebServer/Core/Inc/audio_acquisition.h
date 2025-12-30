/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    audio_acquisition.h
  * @author  Wind Turbine Team
  * @brief   Audio acquisition thread using STWINBX1 microphone
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __AUDIO_ACQUISITION_H
#define __AUDIO_ACQUISITION_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "tx_api.h"
#include "audio_features.h"

/* Defines -------------------------------------------------------------------*/

/**
 * @brief Thread configuration
 */
#define AUDIO_ACQ_THREAD_PRIORITY    8           /* Medium priority (higher = more urgent) */
#define AUDIO_ACQ_THREAD_STACK_SIZE  (2 * 1024)  /* 2 KB stack */
#define AUDIO_ACQ_QUEUE_DEPTH        4           /* Allow 4 pending frames */

/**
 * @brief Queue message structure
 * Contains one frame of audio data ready for feature extraction
 */
typedef struct
{
    int16_t  samples[AUDIO_FRAME_SIZE];      /* 512 PCM samples */
    uint32_t timestamp_ms;                   /* Frame capture timestamp */
    uint32_t frame_number;                   /* Sequential frame counter */
    uint8_t  error_flags;                    /* Error/status flags */
} AudioFrame_t;

/* Status Flags */
#define AUDIO_ACQ_ERROR_DMA        0x01       /* DMA transfer error */
#define AUDIO_ACQ_ERROR_OVERFLOW   0x02       /* Buffer overflow */
#define AUDIO_ACQ_ERROR_CLIPPING   0x04       /* ADC clipping detected */

/* Function Prototypes -------------------------------------------------------*/

/**
 * @brief Create audio acquisition thread and queue
 * @param byte_pool: ThreadX byte pool for memory allocation
 * @retval TX_SUCCESS on success, error code otherwise
 */
UINT AudioAcquisition_Init(TX_BYTE_POOL *byte_pool);

/**
 * @brief Start audio capture (called after ThreadX kernel starts)
 * @retval TX_SUCCESS on success
 */
UINT AudioAcquisition_Start(void);

/**
 * @brief Get queue for audio frame messages
 * @retval Pointer to TX_QUEUE control block
 */
TX_QUEUE* AudioAcquisition_GetQueue(void);

/**
 * @brief Get current frame number (for synchronization)
 * @retval Current frame counter
 */
uint32_t AudioAcquisition_GetFrameCount(void);

/**
 * @brief Check if microphone is capturing
 * @retval 1 if capturing, 0 if idle/error
 */
uint8_t AudioAcquisition_IsActive(void);

/**
 * @brief Get accumulated error count
 * @retval Number of acquisition errors since init
 */
uint32_t AudioAcquisition_GetErrorCount(void);

#ifdef __cplusplus
}
#endif

#endif /* __AUDIO_ACQUISITION_H */

/************************ (C) COPYRIGHT Wind Turbine Team *****END OF FILE****/
