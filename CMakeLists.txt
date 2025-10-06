cmake_minimum_required(VERSION 3.15)
project(DolHook C CXX ASM)

# Options
option(DOLHOOK_NO_BANNER "Disable banner" OFF)
option(DOLHOOK_NO_PATTERN "Disable pattern scanning" OFF)

# devkitPPC setup for runtime
set(DEVKITPPC $ENV{DEVKITPPC})
if(NOT DEVKITPPC)
    set(DEVKITPPC "/opt/devkitpro/devkitPPC")
endif()

set(PPC_PREFIX "${DEVKITPPC}/bin/powerpc-eabi-")

# Runtime (cross-compiled for PPC)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR powerpc)

# Runtime sources
set(RUNTIME_SOURCES
    runtime/src/dolhook.c
    runtime/src/vi_banner.c
    runtime/src/pattern.c
    runtime/src/entry.S
)

# Patcher sources (host)
set(PATCHER_SOURCES
    tools/patchiso/main.cpp
    tools/patchiso/dol.cpp
    tools/patchiso/gcm.cpp
)

# Custom command for runtime
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/payload/payload.elf
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/payload
    COMMAND ${PPC_PREFIX}gcc
        -mcpu=750 -meabi -mhard-float
        -fno-exceptions -fno-asynchronous-unwind-tables
        -Os -Wall -Wextra
        -I${CMAKE_SOURCE_DIR}/runtime/include
        ${DOLHOOK_NO_BANNER_FLAG}
        ${DOLHOOK_NO_PATTERN_FLAG}
        -T ${CMAKE_SOURCE_DIR}/runtime/link.ld
        -nostartfiles -nostdlib -nodefaultlibs
        ${CMAKE_SOURCE_DIR}/runtime/src/entry.S
        ${CMAKE_SOURCE_DIR}/runtime/src/dolhook.c
        ${CMAKE_SOURCE_DIR}/runtime/src/vi_banner.c
        ${CMAKE_SOURCE_DIR}/runtime/src/pattern.c
        -o ${CMAKE_BINARY_DIR}/payload/payload.elf
    DEPENDS ${RUNTIME_SOURCES}
    COMMENT "Building runtime payload (PPC)"
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/payload/payload.bin
    COMMAND ${PPC_PREFIX}objcopy -O binary
        ${CMAKE_BINARY_DIR}/payload/payload.elf
        ${CMAKE_BINARY_DIR}/payload/payload.bin
    DEPENDS ${CMAKE_BINARY_DIR}/payload/payload.elf
    COMMENT "Extracting binary payload"
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/payload/payload.sym
    COMMAND ${PPC_PREFIX}nm ${CMAKE_BINARY_DIR}/payload/payload.elf |
        grep -E '__dolhook_(entry|original_entry)' |
        awk '{print $$3 \" 0x\" $$1}' > ${CMAKE_BINARY_DIR}/payload/payload.sym
        || echo "__dolhook_entry 0x80400000" > ${CMAKE_BINARY_DIR}/payload/payload.sym
    DEPENDS ${CMAKE_BINARY_DIR}/payload/payload.elf
    COMMENT "Generating symbol map"
)

add_custom_target(runtime ALL
    DEPENDS 
        ${CMAKE_BINARY_DIR}/payload/payload.bin
        ${CMAKE_BINARY_DIR}/payload/payload.sym
)

# Patcher (host executable)
add_executable(patchiso ${PATCHER_SOURCES})
target_compile_features(patchiso PRIVATE cxx_std_17)
target_compile_options(patchiso PRIVATE -Wall -Wextra -Werror)
target_include_directories(patchiso PRIVATE tools/patchiso)

# Copy payload to binary directory for patchiso
add_custom_command(TARGET patchiso POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_BINARY_DIR}/payload
        ${CMAKE_BINARY_DIR}/payload
)

# Install
install(TARGETS patchiso DESTINATION bin)
install(DIRECTORY ${CMAKE_BINARY_DIR}/payload/ DESTINATION share/dolhook)

# Testing
enable_testing()
add_test(NAME build_check COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR})