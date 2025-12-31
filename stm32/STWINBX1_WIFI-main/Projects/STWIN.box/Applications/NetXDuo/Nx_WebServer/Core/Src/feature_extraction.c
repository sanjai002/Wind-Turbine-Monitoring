/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    feature_extraction.c
  * @author  Wind Turbine Team
  * @brief   Audio feature extraction thread implementation
  ******************************************************************************
  * Receives audio frames from acquisition, aggregates AUDIO_FRAMES_PER_PACKET
  * frames, computes RMS/FFT/ZCR/SPL, and outputs AudioTelemetryPacket_t
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "feature_extraction.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

/* Private defines -----------------------------------------------------------*/
#define FEATURE_EXTRACT_QUEUE_DEPTH   2  /* 2 completed packets can wait */

/* Private types -------------------------------------------------------------*/

typedef struct
{
    TX_THREAD              thread;                     /* Thread control block */
    TX_QUEUE              *input_queue;                /* From audio acquisition */
    TX_QUEUE               output_queue;               /* To telemetry sender */
    UINT                   is_running;                 /* Thread active flag */
    uint32_t               packet_count;               /* Packets generated */
    uint32_t               error_count;                /* Processing errors */
    uint32_t               seq_number;                 /* Packet sequence number */
    
    FeatureBuffer_t        feature_buffer;             /* Accumulator for frames */
    
    /* Thread resources */
    uint8_t               *thread_stack;
    uint8_t               *queue_memory;
} FeatureExtraction_Context_t;

/* Private variables ---------------------------------------------------------*/
static FeatureExtraction_Context_t feature_ctx = {0};
static uint32_t node_id = 1;  /* Default: Node 1 (can be set via config) */
static uint32_t boot_time_ms = 0;
static uint32_t last_error_flags = 0;

/* Private function prototypes -----------------------------------------------*/
static void FeatureExtraction_ThreadEntry(ULONG thread_input);
static int FeatureExtraction_ProcessBuffer(FeatureBuffer_t *buf, 
                                           AudioTelemetryPacket_t *pkt);

/**
  * @brief  Initialize feature extraction subsystem
  * @param  byte_pool: ThreadX byte pool
  * @retval TX_SUCCESS or error code
  */
UINT FeatureExtraction_Init(TX_BYTE_POOL *byte_pool)
{
    UINT status;
    
    if (!byte_pool)
        return TX_PTR_ERROR;
    
    memset(&feature_ctx, 0, sizeof(feature_ctx));
    
    /* Allocate thread stack */
    status = tx_byte_allocate(byte_pool,
                              (VOID **)&feature_ctx.thread_stack,
                              FEATURE_EXTRACT_THREAD_STACK_SIZE,
                              TX_NO_WAIT);
    if (status != TX_SUCCESS)
        return status;
    
    /* Allocate output queue storage */
    size_t queue_size = FEATURE_EXTRACT_QUEUE_DEPTH * sizeof(AudioTelemetryPacket_t);
    status = tx_byte_allocate(byte_pool,
                              (VOID **)&feature_ctx.queue_memory,
                              queue_size,
                              TX_NO_WAIT);
    if (status != TX_SUCCESS)
        return status;
    
    /* Create output queue for telemetry packets */
    status = tx_queue_create(&feature_ctx.output_queue,
                             "Feature Output Queue",
                             sizeof(AudioTelemetryPacket_t) / sizeof(ULONG),
                             feature_ctx.queue_memory,
                             queue_size);
    if (status != TX_SUCCESS)
        return status;
    
    /* Create feature extraction thread (suspended) */
    status = tx_thread_create(&feature_ctx.thread,
                              "Feature Extraction",
                              FeatureExtraction_ThreadEntry,
                              0,
                              feature_ctx.thread_stack,
                              FEATURE_EXTRACT_THREAD_STACK_SIZE,
                              FEATURE_EXTRACT_THREAD_PRIORITY,
                              FEATURE_EXTRACT_THREAD_PRIORITY,
                              TX_NO_TIME_SLICE,
                              TX_DONT_START);
    
    if (status != TX_SUCCESS)
        return status;
    
    memset(&feature_ctx.feature_buffer, 0, sizeof(feature_ctx.feature_buffer));
    feature_ctx.packet_count = 0;
    feature_ctx.error_count = 0;
    feature_ctx.seq_number = 0;
    feature_ctx.is_running = 0;
    
    return TX_SUCCESS;
}

