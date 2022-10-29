# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/pipewire)

FILE ( GLOB SMARTPIPEWIRE_SOURCES "${CURRENT_HEADER_DIR}/smartPipewire*.h" "${CURRENT_SOURCE_DIR}/PipewireHandler.h" "${CURRENT_SOURCE_DIR}/smartPipewire.cpp" "${CURRENT_SOURCE_DIR}/PipewireHandler.cpp" )

add_library(smartPipewire SHARED ${SMARTPIPEWIRE_SOURCES} )
set_target_properties(smartPipewire PROPERTIES VERSION 1)

# Pipewire
target_include_directories(smartPipewire PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${PIPEWIRE_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR} ${OPENGL_EGL_INCLUDE_DIRS} )
target_link_libraries(smartPipewire PUBLIC ${PIPEWIRE_LIBRARIES} Qt${Qt_VERSION}::Core Qt${Qt_VERSION}::DBus )

# Grabber
FILE ( GLOB PIPEWIRE_SOURCES "${CURRENT_HEADER_DIR}/smartPipewire*.h" "${CURRENT_HEADER_DIR}/Pipewire*.h" "${CURRENT_SOURCE_DIR}/PipewireGrabber.cpp" "${CURRENT_SOURCE_DIR}/PipewireWrapper.cpp" )

add_library(Pipewire-grabber ${PIPEWIRE_SOURCES} )

target_link_libraries(Pipewire-grabber hyperhdr-base ${QT_LIBRARIES} )
