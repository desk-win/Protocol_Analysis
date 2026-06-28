cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Compiler options
set(STM32_MCU_FLAGS  "-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard " )

#Linker options
set(STM32_LINKER_SCRIPT stm32h747xx_flash_CM7.ld)
set(STM32_LINKER_OPTION  )

# Set the project name
set(CMAKE_PROJECT_NAME stm32h747_bbb_CM7)
project(${CMAKE_PROJECT_NAME})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Include mx-generated
include("mx-generated.cmake")

# Add touchgfx generated sources and libraries
add_subdirectory(cmake/touchgfx)

# TouchGFX Designer always generates USE_BPP=24 but our LTDC is RGB565 (16bpp).
# Override it here — this runs AFTER the Designer's cmake file, so our value wins.
get_target_property(_tgx_defs stm32cubemx INTERFACE_COMPILE_DEFINITIONS)
list(REMOVE_ITEM _tgx_defs "USE_BPP=24")
list(APPEND _tgx_defs "USE_BPP=16")
set_target_properties(stm32cubemx PROPERTIES INTERFACE_COMPILE_DEFINITIONS "${_tgx_defs}")

# Inject user include paths into TouchGFX target (survives Designer regeneration)
target_include_directories(TouchGFX PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Hardware
)


# In order to use CMake for cross-compiling
set(CMAKE_CXX_COMPILER_FORCED "true")

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    VIRTIO_MASTER_ONLY
)

# Enable CMake support for ASM, C, and C++ languages
enable_language(C CXX ASM)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    Core/Inc
    Core/Hardware
    Drivers/STM32H7xx_HAL_Driver/Inc
    Drivers/STM32H7xx_HAL_Driver/Inc/Legacy
    # TouchGFX project includes
    TouchGFX/App
    TouchGFX/target
    TouchGFX/target/generated
    TouchGFX/generated/gui_generated/include
    TouchGFX/generated/images/include
    TouchGFX/generated/texts/include
    TouchGFX/generated/fonts/include
    TouchGFX/generated/videos/include
    TouchGFX/gui/include
    # TouchGFX SDK framework includes — provided by cmake/touchgfx (Middlewares/ST/touchgfx/framework/include)
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/Hardware/sys_util.c
    Core/Hardware/delay.c
    Core/Hardware/mpu_util.c
    Core/Hardware/sdram_util.c
    Core/Hardware/ltdc_draw.c
    Core/Hardware/tft_spi.c
    Core/Hardware/lcd.c
    # Touch drivers (hardware I2C2 + GT9xxx/FT5206 auto-detect)
    Core/Hardware/gt9xxx.c
    Core/Hardware/ft5206.c
    Core/Hardware/touch.c
    Core/Src/crc.c
    Core/Src/config_persist.c    # 配置持久化（write_config_bin），独立于 CubeMX
    # TouchGFX HAL callbacks (C++ bridge from LTDC ISR to OSWrappers)
    Core/Src/touchgfx_callbacks.cpp
    # TouchGFX target HAL (C++) — these are user-editable, NOT in cmake/touchgfx
    TouchGFX/target/generated/STM32DMA.cpp
    TouchGFX/target/generated/TouchGFXConfiguration.cpp
    TouchGFX/target/generated/TouchGFXGeneratedHAL.cpp
    TouchGFX/target/generated/OSWrappers.cpp
    TouchGFX/target/TouchGFXHAL.cpp
    TouchGFX/target/TouchGFXPaintRGB888.cpp
    TouchGFX/target/STM32TouchController.cpp
    TouchGFX/target/TouchGFXGPIO.cpp
    # TouchGFX app entry (C)
    TouchGFX/App/app_touchgfx.c
)

# TouchGFX library linking is handled by cmake/touchgfx/CMakeLists.txt

# CRC HAL driver sources — CubeMX may not add these to mx-generated.cmake
# when CRC is added after initial generation. Keep them here permanently.
target_sources(STM32_Drivers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_crc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_crc_ex.c
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# OpenAMP/libmetal 用 newlib 的 syscall (_sbrk/_write/...)。--gc-sections 会把
# 这些 weak/未被直接引用的符号回收 → undefined reference。强制保留。
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -Wl,--undefined=_sbrk
    -Wl,--undefined=_write
    -Wl,--undefined=_read
    -Wl,--undefined=_close
    -Wl,--undefined=_lseek
    -Wl,--undefined=_exit
    -Wl,--undefined=_fstat
    -Wl,--undefined=_isatty
)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(FATAL_ERROR "Generated code requires C11 or higher")
endif()
