/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    audio_features.h
  * @author  Wind Turbine Team
  * @brief   Audio feature extraction and telemetry packet definitions
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __AUDIO_FEATURES_H
#define __AUDIO_FEATURES_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Defines -------------------------------------------------------------------*/
/* Telemetry packet version (compatibility) */
#define AUDIO_TELEMETRY_VERSION    0x01

/* Audio parameters */
#define AUDIO_SAMPLE_RATE          16000      /* 16 kHz */
#define AUDIO_FRAME_SIZE           512        /* samples per frame */
#define AUDIO_FRAMES_PER_PACKET    4          /* 4 frames = 2 seconds @ 16kHz */
#define AUDIO_SAMPLES_PER_PACKET   (AUDIO_FRAME_SIZE * AUDIO_FRAMES_PER_PACKET)

/* FFT configuration */
#define FFT_SIZE                   512        /* FFT bins */
#define FFT_BANDS                  8          /* Reduced to 8 bands for compact packet */

/* Struct Packing and Binary Safety ==========================================*/
/* Ensure struct is tightly packed and portable */

/**
 * @brief Audio Feature Packet Structure
 * 
 * Binary packet layout for UDP telemetry:
 * - Total size: 64 bytes (portable, no padding issues)
 * - All fields are explicitly aligned
 * - Endian-safe: all multi-byte values stored as little-endian via explicit conversion
 * - Version field ensures backward compatibility
 * 
 * Typical packet cycle: ~2 seconds (8 frames of 512 samples each @ 16kHz)
 */
typedef struct __attribute__((packed))
{
    /* Packet Header: 4 bytes */
    uint8_t  version;              /* Protocol version (0x01) */
    uint8_t  reserved1;            /* Reserved for future use */
    uint16_t seq_number;           /* Sequence counter (0-65535) */
    
    /* Timestamps: 4 bytes */
    uint32_t timestamp_ms;         /* Milliseconds since boot */
    
  /* RMS Energy: 4 bytes */
  uint16_t rms_raw;              /* RMS in fixed-point Q15 format (0-32767) */
  uint16_t rms_reserved;         /* Reserved (keep packet fixed-size) */
    
    /* Zero Crossing Rate: 4 bytes */
    uint16_t zcr_count;            /* Zero crossing count in frame */
    uint16_t zcr_rate;             /* ZCR as percentage of Nyquist (0-100) */
    
    /* SPL (Sound Pressure Level): 4 bytes */
    uint16_t spl_db;               /* SPL in dB (0-120), offset by 20*log10(Pref) */
    uint16_t peak_amplitude;       /* Peak absolute value in current frame */
    
    /* FFT Magnitude Bands: 32 bytes (8 bands * 4 bytes each) */
    uint32_t fft_band[FFT_BANDS];  /* Band magnitudes (normalized 0-1000000) */
    
  /* Node Identification: 8 bytes */
    uint8_t  node_id;              /* Node identifier (1-3) */
    uint8_t  status_flags;         /* Status flags (bit 0: error, bit 1: clipping) */
    uint16_t error_count;          /* Cumulative error count since last reset */
    uint32_t uptime_sec;           /* Seconds since node boot */

  /* Reserved: 4 bytes (keeps struct == 64 bytes even with compiler padding edge cases) */
  uint32_t reserved3;
    
} AudioTelemetryPacket_t;

/* Compile-time check for packet size */
_Static_assert(sizeof(AudioTelemetryPacket_t) == 64, "AudioTelemetryPacket_t must be exactly 64 bytes");

/* Status Flags */
#define AUDIO_STATUS_ERROR_FLAG    0x01       /* Set if acquisition error occurred */
#define AUDIO_STATUS_CLIPPING_FLAG 0x02       /* Set if ADC clipping detected */

/* Function Prototypes -------------------------------------------------------*/

/**
 * @brief Initialize audio feature extraction engine
 * @retval HAL status
 */
uint32_t AudioFeatures_Init(void);

/**
 * @brief Calculate RMS from PCM samples
 * @param samples: pointer to int16_t PCM samples
 * @param count: number of samples
 * @retval RMS value in Q15 format (0-32767)
 */
uint16_t AudioFeatures_CalculateRMS(const int16_t *samples, uint32_t count);

/**
 * @brief Calculate Zero Crossing Rate
 * @param samples: pointer to int16_t PCM samples
 * @param count: number of samples
 * @retval ZCR as percentage of Nyquist rate (0-100)
 */
uint16_t AudioFeatures_CalculateZCR(const int16_t *samples, uint32_t count);

/**
 * @brief Calculate SPL (Sound Pressure Level)
 * @param rms: RMS value in Q15 format
 * @param ref_pressure: Reference pressure level (typically 20e-6 Pa for 0dB)
 * @retval SPL in dB (0-120)
 */
uint16_t AudioFeatures_CalculateSPL(uint16_t rms, float ref_pressure);

/**
 * @brief Perform FFT and compute magnitude bands
 * @param samples: pointer to int16_t PCM samples (FFT_SIZE required)
 * @param bands: output array for FFT_BANDS magnitude values
 * @retval 0 on success, non-zero on error
 */
int AudioFeatures_ComputeFFTBands(const int16_t *samples, uint32_t *bands);

/**
 * @brief Find peak amplitude in sample buffer
 * @param samples: pointer to int16_t PCM samples
 * @param count: number of samples
 * @retval Peak absolute value (0-32767)
 */
uint16_t AudioFeatures_FindPeakAmplitude(const int16_t *samples, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif /* __AUDIO_FEATURES_H */

/************************ (C) COPYRIGHT Wind Turbine Team *****END OF FILE****/
