/**
 ******************************************************************************
 * @file    ARCHITECTURE.md
 * @author  Wind Turbine Firmware Team
 * @brief   Complete system architecture documentation
 * @date    2025-01-02
 ******************************************************************************
 * 
 * # STWINBX1 Audio Telemetry System - Complete Architecture
 * 
 * ## OVERVIEW
 * 
 * Single-node audio feature extraction and UDP telemetry system for STWINBX1.
 * Three identical nodes can be deployed by changing node_id in app_threadx.c.
 * 
 * ## HARDWARE STACK
 * 
 * - MCU: STM32U585AIIX (STM32U5 series)
 * - Microphone: IMP34DT05 or IMP34DT85 (PDM digital MEMS)
 * - Wi-Fi: STWINBX1 built-in Wi-Fi module (EMW3080B)
 * - Storage: Optional SD card via FileX
 * - Toolchain: STM32CubeIDE 1.18.1, GCC arm-none-eabi 13.3
 * 
 * ## SOFTWARE STACK
 * 
 * - RTOS: Azure RTOS ThreadX
 * - Network: Azure RTOS NetX Duo
 * - Storage: Azure RTOS FileX (optional)
 * - Drivers: STM32 HAL, STM32 BSP, STWINBX1 BSP
 * - Audio: STM32 Audio Development Framework (ADF)
 * 
 * ## THREAD ARCHITECTURE
 * 
 * ### Thread Hierarchy & Priorities
 * 
 * ```
 * Priority | Thread Name              | Function
 * ---------+--------------------------+-------------------------------------------
 *    5     | HTTP Web Server          | Serves web dashboard (app_netxduo.c)
 *    7     | Feature Extraction       | DSP processing (feature_extraction.c)
 *    8     | Audio Acquisition        | PDM → PCM capture (audio_acquisition.c)
 *    8     | Telemetry TX             | UDP sender (app_telemetry.c)
 *   10     | App Main (NetX Duo)      | IP/DHCP initialization (app_netxduo.c)
 *   15     | LED Thread               | Status blinker (app_netxduo.c)
 * ```
 * 
 * Higher priority = more urgent (ThreadX convention: 0=highest, 31=lowest)
 * 
 * ### Thread Data Flow
 * 
 * ```
 *  ┌─────────────────────────────────────────────────────────────────────┐
 *  │                       HARDWARE: PDM Microphone                      │
 *  │                              (I2S input)                            │
 *  └────────────────────────────────────┬────────────────────────────────┘
 *                                       │
 *                                       ↓
 *  ┌──────────────────────────────────────────────────────────────────────┐
 *  │  DFSDM (Digital Filter for Sigma-Delta Modulation)                  │
 *  │  - Input: PDM data @ high frequency (~3.2 MHz)                       │
 *  │  - Output: PCM @ 16 kHz, 16-bit, mono                               │
 *  │  - DMA: Circular buffer with half-transfer interrupts                │
 *  └────────────────────────────┬─────────────────────────────────────────┘
 *                               │
 *                               ↓
 *  ┌──────────────────────────────────────────────────────────────────────┐
 *  │  AudioAcquisition Thread (Priority 8)                                │
 *  │  ├─ Waits for DMA completion (512 samples = 32ms)                    │
 *  │  ├─ Converts DFSDM output to PCM16                                   │
 *  │  └─ Queues: AudioFrame_t (512 samples)                              │
 *  │                                                                       │
 *  │  Output Queue: AUDIO_ACQ_QUEUE_DEPTH = 4 frames                     │
 *  └────────────────────────────┬─────────────────────────────────────────┘
 *                               │
 *                               ↓
 *  ┌──────────────────────────────────────────────────────────────────────┐
 *  │  FeatureExtraction Thread (Priority 7)                               │
 *  │  ├─ Receives: AudioFrame_t (512 samples, 32ms)                       │
 *  │  ├─ Accumulates: 4 frames = 2048 samples = 2 seconds                │
 *  │  ├─ Computes:                                                        │
 *  │  │   • RMS energy (Q15 fixed-point)                                  │
 *  │  │   • Zero Crossing Rate (%)                                        │
 *  │  │   • Peak Amplitude (ADC counts)                                   │
 *  │  │   • Sound Pressure Level (dB)                                     │
 *  │  │   • FFT magnitude bands (8 bands, 0-8 kHz)                        │
 *  │  └─ Queues: AudioTelemetryPacket_t (64 bytes, every 2 seconds)      │
 *  │                                                                       │
 *  │  Output Queue: FEATURE_EXTRACT_QUEUE_DEPTH = 2 packets              │
 *  └────────────────────────────┬─────────────────────────────────────────┘
 *                               │
 *                               ↓
 *  ┌──────────────────────────────────────────────────────────────────────┐
 *  │  Telemetry Thread (Priority 8)                                       │
 *  │  ├─ Receives: AudioTelemetryPacket_t (64 bytes)                      │
 *  │  ├─ Creates UDP packet with NX_PACKET                                │
 *  │  ├─ Sends: UDP destination (default: broadcast 255.255.255.255)     │
 *  │  │          UDP port: 5000 (configurable)                            │
 *  │  └─ Handles: Retries, error reporting, packet buffering              │
 *  │                                                                       │
 *  │  Output: UDP socket via NetX Duo IP stack                            │
 *  └──────────────────────────┬───────────────────────────────────────────┘
 *                             │
 *                             ↓
 *  ┌──────────────────────────────────────────────────────────────────────┐
 *  │        NETWORK: UDP packets to central receiver or broadcast         │
 *  │                                                                       │
 *  │  Packet content (64 bytes):                                          │
 *  │  ├─ Header: version, sequence number, timestamp                      │
 *  │  ├─ Features: RMS, ZCR rate, SPL, peak amplitude                    │
 *  │  ├─ FFT: 8 frequency bands (1 kHz each)                             │
 *  │  └─ Metadata: node_id, error_count, uptime                          │
 *  └──────────────────────────────────────────────────────────────────────┘
 * ```
 * 
 * ## INITIALIZATION SEQUENCE
 * 
 * ### Phase 1: Hardware Setup (main.c, before ThreadX)
 * 
 * ```c
 * HAL_Init()
 * ├─ Reset peripherals
 * ├─ Initialize system tick
 * └─ Enable Vdd IO2
 * 
 * SystemClock_Config()  // Configure PLL for 160 MHz
 * 
 * MX_ICACHE_Init()      // Enable I-cache (1-way)
 * MX_DCACHE1_Init()     // Enable D-cache
 * BSP_Enable_DCDC2()    // Power for Wi-Fi
 * BSP_LED_Init()        // Status LEDs
 * BSP_BC_Sw_Init()      // Battery charger control
 * MX_USART2_UART_Init() // UART @ 115200
 * BSP_WIFI_MX_GPIO_Init() // Wi-Fi GPIO (SPI/interrupt)
 * ```
 * 
 * ### Phase 2: ThreadX Kernel Startup (app_threadx.c)
 * 
 * ```c
 * MX_ThreadX_Init()
 * ├─ tx_kernel_enter()   // Start ThreadX scheduler
 * │  └─ Calls App_ThreadX_Init(byte_pool)
 * │     ├─ AudioAcquisition_Init(byte_pool)
 * │     │  └─ Create thread + queue
 * │     ├─ FeatureExtraction_Init(byte_pool)
 * │     │  └─ Create thread + queue
 * │     └─ [Telemetry_Init deferred until IP ready]
 * └─ Kernel becomes active, threads run
 * ```
 * 
 * ### Phase 3: NetX Duo Initialization (app_netxduo.c, App_Main_Thread_Entry)
 * 
 * ```c
 * MX_NetXDuo_Init(byte_pool)
 * ├─ nx_system_initialize()
 * ├─ nx_packet_pool_create(&AppPool, ...)
 * ├─ nx_ip_create(&IpInstance, ..., nx_driver_emw3080_entry, ...)
 * │  └─ Wi-Fi driver initialization
 * ├─ nx_arp_enable()
 * ├─ nx_icmp_enable()
 * ├─ nx_udp_enable()
 * ├─ nx_tcp_enable()
 * ├─ Create HTTP web server thread
 * ├─ Create app main thread
 * └─ Create DHCP client
 * 
 * App_Main_Thread_Entry()
 * ├─ Set IP address change callback
 * ├─ nx_dhcp_start()   // Request IP from router
 * ├─ Wait for semaphore (IP assigned)
 * └─ Resume web server thread
 * ```
 * 
 * ### Phase 4: Worker Thread Startup (deferred until IP ready)
 * 
 * When IP address is assigned:
 * 
 * ```c
 * [In main.c or app_threadx.c, create startup thread]
 * ├─ Wait for NX_IP instance to be initialized
 * ├─ Telemetry_Init(byte_pool, &IpInstance)
 * │  └─ Create telemetry thread + UDP socket
 * ├─ AudioAcquisition_Start()
 * │  └─ Resume audio thread, start DFSDM DMA
 * ├─ FeatureExtraction_Start(audio_queue)
 * │  └─ Resume feature extraction thread
 * └─ Telemetry_Start(feature_queue, broadcast_ip)
 *    └─ Resume telemetry thread, enable UDP
 * ```
 * 
 * ## FILE STRUCTURE & MODULE RESPONSIBILITIES
 * 
 * ### Core Audio Subsystem
 * 
 * **audio_features.h / audio_features.c**
 * - RMS calculation (Q15 format)
 * - Zero Crossing Rate computation
 * - SPL (Sound Pressure Level) estimation
 * - FFT-based frequency band analysis
 * - Peak amplitude detection
 * - Static library: no threading, pure DSP
 * 
 * **audio_acquisition.h / audio_acquisition.c**
 * - AudioFrame_t queue message (512 samples)
 * - DFSDM DMA interface (STM32 Audio Framework)
 * - Captures microphone PDM → PCM conversion
 * - Runs at priority 8 (medium-high)
 * - Dependency: main.h (for DFSDM handles)
 * 
 * **feature_extraction.h / feature_extraction.c**
 * - FeatureBuffer_t accumulator (2048 samples, 2 seconds)
 * - AudioTelemetryPacket_t generation
 * - Aggregates 4 audio frames per packet cycle
 * - Runs at priority 7 (higher priority than acq for responsiveness)
 * - Calls audio_features.c functions for DSP
 * 
 * ### Network Subsystem
 * 
 * **app_telemetry.h / app_telemetry.c**
 * - UDP socket creation and binding
 * - Packet transmission via nx_udp_socket_send()
 * - Broadcast or unicast mode selection
 * - Receiver IP and port configuration
 * - Runs at priority 8 (same as audio acq for symmetry)
 * - Dependency: NetX Duo (NX_IP, NX_UDP_SOCKET)
 * 
 * **app_netxduo.h / app_netxduo.c** (CubeMX generated, extended)
 * - IP instance creation
 * - DHCP client for dynamic IP assignment
 * - HTTP web server (reads from SD card)
 * - App main thread for IP initialization
 * - Contains global NX_IP instance
 * - Dependency: NetX Duo, FileX (for SD card)
 * 
 * **app_threadx.h / app_threadx.c** (CubeMX generated, extended)
 * - Thread creation (audio acq, feature extraction, telemetry)
 * - Byte pool allocation for thread stacks and queues
 * - Startup synchronization (waits for IP before starting workers)
 * - Dependency: ThreadX, all subsystem modules
 * 
 * **main.c / main.h** (CubeMX generated, extended)
 * - Hardware initialization (clocks, UART, GPIO)
 * - Cache initialization (I-cache, D-cache)
 * - ThreadX kernel entry point
 * - PUTCHAR redirection to UART for printf()
 * - Dependency: STM32 HAL, BSP drivers
 * 
 * ### Data Structures
 * 
 * **AudioFrame_t** (audio_acquisition.h)
 * ```c
 * typedef struct {
 *     int16_t  samples[512];        // PCM samples
 *     uint32_t timestamp_ms;        // Capture time
 *     uint32_t frame_number;        // Sequence ID
 *     uint8_t  error_flags;         // Status
 * } AudioFrame_t;
 * Size: 1024 + 12 = 1036 bytes (fits in single TX_QUEUE message)
 * ```
 * 
 * **AudioTelemetryPacket_t** (audio_features.h)
 * ```c
 * typedef struct __attribute__((packed)) {
 *     // Header (4 bytes)
 *     uint8_t  version;             // 0x01
 *     uint8_t  reserved1;           // Future
 *     uint16_t seq_number;          // 0-65535
 *     
 *     // Timestamps (4 bytes)
 *     uint32_t timestamp_ms;        // Boot relative
 *     
 *     // RMS (4 bytes)
 *     uint16_t rms_raw;             // Q15 (0-32767)
 *     uint16_t reserved2;           // Padding
 *     
 *     // ZCR (4 bytes)
 *     uint16_t zcr_count;           // Count
 *     uint16_t zcr_rate;            // % (0-100)
 *     
 *     // SPL (4 bytes)
 *     uint16_t spl_db;              // dB (0-120)
 *     uint16_t peak_amplitude;      // ADC (0-32767)
 *     
 *     // FFT bands (32 bytes)
 *     uint32_t fft_band[8];         // Magnitude (0-1000000)
 *     
 *     // Metadata (8 bytes)
 *     uint8_t  node_id;             // 1-3
 *     uint8_t  status_flags;        // Bits: error, clipping
 *     uint16_t error_count;         // Cumulative
 *     uint32_t uptime_sec;          // Since boot
 * } AudioTelemetryPacket_t;
 * Size: EXACTLY 64 bytes (packed, no padding)
 * ```
 * 
 * ## CRITICAL DESIGN DECISIONS
 * 
 * ### 1. No Polling Delays in Threads
 * - ❌ FORBIDDEN: `HAL_Delay()` blocks RTOS scheduler
 * - ✅ CORRECT: `tx_thread_sleep(ms)` yields CPU to other threads
 * 
 * ### 2. Thread-Safe Queues for Communication
 * - Audio frame flow: AudioAcquisition → FeatureExtraction
 * - Feature packet flow: FeatureExtraction → Telemetry
 * - Queues use TX_QUEUE for thread-safe message passing
 * - No shared variables between threads (no mutexes needed)
 * 
 * ### 3. Fixed-Size Packet for Network Efficiency
 * - 64-byte packet: no variable-length serialization overhead
 * - Endian-safe: explicit byte ordering (little-endian assumed)
 * - Versioned: field for protocol compatibility
 * - CRC optional (depends on receiver implementation)
 * 
 * ### 4. Binary Features (No Raw Audio)
 * - Raw PCM: 16,000 samples/sec * 2 bytes = 32 KB/sec
 * - Compressed at 64 bytes/2 sec = 32 bytes/sec = ~1000x reduction
 * - Features sufficient for wind turbine diagnostics
 * 
 * ### 5. Double Buffering in DFSDM DMA
 * - Hardware DMA continuous mode (circular)
 * - Software reads one buffer while DMA fills the other
 * - No data loss even if software lags briefly
 * - Requires HAL_DFSDM_FilterRegularStart_DMA(...) with circular setup
 * 
 * ### 6. Deferred Telemetry Initialization
 * - Telemetry_Init() called AFTER NX_IP is created
 * - Requires valid NX_IP instance for UDP socket creation
 * - Startup thread synchronizes this dependency
 * 
 * ## INTERRUPT & DMA HANDLERS (to implement in stm32u5xx_it.c)
 * 
 * ```c
 * // DFSDM DMA complete interrupt
 * void DMA1_Channel0_IRQHandler(void) {
 *     HAL_DMA_IRQHandler(&handle_GPDMA1_Channel0);  // For DFSDM1 DMA
 * }
 * 
 * // DFSDM filter complete callback (add to HAL callbacks)
 * void HAL_DFSDM_FilterRegularConvCpltCallback(DFSDM_Filter_HandleTypeDef *hdfsdm_filter) {
 *     if (hdfsdm_filter->Instance == DFSDM1_Filter0) {
 *         AudioAcquisition_DMA_Complete_Callback();  // Notify thread
 *     }
 * }
 * 
 * // DFSDM error callback
 * void HAL_DFSDM_FilterErrorCallback(DFSDM_Filter_HandleTypeDef *hdfsdm_filter) {
 *     AudioAcquisition_DMA_Error_Callback();  // Handle error
 * }
 * 
 * // Wi-Fi interrupt for ESP or EMW3080
 * void EXTI_IRQHandler(uint16_t GPIO_Pin) {
 *     if (GPIO_Pin == MXCHIP_NOTIFY_Pin) {
 *         mxchip_WIFI_ISR(MXCHIP_NOTIFY_Pin);
 *         nx_driver_emw3080_interrupt();  // Notify NetX driver
 *     }
 * }
 * ```
 * 
 * ## CONFIGURATION & CUSTOMIZATION
 * 
 * ### Node Identification
 * In **feature_extraction.c**, line ~20:
 * ```c
 * static uint32_t node_id = 1;  // Change to 2 or 3 for other nodes
 * ```
 * 
 * ### Receiver IP Address
 * In **app_telemetry.c**, or call at runtime:
 * ```c
 * ULONG receiver_ip = htonl((192 << 24) | (168 << 16) | (1 << 8) | 100);  // 192.168.1.100
 * Telemetry_SetReceiver(receiver_ip, 5000);
 * ```
 * 
 * ### Broadcast vs Unicast
 * ```c
 * Telemetry_SetBroadcast(1);  // 1 = broadcast, 0 = unicast
 * ```
 * 
 * ### UDP Port
 * Change in **app_telemetry.h**:
 * ```c
 * #define TELEMETRY_UDP_PORT_RX         5000  // Receiver port
 * ```
 * 
 * ### Feature Packet Interval
 * Currently: 2 seconds (4 frames * 512 samples @ 16 kHz = 128 ms per frame)
 * Change in **audio_features.h**:
 * ```c
 * #define AUDIO_FRAMES_PER_PACKET    4  // Increase/decrease for different intervals
 * ```
 * 
 * ### Sample Rate
 * Currently: 16 kHz (requires DFSDM configured for 16 kHz output)
 * Change in **audio_features.h** and rerun STM32CubeMX:
 * ```c
 * #define AUDIO_SAMPLE_RATE          16000  // Or 32000, 48000, etc.
 * ```
 * 
 * ## BUILD & DEPLOYMENT
 * 
 * ### Build Steps
 * 1. Open project in STM32CubeIDE 1.18.1
 * 2. Project → Build Project (Ctrl+B)
 * 3. Verify: No errors, only warnings (unused parameters, etc.)
 * 
 * ### Programming
 * 1. Connect STWINBX1 via USB-JTAG (ST-Link)
 * 2. Run → Run (F11) or Debug (F9)
 * 3. Open serial console @ 115200 baud
 * 4. Watch initialization messages
 * 
 * ### Verification
 * - UART Output: "Audio Acquisition initialized", etc.
 * - Green LED: Blinks when system running normally
 * - Orange LED: Toggles on errors
 * - Ethernet/UDP: Packets arrive at receiver on port 5000
 * 
 * ## PERFORMANCE CHARACTERISTICS
 * 
 * ### CPU Load (estimate)
 * - Audio acquisition: ~2% (DMA-driven, minimal CPU)
 * - Feature extraction: ~15% (FFT and DSP calculations)
 * - Telemetry: ~3% (UDP send every 2 seconds)
 * - Web server: ~2% (idle, processes HTTP requests on demand)
 * - **Total: ~22%** at 160 MHz, leaving headroom for monitoring/logging
 * 
 * ### Memory Usage (estimate)
 * - ThreadX + NetX: ~64 KB
 * - Thread stacks: 2 + 4 + 3 = 9 KB
 * - Queues: ~10 KB
 * - HTTP server buffers: ~20 KB
 * - **Total: ~103 KB** of 384 KB available RAM
 * 
 * ### Latency
 * - Audio capture → feature packet: ~2 seconds (by design)
 * - Feature packet → UDP send: <100 ms (depends on network)
 * - Total end-to-end: ~2.1 seconds
 * 
 * ### Power Consumption
 * - CPU active: ~200 mA @ 160 MHz (check datasheet)
 * - Wi-Fi module: ~100-150 mA (transmit/receive)
 * - Microphone + DFSDM: ~5 mA
 * - **Total: ~300-350 mA** (nominal operation)
 * 
 * ## TROUBLESHOOTING
 * 
 * ### "Audio Acquisition initialization failed"
 * - Check: `audio_acquisition_init()` status code
 * - Verify: Byte pool is not exhausted
 * - Solution: Increase ThreadX heap in app_azure_rtos.h
 * 
 * ### "Feature Extraction queue send failed"
 * - Symptom: Audio frames being dropped
 * - Cause: Feature extraction thread blocked or slow
 * - Solution: Check FFT computation time, reduce FFT_SIZE
 * 
 * ### "UDP send failed: 0x4B"
 * - Error code: 0x4B = NX_NOT_BOUND (socket not bound)
 * - Cause: Socket creation failed or IP not ready
 * - Solution: Check `Telemetry_IsReady()` before sending
 * 
 * ### "DHCP timeout"
 * - No IP address assigned after 30 seconds
 * - Cause: Router DHCP server not responding, Wi-Fi not connected
 * - Solution: Check Wi-Fi SSID/password in mx_wifi_conf.h
 * 
 * ### Microphone not capturing audio
 * - Check: DFSDM configured in STM32CubeMX (PDM channel, filter)
 * - Check: Microphone GPIO connected correctly
 * - Check: `AudioAcquisition_GetErrorCount()` > 0 indicates issues
 * - Solution: Verify DFSDM DMA is circular and enabled
 * 
 * ## COMPLIANCE & STANDARDS
 * 
 * - **C Standard**: C99 (GCC 13.3 default)
 * - **Compiler Flags**: -Wall -Wextra -Wpedantic (enabled in IDE)
 * - **Coding Style**: STMicroelectronics firmware template conventions
 * - **CubeMX Regeneration**: All custom code in USER CODE sections (preserved)
 * 
 * ## FUTURE ENHANCEMENTS
 * 
 * 1. **Adaptive Packet Rate**: Send packets more frequently during anomalies
 * 2. **Packet Compression**: Use LZ4 or similar for reduced bandwidth
 * 3. **Edge ML**: On-device anomaly detection using TinyML
 * 4. **MQTT Client**: Replace UDP with MQTT for pub/sub capability
 * 5. **SD Card Logging**: Buffer packets to SD if network unavailable
 * 6. **Multi-rate Audio**: Support 8 kHz (low power) and 48 kHz (hi-fi)
 * 7. **Cloud Integration**: Direct HTTPS upload to Azure/AWS
 * 8. **Power Management**: Sleep modes when wind speeds low
 * 
 * ---
 * **Document Version**: 1.0
 * **Last Updated**: 2025-01-02
 * **Status**: Complete, Ready for Deployment
 * 
 */
