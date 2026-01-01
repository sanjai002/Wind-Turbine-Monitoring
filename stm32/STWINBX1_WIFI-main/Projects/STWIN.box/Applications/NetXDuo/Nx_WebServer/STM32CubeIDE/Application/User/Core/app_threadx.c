/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @author  MCD Application Team
  * @brief   ThreadX applicative file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "audio_acquisition.h"
#include "feature_extraction.h"
#include "app_telemetry.h"
#include "app_netxduo.h"
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Thread initialization order (critical for proper startup) */
#define INIT_ORDER_THREADX      1   /* ThreadX kernel ready */
#define INIT_ORDER_NETXDUO      2   /* NetX Duo IP stack */
#define INIT_ORDER_AUDIO_ACQ    3   /* Audio acquisition */
#define INIT_ORDER_FEATURE      4   /* Feature extraction */
#define INIT_ORDER_TELEMETRY    5   /* Telemetry transmission */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
static TX_BYTE_POOL *g_byte_pool = NULL;
static TX_THREAD g_startup_thread;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static void App_Startup_Thread_Entry(ULONG input);

/* USER CODE END FPP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer (pointer to UCHAR array from CubeMX)
  * @retval TX_SUCCESS on success, error code otherwise
  * 
  * Thread initialization order (CRITICAL):
  * 1. NetX Duo initialization (from app_netxduo.c)
  * 2. Audio acquisition thread creation
  * 3. Feature extraction thread creation
  * 4. Telemetry transmission thread creation
  * 5. Startup thread triggers IP connectivity and starts all worker threads
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;

   /* USER CODE BEGIN App_ThreadX_MEM_POOL */
  g_byte_pool = byte_pool;
  printf("ThreadX App Initialization Started\n");
  
  /* Initialize audio acquisition thread and queue */
  ret = AudioAcquisition_Init(byte_pool);
  if (ret != TX_SUCCESS)
  {
    printf("AudioAcquisition_Init failed: 0x%02X\n", ret);
    return ret;
  }
  printf("Audio Acquisition initialized\n");
  
  /* Initialize feature extraction thread and queue */
  ret = FeatureExtraction_Init(byte_pool);
  if (ret != TX_SUCCESS)
  {
    printf("FeatureExtraction_Init failed: 0x%02X\n", ret);
    return ret;
  }
  printf("Feature Extraction initialized\n");
  
  /* Note: Telemetry_Init requires NX_IP instance which is initialized
     in MX_NetXDuo_Init. So we'll call it from a startup thread after
     NetX Duo is ready. This function must be called by app_netxduo.c
     at the END of MX_NetXDuo_Init. */
  
  /* USER CODE END App_ThreadX_MEM_POOL */

  /* USER CODE BEGIN App_ThreadX_Init */
  printf("All ThreadX application threads created successfully\n");
  /* USER CODE END App_ThreadX_Init */

  return ret;
}

/**
  * @brief  Create startup synchronization thread
  * @param  byte_pool: ThreadX byte pool
  * @retval TX_SUCCESS on success
  * 
  * This function should be called from MX_NetXDuo_Init AFTER the IP instance
  * has been created. It creates a startup thread that waits for IP assignment
  * and then starts all worker threads.
  */
UINT App_Create_Startup_Thread(TX_BYTE_POOL *byte_pool)
{
  UINT status;
  uint8_t *thread_stack;
  
  if (!byte_pool)
    return TX_PTR_ERROR;
  
  /* Allocate startup thread stack */
  status = tx_byte_allocate(byte_pool,
                            (VOID **)&thread_stack,
                            2 * 1024,  /* 2 KB stack */
                            TX_NO_WAIT);
  if (status != TX_SUCCESS)
    return status;
  
  /* Create startup thread (auto-start) */
  status = tx_thread_create(&g_startup_thread,
                            "Startup Thread",
                            App_Startup_Thread_Entry,
                            0,
                            thread_stack,
                            2 * 1024,
                            9,  /* Priority 9 (between main and workers) */
                            9,
                            TX_NO_TIME_SLICE,
                            TX_AUTO_START);  /* Auto-start */
  
  return status;
}

  /**
  * @brief  MX_ThreadX_Init
  * @param  None
  * @retval None
  * 
  * This function is called from main() AFTER all hardware is initialized:
  * - HAL initialized
  * - System clocks configured
  * - UART ready (for printf)
  * - Wi-Fi GPIO initialized
  * - MX_DCACHE1_Init called
  * - MX_USART2_UART_Init called
  * 
  * The kernel will call App_ThreadX_Init from tx_kernel_enter(), which creates
  * all application threads. Then control passes to the idle thread.
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN  Before_Kernel_Start */
  printf("Entering ThreadX kernel...\n");

  /* USER CODE END  Before_Kernel_Start */

  tx_kernel_enter();

  /* Control never reaches here unless all threads exit */
  /* USER CODE BEGIN  Kernel_Start_Error */
  printf("ThreadX kernel error or all threads exited\n");
  Error_Handler();

  /* USER CODE END  Kernel_Start_Error */
}