/**
  * @brief  Start feature extraction
  * @param  input_queue: Audio frame queue from acquisition thread
  * @retval TX_SUCCESS on success
  */
UINT FeatureExtraction_Start(TX_QUEUE *input_queue)
{
    UINT status;
    
    if (!input_queue || !feature_ctx.thread_stack)
        return TX_PTR_ERROR;
    
    feature_ctx.input_queue = input_queue;
    feature_ctx.is_running = 1;
    boot_time_ms = tx_time_get();
    
    /* Resume feature extraction thread */
    status = tx_thread_resume(&feature_ctx.thread);
    
    return status;
}

/**
  * @brief  Get output queue for telemetry packets
  * @retval TX_QUEUE* or NULL
  */
TX_QUEUE* FeatureExtraction_GetOutputQueue(void)
{
    return feature_ctx.thread_stack ? &feature_ctx.output_queue : NULL;
}

/**
  * @brief  Get packet count
  * @retval Packets generated
  */
uint32_t FeatureExtraction_GetPacketCount(void)
{
    return feature_ctx.packet_count;
}

/**
  * @brief  Get error count
  * @retval Processing errors
  */
uint32_t FeatureExtraction_GetErrorCount(void)
{
    return feature_ctx.error_count;
}

/**
  * @brief  Feature extraction thread entry
  * @param  thread_input: unused
  * @retval None
  * 
  * This thread:
  * 1. Receives audio frames from acquisition queue
  * 2. Accumulates AUDIO_FRAMES_PER_PACKET frames
  * 3. Computes RMS, FFT, ZCR, SPL features
  * 4. Creates AudioTelemetryPacket_t
  * 5. Sends to output queue for UDP transmission
  */
static void FeatureExtraction_ThreadEntry(ULONG thread_input)
{
    (void)thread_input;
    AudioFrame_t frame;
    AudioTelemetryPacket_t telemetry_pkt;
    UINT status;
    
    while (1)
    {
        if (!feature_ctx.is_running)
        {
            tx_thread_suspend(&feature_ctx.thread);
            continue;
        }
        
        /* Wait for audio frame from acquisition queue (blocking, 100ms timeout) */
        status = tx_queue_receive(feature_ctx.input_queue,
                                  (VOID *)&frame,
                                  100);  /* 100ms timeout */
        
        if (status == TX_QUEUE_EMPTY)
        {
            /* No frame available, continue waiting */
            continue;
        }
        else if (status != TX_SUCCESS)
        {
            feature_ctx.error_count++;
            continue;
        }
        
        /* Copy frame samples to accumulator */
        uint32_t sample_offset = feature_ctx.feature_buffer.sample_count;
        if (sample_offset + AUDIO_FRAME_SIZE <= AUDIO_SAMPLES_PER_PACKET)
        {
            memcpy(&feature_ctx.feature_buffer.accumulated_samples[sample_offset],
                   frame.samples,
                   sizeof(frame.samples));
            
            feature_ctx.feature_buffer.sample_count += AUDIO_FRAME_SIZE;
            
            if (feature_ctx.feature_buffer.sample_count == 0)
                feature_ctx.feature_buffer.start_timestamp_ms = frame.timestamp_ms;
            
            /* Track error flags */
            if (frame.error_flags)
                last_error_flags |= frame.error_flags;
        }
        
        /* Check if we have accumulated enough frames */
        if (feature_ctx.feature_buffer.sample_count >= AUDIO_SAMPLES_PER_PACKET)
        {
            /* Process buffer and create telemetry packet */
            int result = FeatureExtraction_ProcessBuffer(&feature_ctx.feature_buffer,
                                                         &telemetry_pkt);
            
            if (result == 0)
            {
                /* Successfully created packet, queue it for transmission */
                status = tx_queue_send(&feature_ctx.output_queue,
                                       (VOID *)&telemetry_pkt,
                                       TX_NO_WAIT);
                
                if (status == TX_SUCCESS)
                {
                    feature_ctx.packet_count++;
                }
                else
                {
                    /* Output queue full, drop packet */
                    feature_ctx.error_count++;
                }
            }
            else
            {
                feature_ctx.error_count++;
            }
            
            /* Reset accumulator for next batch */
            memset(&feature_ctx.feature_buffer, 0, sizeof(feature_ctx.feature_buffer));
            last_error_flags = 0;
        }
    }
}

