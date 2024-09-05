# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber/linux/X11)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/linux/X11)

FILE ( GLOB SMARTX11_SOURCES "${CURRENT_HEADER_DIR}/smartX11*.h" "${CURRENT_SOURCE_DIR}/smartX11.cpp" )

add_library(smart-x11 SHARED ${SMARTX11_SOURCES} )
target_include_directories(smart-x11 PUBLIC ${XLibs_INCLUDE_DIRS})
target_link_libraries(smart-x11 ${XLibs_LIBRARIES} )

FILE ( GLOB X11_SOURCES "${CURRENT_HEADER_DIR}/smartX11*.h" "${CURRENT_HEADER_DIR}/X11*.h" "${CURRENT_SOURCE_DIR}/X11*.cpp" )

add_library(X11-grabber OBJECT ${X11_SOURCES} )

target_link_libraries(X11-grabber
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(X11-grabber REUSE_FROM precompiled_hyperhdr_headers)
endif()



