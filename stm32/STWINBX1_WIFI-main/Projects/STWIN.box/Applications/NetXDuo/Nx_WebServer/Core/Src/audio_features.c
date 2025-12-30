/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    audio_features.c
  * @author  Wind Turbine Team
  * @brief   Audio feature extraction implementation
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "audio_features.h"
#include <math.h>
#include <string.h>

/* Private defines -----------------------------------------------------------*/
#define Q15_MAX           32767
#define Q15_MIN          -32768

/* Reference pressure for SPL calculation (20 microPascals = 0 dB) */
#define SPL_REF_PRESSURE  20e-6f

/**
  * @brief  Initialize audio feature extraction engine
  * @retval HAL status (0 = success)
  */
uint32_t AudioFeatures_Init(void)
{
    /* Feature extraction is purely software-based, no hardware init needed */
    return 0;
}

/**
  * @brief  Calculate RMS energy from PCM samples
  * @param  samples: pointer to int16_t PCM samples
  * @param  count: number of samples
  * @retval RMS value in Q15 format (0-32767)
  * 
  * RMS = sqrt(sum(x[n]^2) / N)
  * Returned as Q15 (16-bit fixed point: 0 = 0.0, 32767 = ~1.0)
  */
uint16_t AudioFeatures_CalculateRMS(const int16_t *samples, uint32_t count)
{
    if (!samples || count == 0)
        return 0;
    
    /* Use double accumulator to avoid overflow */
    double sum = 0.0;
    
    for (uint32_t i = 0; i < count; i++)
    {
        int32_t s = (int32_t)samples[i];
        sum += (double)s * s;
    }
    
    double rms = sqrt(sum / (double)count);
    
    /* Normalize to Q15: divide by max sample value (32768) */
    double normalized_rms = rms / 32768.0;
    
    /* Saturate to Q15 range [0, 32767] */
    if (normalized_rms > 0.99999)
        return Q15_MAX;
    
    uint16_t result = (uint16_t)(normalized_rms * Q15_MAX);
    return result;
}

/**
  * @brief  Calculate Zero Crossing Rate
  * @param  samples: pointer to int16_t PCM samples
  * @param  count: number of samples
  * @retval ZCR as percentage of Nyquist rate (0-100)
  * 
  * ZCR = number of sign changes in signal
  * Normalized to percentage of theoretical maximum (50% for white noise)
  */
uint16_t AudioFeatures_CalculateZCR(const int16_t *samples, uint32_t count)
{
    if (!samples || count < 2)
        return 0;
    
    uint32_t zero_crossings = 0;
    
    for (uint32_t i = 1; i < count; i++)
    {
        /* Check for sign change between consecutive samples */
        if ((samples[i-1] >= 0 && samples[i] < 0) ||
            (samples[i-1] < 0 && samples[i] >= 0))
        {
            zero_crossings++;
        }
    }
    
    /* Normalize: max ZCR is ~0.5 for white noise (one crossing per 2 samples) */
    /* Express as percentage of Nyquist (100 = sample rate / 2) */
    uint16_t zcr_percent = (uint16_t)((zero_crossings * 100) / count);
    
    /* Clamp to 100% */
    if (zcr_percent > 100)
        zcr_percent = 100;
    
    return zcr_percent;
}

/**
  * @brief  Calculate SPL (Sound Pressure Level)
  * @param  rms: RMS value in Q15 format
  * @param  ref_pressure: Reference pressure level (20e-6 Pa for 0 dB)
  * @retval SPL in dB (typical range 0-120 for audio)
  * 
  * SPL_dB = 20 * log10(pressure / reference_pressure)
  */
uint16_t AudioFeatures_CalculateSPL(uint16_t rms_q15, float ref_pressure)
{
    /* Convert RMS from Q15 to linear pressure (assuming ADC Vref = 3.3V, gain calibration) */
    /* For typical microphone: 1 LSB = ~95 dB SPL at full scale */
    float pressure = (float)rms_q15 * ref_pressure / Q15_MAX;
    
    if (pressure < 1e-7f)
        pressure = 1e-7f; /* Avoid log of zero */
    
    /* SPL = 20 * log10(pressure / ref_pressure) */
    float spl = 20.0f * log10f(pressure / ref_pressure);
    
    /* Offset to dB-SPL scale (typical: add ~84 for microphone calibration) */
    /* This value should be calibrated per hardware setup */
    spl += 84.0f;
    
    /* Clamp to 0-120 dB range */
    if (spl < 0.0f)
        spl = 0.0f;
    if (spl > 120.0f)
        spl = 120.0f;
    
    return (uint16_t)spl;
}

