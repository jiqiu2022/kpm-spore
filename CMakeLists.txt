cmake_minimum_required(VERSION 3.15)
project(KPM-Build-Anywhere C)

# Option to configure project for IDE IntelliSense only (skips NDK/toolchain checks)
option(KPM_IDE_ONLY "Configure for IntelliSense only; skip NDK/toolchain checks and custom build steps" OFF)

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
# NDK Configuration (skipped in IDE-only mode)
# ==============================================================================
if(NOT KPM_IDE_ONLY)
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
else()
    message(STATUS "KPM_IDE_ONLY=ON: Skipping NDK/toolchain checks; configuring IntelliSense targets only.")
endif()

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

# Full include directory list for IDEs
set(FULL_INCLUDE_DIRS "")
foreach(dir ${INCLUDE_DIRS})
    list(APPEND FULL_INCLUDE_DIRS "${KP_DIR}/kernel/${dir}")
endforeach()

# 添加内核编译定义
set(KERNEL_DEFINITIONS
        -D__KERNEL__
        -D__LINUX__
        -D__USE_MISC
        -D_GNU_SOURCE
        -D__LINUX_ARM_ARCH__=8
        -D__aarch64__
)

# Use a known C standard for parsing by IDEs
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
set(CFLAGS
        ${INCLUDE_FLAGS}
        ${KERNEL_DEFINITIONS}
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
        -nostdinc
        -ffreestanding
)

# ==============================================================================
# Module Discovery and Build
# ==============================================================================

# Find all module directories
file(GLOB MODULE_DIRS LIST_DIRECTORIES true "${CMAKE_SOURCE_DIR}/modules/*")

set(ALL_MODULES "")
set(MODULE_SOURCES "")

foreach(MODULE_DIR ${MODULE_DIRS})
    if(IS_DIRECTORY "${MODULE_DIR}")
        get_filename_component(MODULE_NAME "${MODULE_DIR}" NAME)

        # Skip template directory
        if(MODULE_NAME STREQUAL "template")
            continue()
        endif()

        # Recursively find all .c files in module directory
        file(GLOB_RECURSE MODULE_SOURCES
                "${MODULE_DIR}/*.c"
        )

        # Filter out any unwanted files if needed
        list(FILTER MODULE_SOURCES EXCLUDE REGEX ".*/test/.*")
        list(FILTER MODULE_SOURCES EXCLUDE REGEX ".*/debug/.*")

        if(NOT MODULE_SOURCES)
            message(WARNING "Module ${MODULE_NAME}: no .c files found, skipping")
            continue()
        endif()

        # Collect sources for IDE IntelliSense
        list(APPEND ALL_MODULE_SOURCES ${MODULE_SOURCES})

        if(KPM_IDE_ONLY)
            message(STATUS "IDE-only: discovered module sources for ${MODULE_NAME}: ${MODULE_SOURCES}")
            continue()
        endif()

        # Check for custom linker script
        set(MODULE_LDS "${MODULE_DIR}/module.lds")
        if(EXISTS "${MODULE_LDS}")
            set(MODULE_CFLAGS ${CFLAGS} -T${MODULE_LDS})
        else()
            set(MODULE_CFLAGS ${CFLAGS})
        endif()

        # Create object file list
        set(MODULE_OBJS "")
        foreach(source_file ${MODULE_SOURCES})
            # Get relative path for output naming
            file(RELATIVE_PATH rel_path "${MODULE_DIR}" "${source_file}")
            # Replace path separators and extension
            string(REPLACE "/" "_" obj_name "${rel_path}")
            string(REPLACE ".c" ".o" obj_name "${obj_name}")
            set(obj_path "${CMAKE_BINARY_DIR}/${MODULE_NAME}/${obj_name}")
            list(APPEND MODULE_OBJS "${obj_path}")

            # Compile each source file
            add_custom_command(
                    OUTPUT ${obj_path}
                    COMMAND ${CC} ${MODULE_CFLAGS} -c -o ${obj_path} ${source_file}
                    DEPENDS ${source_file}
                    COMMENT "Compiling ${rel_path} for module: ${MODULE_NAME}"
            )
        endforeach()

        # Output path for final module
        set(MODULE_KPM "${CMAKE_BINARY_DIR}/${MODULE_NAME}/${MODULE_NAME}.kpm")

        # Create module output directory
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/${MODULE_NAME}")

        # Link all object files into final module
        add_custom_command(
                OUTPUT ${MODULE_KPM}
                COMMAND ${CC} -nostdlib -r -o ${MODULE_KPM} ${MODULE_OBJS}
                COMMAND ${STRIP_TOOL} -g --strip-unneeded --strip-debug
                --remove-section=.comment
                --remove-section=.note.GNU-stack
                ${MODULE_KPM}
                DEPENDS ${MODULE_OBJS}
                COMMENT "Linking KPM: ${MODULE_NAME}.kpm"
                VERBATIM
        )

        # Create target for this module
        add_custom_target(${MODULE_NAME} DEPENDS ${MODULE_KPM})

        list(APPEND ALL_MODULES ${MODULE_NAME})

        message(STATUS "Found module: ${MODULE_NAME} with sources: ${MODULE_SOURCES}")
    endif()
endforeach()

# Configure a dummy static library to expose compile options for IDEs
if(MODULE_SOURCES)
    add_library(kpm_intellisense STATIC EXCLUDE_FROM_ALL ${MODULE_SOURCES})
    target_include_directories(kpm_intellisense PRIVATE ${FULL_INCLUDE_DIRS})
    target_compile_options(kpm_intellisense PRIVATE -Wall)
    message(STATUS "Configured IntelliSense target: kpm_intellisense")
endif()

# Create 'all-modules' target to build all modules (skip in IDE-only mode)
if(NOT KPM_IDE_ONLY)
    if(ALL_MODULES)
        add_custom_target(all-modules ALL)
        add_dependencies(all-modules ${ALL_MODULES})
        list(LENGTH ALL_MODULES MODULE_COUNT)
        message(STATUS "Total modules found: ${MODULE_COUNT}")
        message(STATUS "Build targets: ${ALL_MODULES}")
    else()
        message(WARNING "No modules found in modules/ directory")
    endif()
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
