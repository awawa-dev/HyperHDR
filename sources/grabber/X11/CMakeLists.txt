# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber/X11)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/X11)

FILE ( GLOB SMARTX11_SOURCES "${CURRENT_HEADER_DIR}/smartX11*.h" "${CURRENT_SOURCE_DIR}/smartX11.cpp" )

add_library(smartX11 SHARED ${SMARTX11_SOURCES} )
target_include_directories(smartX11 PUBLIC ${XLibs_INCLUDE_DIRS})
target_link_libraries(smartX11 ${XLibs_LIBRARIES} )
set_target_properties(smartX11 PROPERTIES VERSION 1)

FILE ( GLOB X11_SOURCES "${CURRENT_HEADER_DIR}/smartX11*.h" "${CURRENT_HEADER_DIR}/X11*.h" "${CURRENT_SOURCE_DIR}/X11*.cpp" )

add_library(X11-grabber ${X11_SOURCES} )

target_link_libraries(X11-grabber
	hyperhdr-base
	${QT_LIBRARIES}
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(X11-grabber REUSE_FROM precompiled_hyperhdr_headers)
endif()



