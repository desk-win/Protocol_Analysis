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
set(CMAKE_PROJECT_NAME qiansai_CM7)
project(${CMAKE_PROJECT_NAME})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Include mx-generated
include("mx-generated.cmake")

# Add touchgfx generated sources and libraries
add_subdirectory(cmake/touchgfx)


# In order to use CMake for cross-compiling
set(CMAKE_CXX_COMPILER_FORCED "true")

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
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
    # TouchGFX SDK framework includes
    E:/TouchGFX/4.26.1/touchgfx/framework/include
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
    Core/Src/crc.c
    # TouchGFX HAL callbacks (C++ bridge from LTDC ISR to OSWrappers)
    Core/Src/touchgfx_callbacks.cpp
    # TouchGFX target HAL (C++) — these are user-editable, NOT in cmake/touchgfx
    TouchGFX/target/generated/STM32DMA.cpp
    TouchGFX/target/generated/TouchGFXConfiguration.cpp
    TouchGFX/target/generated/TouchGFXGeneratedHAL.cpp
    TouchGFX/target/generated/OSWrappers.cpp
    TouchGFX/target/TouchGFXHAL.cpp
    TouchGFX/target/STM32TouchController.cpp
    TouchGFX/target/TouchGFXGPIO.cpp
    # TouchGFX app entry (C)
    TouchGFX/App/app_touchgfx.c
)

# TouchGFX library linking is handled by cmake/touchgfx/CMakeLists.txt

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()









