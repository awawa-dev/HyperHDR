# Define the current source locations

SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/utils-image)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/utils-image)

FILE ( GLOB_RECURSE utils_image_SOURCES "${CURRENT_HEADER_DIR}/*.h" "${CURRENT_SOURCE_DIR}/*.cpp" )

if (USE_SHARED_LIBS)
	add_library(utils-image SHARED ${utils_image_SOURCES})
	if(WIN32)
	   target_compile_definitions(utils-image
		  INTERFACE
			 "_IMAGE_SHARED_API=__declspec(dllimport)"
		  PRIVATE
			 "_IMAGE_SHARED_API=__declspec(dllexport)"
	   )
	else()
	   target_compile_definitions(utils-image
		  INTERFACE
			 "_IMAGE_SHARED_API=__attribute__((visibility(\"default\")))"
	   )
	endif()
	install(
		TARGETS   utils-image
		LIBRARY   DESTINATION "${CMAKE_INSTALL_LIBDIR}"
		ARCHIVE   DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	)
else()
	add_library(utils-image STATIC ${utils_image_SOURCES})
endif()

target_include_directories(utils-image PRIVATE stb lunasvg TurboJPEG::TurboJPEG)

target_link_libraries(utils-image PRIVATE
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
	stb
	lunasvg
	image
	TurboJPEG::TurboJPEG
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(utils-image REUSE_FROM precompiled_hyperhdr_headers)
endif()

