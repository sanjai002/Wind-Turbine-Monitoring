################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
/media/sanjai/New\ Volume1/projects/wind\ turbine/stm32/STWINBX1_WIFI-main/Projects/STWIN.box/Applications/NetXDuo/Nx_WebServer/FileX/Target/fx_stm32_sd_driver_glue.c 

OBJS += \
./Application/User/FileX/Target/fx_stm32_sd_driver_glue.o 

C_DEPS += \
./Application/User/FileX/Target/fx_stm32_sd_driver_glue.d 


# Each subdirectory must supply rules for building sources it contributes
Application/User/FileX/Target/fx_stm32_sd_driver_glue.o: /media/sanjai/New\ Volume1/projects/wind\ turbine/stm32/STWINBX1_WIFI-main/Projects/STWIN.box/Applications/NetXDuo/Nx_WebServer/FileX/Target/fx_stm32_sd_driver_glue.c Application/User/FileX/Target/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DFX_INCLUDE_USER_DEFINE_FILE -DLX_INCLUDE_USER_DEFINE_FILE -DNX_INCLUDE_USER_DEFINE_FILE -DTX_INCLUDE_USER_DEFINE_FILE -DTX_SINGLE_MODE_NON_SECURE=1 -DUSE_HAL_DRIVER -DSTM32U585xx -c -I../../FileX/App -I../../FileX/Target -I../../LevelX/App -I../../LevelX/Target -I../../NetXDuo/App -I../../Core/Inc -I../../AZURE_RTOS/App -I../../../../../../../Drivers/STM32U5xx_HAL_Driver/Inc -I../../../../../../../Drivers/STM32U5xx_HAL_Driver/Src -I../../../../../../../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../../../../../../../Middlewares/ST/levelx/common/inc -I../../../../../../../Middlewares/ST/netxduo/addons/dhcp -I../../../../../../../Middlewares/ST/netxduo/addons/web -I../../../../../../../Middlewares/ST/threadx/common/inc -I../../../../../../../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../../../../../../../Middlewares/ST/filex/common/inc -I../../../../../../../Middlewares/ST/filex/ports/generic/inc -I../../../../../../../Middlewares/ST/levelx/common/inc -I../../../../../../../Middlewares/ST/netxduo/common/inc -I../../../../../../../Middlewares/ST/netxduo/ports/cortex_m33/gnu/inc -I../../../../../../../Middlewares/ST/threadx/ports/cortex_m33/gnu/inc -I../../../../../../../Drivers/CMSIS/Include -I../../../../../../../Drivers/BSP/STWIN.box -I../../../../../../../Drivers/BSP/Components/mx_wifi -I../../../../../../../Middlewares/ST/netxduo/common/drivers/wifi/mxchip -I../../NetXDuo/Target -Ofast -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Application/User/FileX/Target/fx_stm32_sd_driver_glue.d" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Application-2f-User-2f-FileX-2f-Target

clean-Application-2f-User-2f-FileX-2f-Target:
	-$(RM) ./Application/User/FileX/Target/fx_stm32_sd_driver_glue.cyclo ./Application/User/FileX/Target/fx_stm32_sd_driver_glue.d ./Application/User/FileX/Target/fx_stm32_sd_driver_glue.o ./Application/User/FileX/Target/fx_stm32_sd_driver_glue.su

.PHONY: clean-Application-2f-User-2f-FileX-2f-Target