/**
  * @brief  Find peak amplitude in sample buffer
  * @param  samples: pointer to int16_t PCM samples
  * @param  count: number of samples
  * @retval Peak absolute value (0-32767)
  */
uint16_t AudioFeatures_FindPeakAmplitude(const int16_t *samples, uint32_t count)
{
    if (!samples || count == 0)
        return 0;
    
    uint16_t peak = 0;
    
    for (uint32_t i = 0; i < count; i++)
    {
        int16_t abs_val = samples[i];
        if (abs_val < 0)
            abs_val = -abs_val;
        
        if ((uint16_t)abs_val > peak)
            peak = (uint16_t)abs_val;
    }
    
    return peak;
}

/**
  * @brief  Perform FFT and compute magnitude bands
  * @param  samples: pointer to int16_t PCM samples (FFT_SIZE = 512 samples required)
  * @param  bands: output array for FFT_BANDS magnitude values (8 bands)
  * @retval 0 on success, non-zero on error
  * 
  * This is a simplified implementation using Goertzel algorithm per frequency bin
  * For production, use optimized FFT library (e.g., CMSIS-DSP)
  */
int AudioFeatures_ComputeFFTBands(const int16_t *samples, uint32_t *bands)
{
    if (!samples || !bands)
        return -1;
    
    /* Initialize band magnitudes to zero */
    memset(bands, 0, FFT_BANDS * sizeof(uint32_t));
    
    /* 
     * Simplified band allocation (for 512 FFT @ 16kHz):
     * Band 0: 0-1000 Hz
     * Band 1: 1000-2000 Hz
     * Band 2: 2000-3000 Hz
     * ... up to Band 7: 7000-8000 Hz (Nyquist = 8000 Hz)
     */
    
    const uint32_t SAMPLE_RATE = 16000;
    /* NOTE: FFT_SIZE is a macro in audio_features.h, so don't redeclare it here. */
    const uint32_t bin_width = SAMPLE_RATE / FFT_SIZE;  /* ~31 Hz per bin */
    const uint32_t BAND_WIDTH = 1000;  /* 1 kHz per band */
    const uint32_t bins_per_band = BAND_WIDTH / bin_width;  /* ~32 bins per band */
    
    /* Goertzel algorithm: compute magnitude for each band */
    for (int band = 0; band < FFT_BANDS; band++)
    {
        uint32_t magnitude = 0;
    uint32_t bin_start = (uint32_t)band * bins_per_band;
    uint32_t bin_end = (uint32_t)(band + 1) * bins_per_band;
        
        if (bin_end > FFT_SIZE / 2)
            bin_end = FFT_SIZE / 2;  /* Nyquist limit */
        
        /* Simplified: sum energy in frequency bin range */
        double band_energy = 0.0;
        for (uint32_t k = bin_start; k < bin_end && k < FFT_SIZE / 2; k++)
        {
            /* Simplified Goertzel coefficient (real implementation uses complex math) */
            double freq = (double)k * (double)bin_width;
            double omega = 2.0 * M_PI * freq / SAMPLE_RATE;
            
            double s_prev = 0.0, s_curr = 0.0, s_next = 0.0;
            double coeff = 2.0 * cos(omega);
            
            for (uint32_t n = 0; n < FFT_SIZE; n++)
            {
                s_next = (double)samples[n] + coeff * s_curr - s_prev;
                s_prev = s_curr;
                s_curr = s_next;
            }
            
            /* Magnitude squared */
            double real = s_curr - s_prev * cos(omega);
            double imag = s_prev * sin(omega);
            band_energy += (real * real + imag * imag);
        }
        
        /* Normalize and scale (0-1000000 range) */
        magnitude = (uint32_t)(sqrt(band_energy) / 1000.0);
        if (magnitude > 1000000)
            magnitude = 1000000;
        
        bands[band] = magnitude;
    }
    
    return 0;
}

/************************ (C) COPYRIGHT Wind Turbine Team *****END OF FILE****/
