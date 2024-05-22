# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/hyperimage)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/hyperimage)

FILE ( GLOB HYPERIMAGE_SOURCES "${CURRENT_HEADER_DIR}/*.h" )

list(APPEND HYPERIMAGE_SOURCES ${CURRENT_SOURCE_DIR}/HyperImage.cpp)

add_library(hyperimage ${HYPERIMAGE_SOURCES} )

target_include_directories(hyperimage PRIVATE ${CMAKE_SOURCE_DIR}/include/hyperimage/stb)
target_include_directories(hyperimage PRIVATE ${CMAKE_SOURCE_DIR}/external/lunasvg/3rdparty/stb)
target_include_directories(hyperimage PRIVATE ${CMAKE_SOURCE_DIR}/external/lunasvg/3rdparty/plutovg)


target_link_libraries(hyperimage
	hyperhdr-base
	${QT_LIBRARIES}
	lunasvg
	TurboJPEG::TurboJPEG
)
