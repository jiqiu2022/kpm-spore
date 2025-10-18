cmake_minimum_required(VERSION 3.15)
project(KPM-Build-Anywhere NONE)

# Enable compile commands for IDEs (CLion, VS Code, etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# ==============================================================================
# Platform Detection
# ==============================================================================
if(WIN32)
    set(PLATFORM "windows-x86_64")
elseif(APPLE)
    # macOS - older NDKs only have darwin-x86_64, newer have darwin-aarch64
    execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(ARCH STREQUAL "arm64")
        set(PREFERRED_PLATFORM "darwin-aarch64")
        set(FALLBACK_PLATFORM "darwin-x86_64")
    else()
        set(PREFERRED_PLATFORM "darwin-x86_64")
        set(FALLBACK_PLATFORM "darwin-x86_64")
    endif()
    set(PLATFORM ${PREFERRED_PLATFORM})
else()
    set(PLATFORM "linux-x86_64")
endif()

message(STATUS "Detected host architecture: ${ARCH}")

# ==============================================================================
# NDK Detection Function
# ==============================================================================
function(find_ndk_path OUTPUT_VAR)
    set(NDK_FOUND FALSE)
    set(POSSIBLE_PATHS "")
    
    if(WIN32)
        set(POSSIBLE_PATHS
            "$ENV{LOCALAPPDATA}/Android/Sdk/ndk"
            "$ENV{USERPROFILE}/AppData/Local/Android/Sdk/ndk"
            "C:/Android/Sdk/ndk"
            "C:/android-ndk"
        )
    elseif(APPLE)
        set(POSSIBLE_PATHS
            "$ENV{HOME}/Library/Android/sdk/ndk"
            "/usr/local/share/android-ndk"
            "/opt/android-ndk"
        )
    else()
        set(POSSIBLE_PATHS
            "$ENV{HOME}/Android/Sdk/ndk"
            "/opt/android-ndk"
            "/usr/local/android-ndk"
        )
    endif()
    
    foreach(base_path ${POSSIBLE_PATHS})
        if(EXISTS "${base_path}")
            file(GLOB ndk_versions LIST_DIRECTORIES true "${base_path}/*")
            foreach(version_path ${ndk_versions})
                if(IS_DIRECTORY "${version_path}")
                    if(EXISTS "${version_path}/toolchains/llvm/prebuilt/${PLATFORM}")
                        set(${OUTPUT_VAR} "${version_path}" PARENT_SCOPE)
                        set(NDK_FOUND TRUE)
                        message(STATUS "Found NDK: ${version_path}")
                        return()
                    endif()
                endif()
            endforeach()
        endif()
    endforeach()
    
    if(NOT NDK_FOUND)
        set(${OUTPUT_VAR} "" PARENT_SCOPE)
    endif()
endfunction()

# ==============================================================================
# NDK Configuration
# ==============================================================================
if(DEFINED ENV{NDK_PATH})
    set(NDK_PATH $ENV{NDK_PATH})
    message(STATUS "Using NDK_PATH from environment: ${NDK_PATH}")
else()
    message(STATUS "NDK_PATH not set, searching for NDK automatically...")
    find_ndk_path(NDK_PATH)
    if(NOT NDK_PATH)
        message(FATAL_ERROR 
            "Could not find NDK automatically. Please set NDK_PATH environment variable:\n"
            "  export NDK_PATH=/path/to/ndk\n"
            "Or download NDK from: https://developer.android.com/ndk/downloads"
        )
    endif()
endif()

# For macOS, detect which platform directory actually exists in NDK
if(APPLE AND DEFINED FALLBACK_PLATFORM)
    set(PREBUILT_PATH "${NDK_PATH}/toolchains/llvm/prebuilt/${PLATFORM}")
    if(NOT EXISTS "${PREBUILT_PATH}")
        message(STATUS "Platform ${PLATFORM} not found in NDK, trying ${FALLBACK_PLATFORM}")
        set(PLATFORM ${FALLBACK_PLATFORM})
        set(PREBUILT_PATH "${NDK_PATH}/toolchains/llvm/prebuilt/${PLATFORM}")
        if(NOT EXISTS "${PREBUILT_PATH}")
            message(FATAL_ERROR "NDK toolchain not found at: ${PREBUILT_PATH}")
        endif()
    endif()
endif()

set(TOOLCHAIN_PREFIX "${NDK_PATH}/toolchains/llvm/prebuilt/${PLATFORM}/bin/")

# Set tools
set(CC "${TOOLCHAIN_PREFIX}aarch64-linux-android31-clang")
set(LD "${TOOLCHAIN_PREFIX}ld.lld")
set(STRIP_TOOL "${TOOLCHAIN_PREFIX}llvm-strip")

# Verify tools exist
if(NOT EXISTS "${CC}")
    message(FATAL_ERROR "NDK compiler not found: ${CC}\nPlease check your NDK installation.")
endif()

message(STATUS "Using platform: ${PLATFORM}")
message(STATUS "Using NDK toolchain: ${TOOLCHAIN_PREFIX}")

# ==============================================================================
# KernelPatch Source Configuration
# ==============================================================================
set(KP_VERSION "0.11.3" CACHE STRING "KernelPatch version to use")
set(KP_ZIP_URL "https://github.com/bmax121/KernelPatch/archive/refs/heads/main.zip" CACHE STRING "KernelPatch zip URL")

set(KP_LOCAL_DIR "${CMAKE_SOURCE_DIR}/third_party/KernelPatch")

if(DEFINED ENV{KP_DIR})
    set(KP_DIR $ENV{KP_DIR})
    message(STATUS "Using KernelPatch from environment: ${KP_DIR}")
