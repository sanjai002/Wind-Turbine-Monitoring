/**
 ******************************************************************************
 * @file    INTEGRATION_GUIDE.md
 * @author  Wind Turbine Firmware Team
 * @brief   Step-by-step integration guide for connecting all subsystems
 * @date    2025-01-02
 ******************************************************************************
 * 
 * # INTEGRATION GUIDE - Single Node Audio Telemetry System
 * 
 * ## OVERVIEW
 * 
 * This document explains how to integrate the newly created audio telemetry
 * subsystem into the existing STWINBX1 NetXDuo web server project.
 * 
 * New files created:
 * - Core/Inc/audio_features.h
 * - Core/Src/audio_features.c
 * - Core/Inc/audio_acquisition.h
 * - Core/Src/audio_acquisition.c
 * - Core/Inc/feature_extraction.h
 * - Core/Src/feature_extraction.c
 * - NetXDuo/App/app_telemetry.h
 * - NetXDuo/App/app_telemetry.c
 * 
 * Modified files:
 * - Core/Inc/app_threadx.h (added function prototype)
 * - Core/Src/app_threadx.c (integrated thread creation and startup logic)
 * - NetXDuo/App/app_netxduo.h (exported NX_IP instance)
 * - Core/Src/main.c (no changes needed, ready as-is)
 * 
 * ## STEP 1: VERIFY CubeMX CONFIGURATION
 * 
 * Before building, verify that STM32CubeMX has configured:
 * 
 * ### Audio Capture (Microphone)
 * - [ ] DFSDM (Digital Filter for Sigma-Delta Modulation) enabled
 *   - Channel 1: PDM input for microphone
 *   - Filter 0: Output PCM @ 16 kHz, 16-bit, mono
 *   - DMA: Circular mode, enabled
 * 
 * - [ ] I2S or SPI clock for PDM microphone
 *   - Verify: PDM clock frequency suitable for microphone
 *   - Typical: 3.2 MHz or as per IMP34DT05/DT85 datasheet
 * 
 * - [ ] GPIO configured for microphone data/clock lines
 *   - PDM_IN: Digital input
 *   - PDM_CLK: Clock output or input (check datasheet)
 * 
 * ### Networking (Wi-Fi)
 * - [ ] SPI configured for Wi-Fi module (EMW3080B)
 *   - Baudrate: typically 40 MHz or as configured
 *   - DMA: enabled for transfers
 * 
 * - [ ] GPIO for Wi-Fi interrupt (NOTIFY pin)
 *   - Rising edge triggered
 *   - External interrupt enabled
 * 
 * - [ ] UART for debug output (UART2)
 *   - Already configured @ 115200 baud
 * 
 * ### RTOS & Memory
 * - [ ] ThreadX: Core enabled, heap configured
 * - [ ] NetX Duo: Enabled, memory pool size sufficient
 * - [ ] FileX: Optional, for SD card (already configured)
 * 
 * **ACTION**: If any above are unchecked:
 * 1. Open Nx_WebServer.ioc in STM32CubeMX
 * 2. Configure missing components
 * 3. Generate code (project will be updated, user code preserved)
 * 4. Rebuild project
 * 
 * ## STEP 2: REVIEW app_netxduo.c MODIFICATIONS
 * 
 * The app_netxduo.c file needs ONE addition at the END of MX_NetXDuo_Init():
 * 
 * ```c
 * UINT MX_NetXDuo_Init(VOID *memory_ptr)
 * {
 *   // ... existing code ...
 *   
 *   // Create startup thread to initialize telemetry after IP is ready
 *   ret = App_Create_Startup_Thread(byte_pool);
 *   if (ret != TX_SUCCESS)
 *   {
 *     printf("Startup thread creation failed: 0x%02X\n", ret);
 *     Error_Handler();
 *   }
 *   
 *   return ret;
 * }
 * ```
 * 
 * **LOCATION**: NetXDuo/App/app_netxduo.c, at the very end of MX_NetXDuo_Init()
 * **BEFORE THE FINAL `return ret;`**
 * 
 * The startup thread will:
 * 1. Wait for IP address assignment (DHCP)
 * 2. Initialize telemetry module
 * 3. Start all worker threads
 * 
 * ## STEP 3: INCLUDE HEADERS IN app_netxduo.c
 * 
 * At the top of NetXDuo/App/app_netxduo.c, add:
 * 
 * ```c
 * /* USER CODE BEGIN Includes */
 * #include "app_threadx.h"  // For App_Create_Startup_Thread()
 * // ... other includes ...
 * /* USER CODE END Includes */
 * ```
 * 
 * This allows app_netxduo.c to call the startup thread creation function.
 * 
 * ## STEP 4: ADD DMA CALLBACKS (OPTIONAL BUT RECOMMENDED)
 * 
 * For production operation, add these callbacks to Core/Src/stm32u5xx_it.c:
 * 
 * ```c
 * // Add to stm32u5xx_it.c (in HAL_DFSDM_FilterRegularConvCpltCallback section)
 * 
 * void HAL_DFSDM_FilterRegularConvCpltCallback(DFSDM_Filter_HandleTypeDef *hdfsdm_filter)
 * {
 *   if (hdfsdm_filter->Instance == DFSDM1_Filter0)
 *   {
 *     /* Half-transfer complete: DMA filled first half-buffer */
 *     AudioAcquisition_DMA_Complete_Callback();
 *   }
 * }
 * 
 * void HAL_DFSDM_FilterErrorCallback(DFSDM_Filter_HandleTypeDef *hdfsdm_filter)
 * {
 *   if (hdfsdm_filter->Instance == DFSDM1_Filter0)
 *   {
 *     AudioAcquisition_DMA_Error_Callback();
 *   }
 * }
 * ```
 * 
 * **LOCATION**: Core/Src/stm32u5xx_it.c (after line with /* USER CODE BEGIN PERIPH_CALLBACK_ADD */)
 * 
 * This allows the acquisition thread to be notified when new audio data is ready.
 * Without this, the system still works but uses polling (less efficient).
 * 
 * ## STEP 5: UPDATE DFSDM INITIALIZATION (if needed)
 * 
 * If STM32CubeMX created a separate MX_DFSDM_Init() function, ensure it's called
 * from main.c BEFORE MX_ThreadX_Init():
 * 
 * **In main.c:**
 * ```c
 * // User code section: /* USER CODE BEGIN 2 */
 * MX_DFSDM_Init();  // Add this if not already present
 * /* USER CODE END 2 */
 * ```
 * 
 * ## STEP 6: CONFIGURE NODE IDENTIFIER
 * 
 * For multiple nodes (or future expansion), set the node_id in feature_extraction.c:
 * 
 * **File**: Core/Src/feature_extraction.c, line ~20
 * ```c
 * static uint32_t node_id = 1;  // Node 1 (change to 2 or 3 for other nodes)
 * ```
 * 
 * This value is included in every telemetry packet for identification.
 * 
 * ## STEP 7: BUILD PROJECT
 * 
 * ### Clean Build
 * ```bash
 * Project → Clean
 * Project → Build Project
 * ```
 * 
 * ### Expected Output
 * - No errors
 * - Warnings are acceptable (unused parameters, etc.)
 * 
 * ### Common Build Errors & Fixes
 * 
 * **Error**: "undefined reference to `AudioAcquisition_Init'"
 * - **Cause**: audio_acquisition.c not included in build
 * - **Fix**: Add file to STM32CubeIDE project (right-click project → Add Files)
 * 
 * **Error**: "undefined reference to `Telemetry_Init'"
 * - **Cause**: app_telemetry.c not compiled
 * - **Fix**: Add NetXDuo/App/app_telemetry.c to project build
 * 
 * **Error**: "IpInstance undeclared" in app_threadx.c
 * - **Cause**: app_netxduo.h not properly exporting NX_IP
 * - **Fix**: Verify extern declaration in app_netxduo.h
 * 
 * **Error**: "conflicting types for 'AudioFrame_t'"
 * - **Cause**: Duplicate #include of audio_acquisition.h
 * - **Fix**: Remove duplicate includes from header files
 * 
 * ## STEP 8: PROGRAM AND TEST
 * 
 * ### Hardware Setup
 * 1. Connect STWINBX1 to USB (ST-Link for power and debug)
 * 2. Ensure Wi-Fi antenna is connected
 * 3. Place microphone near sound source (speaker for testing)
 * 
 * ### Program Device
 * ```
 * Run → Debug (F9) or Run (Ctrl+F5)
 * ```
 * 
 * ### Monitor Serial Output
 * 1. Open serial terminal (e.g., PuTTY, minicom, VS Code Serial Monitor)
 * 2. Port: /dev/ttyACM0 (Linux) or COM4 (Windows) - check Device Manager
 * 3. Baudrate: 115200, 8N1 (8 bits, No parity, 1 stop bit)
 * 4. Reset board: Press reset button
 * 
 * ### Expected Messages (in order)
 * ```
 * ThreadX App Initialization Started
 * Audio Acquisition initialized
 * Feature Extraction initialized
 * All ThreadX application threads created successfully
 * Entering ThreadX kernel...
 * Nx_WebServer_Application_Started..
 * Startup thread running
 * Waiting for IP assignment... (10 seconds)
 * Waiting for IP assignment... (20 seconds)
 * STM32 IpAddress: 192.168.1.X
 * IP address assigned: 192.168.1.X
 * Telemetry initialized
 * Audio acquisition started
 * Feature extraction started
 * Telemetry transmission started
 * ========================================
 *   All subsystems initialized successfully
 * ========================================
 * Audio capture -> Feature extraction -> UDP telemetry pipeline ACTIVE
 * ```
 * 
 * If messages don't appear, check:
 * - Serial port is open and at correct baudrate
 * - UART2 is enabled in STM32CubeMX
 * - printf() redirection is working (check HAL_UART_Transmit in main.c)
 * 
 * ## STEP 9: VERIFY NETWORK FUNCTIONALITY
 * 
 * ### Check UDP Packets
 * On a PC on the same network:
 * 
 * **Linux/Mac:**
 * ```bash
 * # Listen on UDP port 5000 (where telemetry packets arrive)
 * nc -u -l 5000
 *
 * # Or use tcpdump to capture packets
 * sudo tcpdump -i eth0 -n udp port 5000
 * ```
 * 
 * **Windows (PowerShell):**
 * ```powershell
 * # Use netstat to monitor UDP activity
 * netstat -an | findstr 5000
 * ```
 * 
 * ### Packet Inspection
 * - Packets should arrive every ~2 seconds (or as configured)
 * - Each packet is 64 bytes
 * - Use Wireshark to inspect packet contents
 * 
 * ### Expected Payload (first 16 bytes)
 * ```
 * Byte 0:    0x01           (Version)
 * Byte 1:    0x00           (Reserved)
 * Byte 2-3:  0xXXXX         (Sequence number, little-endian)
 * Byte 4-7:  0xXXXXXXXX     (Timestamp in ms)
 * ...rest: feature data...
 * ```
 * 
 * ## STEP 10: CONFIGURE FOR PRODUCTION
 * 
 * ### Set Receiver IP Address (if not using broadcast)
 * In app_telemetry.c:
 * ```c
 * // Change in Telemetry_SetReceiver() call from app_threadx.c
 * // Example: set specific receiver IP
 * ULONG receiver_ip = 0xC0A80164;  // 192.168.1.100 in network byte order
 * Telemetry_Start(feature_queue, receiver_ip);
 * ```
 * 
 * ### Enable/Disable Broadcast
 * ```c
 * Telemetry_SetBroadcast(0);  // 0 = unicast, 1 = broadcast
 * ```
 * 
 * ### Adjust Feature Extraction Interval
 * In audio_features.h:
 * ```c
 * #define AUDIO_FRAMES_PER_PACKET    4  // 4 frames = 2 seconds @ 16 kHz
 * // Change to 2 for 1 second, 8 for 4 seconds, etc.
 * ```
 * 
 * ### Monitor Thread Performance
 * Add periodic status output:
 * ```c
 * // In app_threadx.c or main thread:
 * printf("Audio frames: %lu\n", AudioAcquisition_GetFrameCount());
 * printf("Feature packets: %lu\n", FeatureExtraction_GetPacketCount());
 * printf("Telemetry TX: %lu\n", Telemetry_GetTxCount());
 * ```
 * 
 * ## TROUBLESHOOTING INTEGRATION ISSUES
 * 
 * ### Issue: "Multiple definitions of audio_acquisition" errors
 * **Cause**: File included multiple times or added twice to project
 * **Fix**: 
 * - In project: Project → Properties → C/C++ Build → Source
 * - Remove duplicate files
 * - Rebuild with Project → Clean first
 * 
 * ### Issue: Linker errors for nx_api.h symbols
 * **Cause**: NetX Duo library not linked
 * **Fix**: 
 * - Verify library is in Middlewares/NetXDuo folder
 * - Check project linker script includes library path
 * - Rebuild STM32CubeMX project
 * 
 * ### Issue: App starts but no audio features extracted
 * **Cause**: DFSDM not producing data, microphone not connected
 * **Fix**:
 * - Check DFSDM configuration in STM32CubeMX
 * - Verify microphone GPIO and clock connections
 * - Use oscilloscope to verify PDM clock (should be ~3.2 MHz)
 * - Check AudioAcquisition_GetErrorCount() for DMA failures
 * 
 * ### Issue: Network packets not being sent
 * **Cause**: IP address not assigned, Wi-Fi not connected
 * **Fix**:
 * - Check router DHCP server is running
 * - Verify Wi-Fi SSID/password in mx_wifi_conf.h
 * - Check Wi-Fi antenna connection
 * - Monitor IpAddress value via debugger breakpoint
 * 
 * ## DEPLOYMENT CHECKLIST
 * 
 * Before deploying to production:
 * 
 * - [ ] Project builds without errors
 * - [ ] Device boots and shows initialization messages
 * - [ ] IP address assigned by DHCP (check console output)
 * - [ ] UDP packets received on PC (check with netstat/tcpdump)
 * - [ ] Audio packets contain non-zero feature values
 * - [ ] Telemetry thread running (check Telemetry_GetTxCount increases)
 * - [ ] No error messages in console output
 * - [ ] Green LED blinks during operation (normal status)
 * - [ ] Orange LED toggles only on errors
 * - [ ] Node ID matches expected identifier
 * - [ ] Receiver IP address configured correctly
 * - [ ] Power consumption within specifications (~300-350 mA)
 * 
 * ## PERFORMANCE VALIDATION
 * 
 * ### Measure Latency
 * 1. Send known stimulus to microphone (e.g., beep)
 * 2. Timestamp when beep occurs
 * 3. Timestamp when packet received at central station
 * 4. Latency = packet time - stimulus time (typical: 2-3 seconds)
 * 
 * ### Monitor CPU Usage
 * Add ThreadX performance monitor:
 * ```c
 * tx_thread_performance_info_get(&g_startup_thread, ...);
 * // Check context switches, suspensions, etc.
 * ```
 * 
 * ### Check Memory Usage
 * - Verify free heap: `tx_byte_pool_info_get()`
 * - Watch for memory exhaustion (error codes with 0x13 = OUT_OF_MEMORY)
 * 
 * ## NEXT STEPS (FUTURE ENHANCEMENTS)
 * 
 * - [ ] Add second and third node (change node_id in each)
 * - [ ] Implement central receiver on PC to collect all packets
 * - [ ] Add JSON logging to SD card
 * - [ ] Implement MQTT transport for cloud connectivity
 * - [ ] Add anomaly detection algorithm
 * - [ ] Implement time synchronization across nodes
 * 
 * ---
 * **Document Version**: 1.0
 * **Last Updated**: 2025-01-02
 * **Status**: Ready for Implementation
 * 
 */
