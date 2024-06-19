# Define the current source locations
set(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/webserver)
set(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/webserver)

FILE ( GLOB WebConfig_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )
FILE ( GLOB_RECURSE webFiles RELATIVE ${CMAKE_BINARY_DIR}  ${CMAKE_SOURCE_DIR}/www/* )

FOREACH( f ${webFiles} )
    STRING ( REPLACE "www/" ";" workingWebFile ${f})
    list(GET workingWebFile -1 fname)
    SET(HYPERHDR_WEBCONFIG_RES "${HYPERHDR_WEBCONFIG_RES}\n\t\t<file alias=\"/www/${fname}\">${f}</file>")
ENDFOREACH()
CONFIGURE_FILE(${CURRENT_SOURCE_DIR}/WebConfig.qrc.in ${CMAKE_BINARY_DIR}/WebConfig.qrc )
SET(WebConfig_RESOURCES ${CMAKE_BINARY_DIR}/WebConfig.qrc)

add_library(webserver
	${WebConfig_SOURCES}
	${WebConfig_RESOURCES}
)

target_link_libraries(webserver
	hyperhdr-base
	hyperhdr-utils
	hyperhdr-api
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if (USE_STATIC_QT_PLUGINS)
	target_link_libraries(webserver ${STATIC_QT_PLUGINS_LIBS})
endif()

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(webserver REUSE_FROM precompiled_hyperhdr_headers)
endif()