elseif(EXISTS "${KP_LOCAL_DIR}")
    set(KP_DIR "${KP_LOCAL_DIR}")
    message(STATUS "Using KernelPatch from third_party: ${KP_DIR}")
else()
    message(STATUS "Downloading KernelPatch from GitHub (zip archive)...")
    message(STATUS "This may take a few minutes on first run...")
    
    FetchContent_Declare(
        kernelpatch
        URL ${KP_ZIP_URL}
        SOURCE_DIR ${KP_LOCAL_DIR}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    
    FetchContent_MakeAvailable(kernelpatch)
    set(KP_DIR "${KP_LOCAL_DIR}")
    message(STATUS "KernelPatch downloaded to: ${KP_DIR}")
endif()

# Verify KernelPatch directory
if(NOT EXISTS "${KP_DIR}/kernel")
    message(FATAL_ERROR 
        "KernelPatch directory is invalid: ${KP_DIR}\n"
        "Missing 'kernel' subdirectory."
    )
endif()

message(STATUS "KernelPatch directory: ${KP_DIR}")

# ==============================================================================
# Build Configuration
# ==============================================================================

# Include directories from KernelPatch
set(INCLUDE_DIRS
    .
    include
    patch/include
    linux/include
    linux/arch/arm64/include
    linux/tools/arch/arm64/include
)

set(INCLUDE_FLAGS "")
foreach(dir ${INCLUDE_DIRS})
    list(APPEND INCLUDE_FLAGS "-I${KP_DIR}/kernel/${dir}")
endforeach()

# Compiler flags
set(CFLAGS
    ${INCLUDE_FLAGS}
    -Wall
    -Ofast
    -fno-PIC
    -fno-asynchronous-unwind-tables
    -fno-stack-protector
    -fno-unwind-tables
    -fno-semantic-interposition
    -U_FORTIFY_SOURCE
    -fno-common
    -fvisibility=hidden
    -O2
)

# ==============================================================================
# Module Discovery and Build
# ==============================================================================

# Find all module directories
file(GLOB MODULE_DIRS LIST_DIRECTORIES true "${CMAKE_SOURCE_DIR}/modules/*")

set(ALL_MODULES "")

foreach(MODULE_DIR ${MODULE_DIRS})
    if(IS_DIRECTORY "${MODULE_DIR}")
        get_filename_component(MODULE_NAME "${MODULE_DIR}" NAME)
        
        # Skip template directory
        if(MODULE_NAME STREQUAL "template")
            continue()
        endif()
        
        # Check if module.c exists
        set(MODULE_SOURCE "${MODULE_DIR}/module.c")
        if(NOT EXISTS "${MODULE_SOURCE}")
            message(WARNING "Module ${MODULE_NAME}: module.c not found, skipping")
            continue()
        endif()
        
        # Check for custom linker script
        set(MODULE_LDS "${MODULE_DIR}/module.lds")
        if(EXISTS "${MODULE_LDS}")
            set(MODULE_CFLAGS ${CFLAGS} -T${MODULE_LDS})
        else()
            set(MODULE_CFLAGS ${CFLAGS})
        endif()
        
        # Output paths
        set(MODULE_OBJ "${CMAKE_BINARY_DIR}/${MODULE_NAME}/${MODULE_NAME}.o")
        set(MODULE_KPM "${CMAKE_BINARY_DIR}/${MODULE_NAME}/${MODULE_NAME}.kpm")
        
        # Create module output directory
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/${MODULE_NAME}")
        
        # Compile command
        add_custom_command(
            OUTPUT ${MODULE_OBJ}
            COMMAND ${CC} ${MODULE_CFLAGS} -c -o ${MODULE_OBJ} ${MODULE_SOURCE}
            DEPENDS ${MODULE_SOURCE} ${MODULE_LDS}
            COMMENT "Compiling module: ${MODULE_NAME}"
        )
        
        # Link command
        add_custom_command(
            OUTPUT ${MODULE_KPM}
            COMMAND ${CC} -nostdlib -r -o ${MODULE_KPM} ${MODULE_OBJ}
            COMMAND ${STRIP_TOOL} -g --strip-unneeded --strip-debug 
                    --remove-section=.comment 
                    --remove-section=.note.GNU-stack 
                    ${MODULE_KPM}
            DEPENDS ${MODULE_OBJ}
            COMMENT "Creating KPM: ${MODULE_NAME}.kpm"
            VERBATIM
        )
        
        # Create target for this module
        add_custom_target(${MODULE_NAME} DEPENDS ${MODULE_KPM})
        
        list(APPEND ALL_MODULES ${MODULE_NAME})
        
        message(STATUS "Found module: ${MODULE_NAME}")
    endif()
endforeach()

# Create 'all-modules' target to build all modules
if(ALL_MODULES)
    add_custom_target(all-modules ALL)
    add_dependencies(all-modules ${ALL_MODULES})
    list(LENGTH ALL_MODULES MODULE_COUNT)
    message(STATUS "Total modules found: ${MODULE_COUNT}")
    message(STATUS "Build targets: ${ALL_MODULES}")
else()
    message(WARNING "No modules found in modules/ directory")
endif()

# ==============================================================================
# Clean Configuration
# ==============================================================================
set_directory_properties(PROPERTIES 
    ADDITIONAL_CLEAN_FILES "${CMAKE_BINARY_DIR}/*/*.o;${CMAKE_BINARY_DIR}/*/*.kpm"
)

message(STATUS "")
message(STATUS "========================================")
message(STATUS "Configuration completed successfully")
message(STATUS "========================================")
message(STATUS "Build commands:")
message(STATUS "  All modules:     cmake --build build")
message(STATUS "  Specific module: cmake --build build --target <module_name>")
message(STATUS "  Available targets: ${ALL_MODULES}")
message(STATUS "========================================")
