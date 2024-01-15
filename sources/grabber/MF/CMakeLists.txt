# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber/MF)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/MF)

FILE ( GLOB WMF_SOURCES "${CURRENT_HEADER_DIR}/*.h" "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(MF-grabber ${WMF_SOURCES} )

target_link_libraries(MF-grabber
	hyperhdr-base
	${QT_LIBRARIES}
)

target_include_directories(MF-grabber PUBLIC ${TURBOJPEG_INCLUDE_DIRS})
target_link_libraries(MF-grabber ${TURBOJPEG_LINK_LIBRARIES})

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(MF-grabber REUSE_FROM precompiled_hyperhdr_headers)
endif()
