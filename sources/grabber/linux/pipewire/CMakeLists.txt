# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber/linux/pipewire)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/linux/pipewire)

FILE ( GLOB SMARTPIPEWIRE_SOURCES "${CURRENT_HEADER_DIR}/smartPipewire.h" "${CURRENT_HEADER_DIR}/PipewireHandler.h" "${CURRENT_HEADER_DIR}/ScreenCastProxy.h" "${CURRENT_SOURCE_DIR}/smartPipewire.cpp" "${CURRENT_SOURCE_DIR}/PipewireHandler.cpp" )

add_library(smart-pipewire SHARED ${SMARTPIPEWIRE_SOURCES} )

# Pipewire
target_include_directories(smart-pipewire PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${PIPEWIRE_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR} ${OPENGL_EGL_INCLUDE_DIRS} )
target_link_libraries(smart-pipewire PUBLIC
	${PIPEWIRE_LIBRARIES}
	Qt${Qt_VERSION}::Core
	image
	sdbus-c++
)
set_property(TARGET smart-pipewire PROPERTY CXX_STANDARD 17)

# Grabber
FILE ( GLOB PIPEWIRE_SOURCES "${CURRENT_HEADER_DIR}/smartPipewire.h" "${CURRENT_HEADER_DIR}/PipewireGrabber.h" "${CURRENT_HEADER_DIR}/PipewireWrapper.h"
									"${CURRENT_SOURCE_DIR}/PipewireGrabber.cpp" "${CURRENT_SOURCE_DIR}/PipewireWrapper.cpp" )

add_library(Pipewire-grabber OBJECT ${PIPEWIRE_SOURCES} )

target_link_libraries(Pipewire-grabber
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(Pipewire-grabber REUSE_FROM precompiled_hyperhdr_headers)
endif()
