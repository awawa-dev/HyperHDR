# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber/framebuffer)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/framebuffer)

# Grabber
FILE ( GLOB FRAMEBUFFER_SOURCES "${CURRENT_HEADER_DIR}/*.h" "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(framebuffer-grabber ${FRAMEBUFFER_SOURCES} )

target_link_libraries(framebuffer-grabber hyperhdr-base ${QT_LIBRARIES} )

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(framebuffer-grabber REUSE_FROM precompiled_hyperhdr_headers)
endif()