/* USER CODE BEGIN 1 */

/**
  * @brief  Application startup thread
  * @param  input: unused
  * @retval None
  * 
  * This thread is responsible for:
  * 1. Waiting for NetX Duo to be initialized
  * 2. Waiting for IP address to be assigned (DHCP)
  * 3. Initializing telemetry module with valid IP instance
  * 4. Starting all worker threads
  */
static void App_Startup_Thread_Entry(ULONG input)
{
  (void)input;
  UINT status;
  TX_QUEUE *audio_queue;
  TX_QUEUE *feature_queue;
  ULONG wait_count = 0;
  
  printf("Startup thread running\n");
  
  /* Wait for NetX Duo IP address to be assigned (DHCP) - max 60 seconds */
  while (IpAddress == 0 && wait_count < 600)
  {
    tx_thread_sleep(100);  /* 100ms tick */
    wait_count++;
    if ((wait_count % 10) == 0)
      printf("Waiting for IP assignment... (%lu seconds)\n", wait_count / 10);
  }
  
  if (IpAddress == 0)
  {
    printf("ERROR: IP address not assigned after timeout\n");
    Error_Handler();
  }
  
  printf("IP address assigned: ");
  PRINT_IP_ADDRESS(IpAddress);
  
  /* Initialize telemetry module with IP instance */
  status = Telemetry_Init(g_byte_pool, &IpInstance);
  if (status != TX_SUCCESS)
  {
    printf("Telemetry_Init failed: 0x%02X\n", status);
    Error_Handler();
  }
  printf("Telemetry initialized\n");
  
  /* Get queue pointers for inter-thread communication */
  audio_queue = AudioAcquisition_GetQueue();
  feature_queue = FeatureExtraction_GetOutputQueue();
  
  if (!audio_queue || !feature_queue)
  {
    printf("ERROR: Could not get queue pointers\n");
    Error_Handler();
  }
  
  /* Start audio acquisition (captures from microphone) */
  status = AudioAcquisition_Start();
  if (status != TX_SUCCESS)
  {
    printf("AudioAcquisition_Start failed: 0x%02X\n", status);
    Error_Handler();
  }
  printf("Audio acquisition started\n");
  
  /* Start feature extraction (consumes audio_queue, produces feature_queue) */
  status = FeatureExtraction_Start(audio_queue);
  if (status != TX_SUCCESS)
  {
    printf("FeatureExtraction_Start failed: 0x%02X\n", status);
    Error_Handler();
  }
  printf("Feature extraction started\n");
  
  /* Start telemetry transmission (consumes feature_queue) */
  /* Use broadcast by default (0xFFFFFFFF) */
  status = Telemetry_Start(feature_queue, 0xFFFFFFFF);
  if (status != TX_SUCCESS)
  {
    printf("Telemetry_Start failed: 0x%02X\n", status);
    Error_Handler();
  }
  printf("Telemetry transmission started\n");
  
  printf("\n");
  printf("========================================\n");
  printf("  All subsystems initialized successfully\n");
  printf("========================================\n");
  printf("Audio capture -> Feature extraction -> UDP telemetry pipeline ACTIVE\n");
  printf("  Audio acq thread: Priority 8\n");
  printf("  Feature extr:    Priority 7\n");
  printf("  Telemetry TX:    Priority 8\n");
  printf("  Web server:      Priority 5 (HTTP on port 80)\n");
  printf("========================================\n\n");
  
  /* Suspend this startup thread - initialization complete */
  tx_thread_suspend(&g_startup_thread);
}

/* USER CODE END 1 */
