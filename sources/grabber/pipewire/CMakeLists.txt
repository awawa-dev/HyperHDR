# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber/pipewire)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/pipewire)

FILE ( GLOB SMARTPIPEWIRE_SOURCES "${CURRENT_HEADER_DIR}/smartPipewire.h" "${CURRENT_HEADER_DIR}/PipewireHandler.h" "${CURRENT_HEADER_DIR}/ScreenCastProxy.h" "${CURRENT_SOURCE_DIR}/smartPipewire.cpp" "${CURRENT_SOURCE_DIR}/PipewireHandler.cpp" )

add_library(smartPipewire SHARED ${SMARTPIPEWIRE_SOURCES} )

# Pipewire
target_include_directories(smartPipewire PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${PIPEWIRE_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR} ${OPENGL_EGL_INCLUDE_DIRS} )
target_link_libraries(smartPipewire PUBLIC ${PIPEWIRE_LIBRARIES} Qt${Qt_VERSION}::Core sdbus-c++)
set_property(TARGET smartPipewire PROPERTY CXX_STANDARD 17)

# Grabber
FILE ( GLOB PIPEWIRE_SOURCES "${CURRENT_HEADER_DIR}/smartPipewire.h" "${CURRENT_HEADER_DIR}/PipewireGrabber.h" "${CURRENT_HEADER_DIR}/PipewireWrapper.h"
									"${CURRENT_SOURCE_DIR}/PipewireGrabber.cpp" "${CURRENT_SOURCE_DIR}/PipewireWrapper.cpp" )

add_library(Pipewire-grabber ${PIPEWIRE_SOURCES} )

target_link_libraries(Pipewire-grabber hyperhdr-base ${QT_LIBRARIES} )

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(Pipewire-grabber REUSE_FROM precompiled_hyperhdr_headers)
endif()
