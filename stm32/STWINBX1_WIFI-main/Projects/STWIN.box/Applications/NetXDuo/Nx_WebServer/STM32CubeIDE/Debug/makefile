################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include Middlewares/ThreadX/Performance/subdir.mk
-include Middlewares/ThreadX/Core/subdir.mk
-include Middlewares/NetXDuo/NX\ Core/subdir.mk
-include Middlewares/NetXDuo/Addons\ Web\ Server/subdir.mk
-include Middlewares/NetXDuo/Addons\ DHCP\ Client/subdir.mk
-include Middlewares/Interfaces/Network/wifi/subdir.mk
-include Middlewares/FileX/SD\ interface/subdir.mk
-include Middlewares/FileX/Core/subdir.mk
-include Drivers/STM32U5xx_HAL_Driver/subdir.mk
-include Drivers/CMSIS/subdir.mk
-include Drivers/BSP/STWIN.box/subdir.mk
-include Drivers/BSP/Components/mx_wifi/io_pattern/subdir.mk
-include Drivers/BSP/Components/mx_wifi/core/subdir.mk
-include Drivers/BSP/Components/mx_wifi/subdir.mk
-include Application/User/Startup/subdir.mk
-include Application/User/NetXDuo/App/subdir.mk
-include Application/User/FileX/Target/subdir.mk
-include Application/User/FileX/App/subdir.mk
-include Application/User/Core/subdir.mk
-include Application/User/AZURE_RTOS/App/subdir.mk
-include objects.mk

# Re-generate the linker input list from the current $(OBJS).
# CubeIDE links via @"objects.list".
#
# IMPORTANT: $(OBJS) contains escaped spaces (e.g. "NX\ Core") because
# Cube-generated folder names include spaces. A naive "tr ' ' '\n'" will
# split those paths and break the link step.
objects.list: $(OBJS)
	@rm -f $@
	@# The linker "@file" syntax splits on whitespace; paths that contain spaces must be quoted per line.
	@# $(OBJS) contains make-escaped spaces (e.g. NX\ Core). We convert those to real spaces, then wrap each path in quotes.
	@set -f; \
	for f in $(OBJS); do \
		p=`printf '%s' "$$f" | sed -e 's/\\ / /g'`; \
		printf '"%s"\n' "$$p" >> $@; \
	done

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := Nx_WebServer
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
EXECUTABLES += \
Nx_WebServer.elf \

MAP_FILES += \
Nx_WebServer.map \

SIZE_OUTPUT += \
default.size.stdout \

OBJDUMP_LIST += \
Nx_WebServer.list \


# All Target
all: main-build

# Main-build Target
main-build: Nx_WebServer.elf secondary-outputs

# Tool invocations
Nx_WebServer.elf Nx_WebServer.map: $(OBJS) $(USER_OBJS) /media/sanjai/New\ Volume1/projects/wind\ turbine/stm32/STWINBX1_WIFI-main/Projects/STWIN.box/Applications/NetXDuo/Nx_WebServer/STM32CubeIDE/STM32U585AIIX_FLASH.ld makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-gcc -o "Nx_WebServer.elf" @"objects.list" $(USER_OBJS) $(LIBS) -mcpu=cortex-m33 -T"/media/sanjai/New Volume1/projects/wind turbine/stm32/STWINBX1_WIFI-main/Projects/STWIN.box/Applications/NetXDuo/Nx_WebServer/STM32CubeIDE/STM32U585AIIX_FLASH.ld" --specs=nosys.specs -Wl,-Map="Nx_WebServer.map" -Wl,--gc-sections -static --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -Wl,--start-group -lc -lm -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

default.size.stdout: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-size  $(EXECUTABLES)
	@echo 'Finished building: $@'
	@echo ' '

Nx_WebServer.list: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objdump -h -S $(EXECUTABLES) > "Nx_WebServer.list"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) Nx_WebServer.elf Nx_WebServer.list Nx_WebServer.map default.size.stdout
	-@echo ' '

secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST)

fail-specified-linker-script-missing:
	@echo 'Error: Cannot find the specified linker script. Check the linker settings in the build configuration.'
	@exit 2

warn-no-linker-script-specified:
	@echo 'Warning: No linker script specified. Check the linker settings in the build configuration.'

.PHONY: all clean dependents main-build fail-specified-linker-script-missing warn-no-linker-script-specified

-include ../makefile.targets