/**
  * @brief  Process accumulated buffer and generate telemetry packet
  * @param  buf: FeatureBuffer with accumulated samples
  * @param  pkt: Output telemetry packet structure
  * @retval 0 on success, -1 on error
  * 
  * Computes:
  * - RMS energy
  * - Zero Crossing Rate
  * - Sound Pressure Level
  * - FFT magnitude bands
  * - Peak amplitude
  */
static int FeatureExtraction_ProcessBuffer(FeatureBuffer_t *buf, 
                                           AudioTelemetryPacket_t *pkt)
{
    if (!buf || !pkt)
        return -1;
    
    if (buf->sample_count == 0)
        return -1;
    
    /* Initialize packet */
    memset(pkt, 0, sizeof(*pkt));
    
    /* Packet header */
    pkt->version = AUDIO_TELEMETRY_VERSION;
    pkt->seq_number = feature_ctx.seq_number++;
    if (feature_ctx.seq_number > 65535)
        feature_ctx.seq_number = 0;  /* Wrap around */
    
    /* Timestamp */
    uint32_t ticks_elapsed = tx_time_get() - boot_time_ms;
    pkt->timestamp_ms = buf->start_timestamp_ms;  /* Or use current time */
    
    /* Node identification */
    pkt->node_id = node_id;
    pkt->status_flags = (last_error_flags & 0xFF);
    pkt->error_count = feature_ctx.error_count & 0xFFFF;
    
    /* Uptime in seconds */
    pkt->uptime_sec = ticks_elapsed / 1000;  /* Assuming 1 tick = 1ms */
    
    /* ===== FEATURE EXTRACTION ===== */
    
    /* 1. RMS Energy */
    pkt->rms_raw = AudioFeatures_CalculateRMS(buf->accumulated_samples, 
                                              buf->sample_count);
    
    /* 2. Zero Crossing Rate */
    pkt->zcr_rate = AudioFeatures_CalculateZCR(buf->accumulated_samples,
                                               buf->sample_count);
    pkt->zcr_count = (buf->sample_count / 2);  /* Approximate count */
    
    /* 3. Peak Amplitude */
    pkt->peak_amplitude = AudioFeatures_FindPeakAmplitude(buf->accumulated_samples,
                                                          buf->sample_count);
    
    /* 4. Sound Pressure Level */
    pkt->spl_db = AudioFeatures_CalculateSPL(pkt->rms_raw, 20e-6f);
    
    /* 5. FFT Magnitude Bands */
    uint32_t fft_bands_tmp[FFT_BANDS];
    int fft_result = AudioFeatures_ComputeFFTBands(buf->accumulated_samples,
                                                   fft_bands_tmp);
    
    if (fft_result != 0)
    {
        /* FFT computation failed, but continue with available data */
        memset(fft_bands_tmp, 0, sizeof(fft_bands_tmp));
    }

    /* Copy into packed packet field as a plain byte copy to avoid alignment issues. */
    memcpy(pkt->fft_band, fft_bands_tmp, sizeof(pkt->fft_band));
    
    return 0;
}

/************************ (C) COPYRIGHT Wind Turbine Team *****END OF FILE****/
