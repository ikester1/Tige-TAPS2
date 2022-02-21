cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

#
# These options are required to build the SDK
#
add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        -Wno-maybe-uninitialized
        )


set( MYTARGET taps )
set( MYINC include )

project( ${MYTARGET} )

set( SOURCES
        taps.cpp
        hal.cpp
        CActuator.cpp
        CNVFlash.cpp
        CNVFRAM.cpp
        CMessage.cpp
        CGauge.cpp
        CI2C.cpp
        util.cpp
        CPowerFail.cpp
)

set( HEADERS
        ${MYINC}/taps.hpp
        ${MYINC}/CActuator.hpp
        ${MYINC}/CGauge.hpp
        ${MYINC}/CNVFlash.hpp
        ${MYINC}/CNVFRAM.hpp
        ${MYINC}/CNVState.hpp
        ${MYINC}/config.h
        ${MYINC}/CPowerFail.hpp
        ${MYINC}/CRC.hpp
        ${MYINC}/hal.hpp
        ${MYINC}/util.hpp
)

if( 0 )
    #  The pico SDK has so many warnings in the code;  the following breaks.  So I'm sure
    #    my code is icky as well (but i can't easily check)
    #
    set_source_files_properties( ${SOURCES}
        PROPERTIES COMPILE_OPTIONS "-Werror;-Wextra"
    )
endif()

set_source_files_properties( ${SOURCES}
    PROPERTIES COMPILE_OPTIONS "-O3;-fno-exceptions"
)

add_executable( ${MYTARGET} ${SOURCES} ${HEADERS} )
target_include_directories( ${MYTARGET} PRIVATE ${MYINC} )

#
# This just strips the TARGET to reduce its size.  Can be commented out if desired
#
add_custom_command( TARGET ${MYTARGET}
                    POST_BUILD
                    COMMAND eu-strip -o "$<TARGET_FILE:${MYTARGET}>.stripped" "$<TARGET_FILE:${MYTARGET}>" && mv -f "$<TARGET_FILE:${MYTARGET}>.stripped" "$<TARGET_FILE:${MYTARGET}>" )

# Pull in our pico_stdlib which aggregates commonly used features
target_link_libraries(${MYTARGET} pico_stdlib hardware_pwm hardware_clocks hardware_flash hardware_i2c )

# enable usb output, disable uart output
pico_enable_stdio_usb(${MYTARGET} 1)
pico_enable_stdio_uart(${MYTARGET} 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(${MYTARGET})
