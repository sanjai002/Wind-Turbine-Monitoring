/*
 * Compatibility shim for missing ADF/MDF headers.
 * This file intentionally provides only minimal forward declarations and
 * macro stubs so the STWIN.box BSP audio driver can compile until the
 * proper STM32 ADF/MDF middleware is added to the project.
 *
 * IMPORTANT: Replace this file with the official ADF/MDF headers from
 * the STM32 package for correct audio functionality. This shim is
 * compile-time only and does NOT implement real audio behavior.
 */
#ifndef STWIN_BOX_AUDIO_H_COMPAT_H
#define STWIN_BOX_AUDIO_H_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Include the user's audio.h (the standard generic audio driver header)
   located in Drivers/BSP/Components if present. */
#include "../Components/audio.h"

/* Minimal MDF/ADF type stubs and macro definitions used by the BSP.
   Only added when not already defined by real MDF/ADF HAL headers. */
#ifndef MDF_HANDLE_TYPEDEF
#define MDF_HANDLE_TYPEDEF
typedef struct
{
  void *Instance;
  struct {
    struct {
      uint32_t InterleavedFilters;
      uint32_t ProcClockDivider;
      struct { uint32_t Activation; } OutputClock;
    } CommonParam;
    struct { uint32_t Activation; } SerialInterface;
  } Init;
} MDF_HandleTypeDef;
#endif /* MDF_HANDLE_TYPEDEF */

#ifndef MDF_FILTER_CONFIG_TYPEDEF
#define MDF_FILTER_CONFIG_TYPEDEF
typedef struct
{
  struct { uint32_t Activation; uint32_t DecimationRatio; } ReshapeFilter;
  uint32_t CicMode;
  int32_t  Gain;
  uint32_t DecimationRatio;
  uint32_t DataSource;
  uint32_t Delay;
  uint32_t Offset;
  struct { uint32_t Activation; uint32_t CutOffFrequency; } HighPassFilter;
  struct { uint32_t Activation; } Integrator;
  struct { uint32_t Activation; } SoundActivity;
  uint32_t FifoThreshold;
  uint32_t DiscardSamples;
  uint32_t AcquisitionMode;
  struct { uint32_t Source; uint32_t Edge; } Trigger;
} MDF_FilterConfigTypeDef;
#endif /* MDF_FILTER_CONFIG_TYPEDEF */

#ifndef MDF_DMA_CONFIG_TYPEDEF
#define MDF_DMA_CONFIG_TYPEDEF
typedef struct
{
  uint32_t Address;
  uint32_t DataLength;
  uint32_t MsbOnly;
} MDF_DmaConfigTypeDef;
#endif /* MDF_DMA_CONFIG_TYPEDEF */

/* Minimal DMA queue/node stubs (real types come from HAL DMA headers). */
#ifndef DMA_QLIST_TYPEDEF
#define DMA_QLIST_TYPEDEF
typedef struct { void *dummy; } DMA_QListTypeDef;
#endif
#ifndef DMA_NODE_TYPEDEF
#define DMA_NODE_TYPEDEF
typedef struct { void *dummy; } DMA_NodeTypeDef;
#endif

/* Minimal macro stubs for MDF constants used by the BSP. Values are placeholders
   and should be replaced by the real definitions from the official headers. */
#ifndef MDF_RSF_DECIMATION_RATIO_4
#define MDF_RSF_DECIMATION_RATIO_4 4U
#endif
#ifndef MDF_ONE_FILTER_SINC4
#define MDF_ONE_FILTER_SINC4 1U
#endif
#ifndef MDF_ONE_FILTER_SINC5
#define MDF_ONE_FILTER_SINC5 2U
#endif
#ifndef MDF_TWO_FILTERS_MCIC_SINC3
#define MDF_TWO_FILTERS_MCIC_SINC3 3U
#endif
#ifndef MDF_HPF_CUTOFF_0_000625FPCM
#define MDF_HPF_CUTOFF_0_000625FPCM 0U
#endif
#ifndef MDF_FIFO_THRESHOLD_NOT_EMPTY
#define MDF_FIFO_THRESHOLD_NOT_EMPTY 0U
#endif
#ifndef MDF_MODE_ASYNC_CONT
#define MDF_MODE_ASYNC_CONT 0U
#endif
#ifndef MDF_MODE_SYNC_CONT
#define MDF_MODE_SYNC_CONT 1U
#endif
#ifndef MDF_FILTER_TRIG_ADF_TRGO
#define MDF_FILTER_TRIG_ADF_TRGO 0U
#endif
#ifndef MDF_FILTER_TRIG_RISING_EDGE
#define MDF_FILTER_TRIG_RISING_EDGE 0U
#endif
#ifndef MDF_DATA_SOURCE_ADCITF1
#define MDF_DATA_SOURCE_ADCITF1 0U
#endif

#ifdef __cplusplus
}
#endif

#endif /* STWIN_BOX_AUDIO_H_COMPAT_H */
