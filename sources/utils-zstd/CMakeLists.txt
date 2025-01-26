# Define the current source locations

SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/utils-zstd)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/utils-zstd)

FILE ( GLOB_RECURSE utils_zstd_SOURCES "${CURRENT_HEADER_DIR}/*.h" "${CURRENT_SOURCE_DIR}/*.cpp" )

if (USE_SHARED_LIBS)
	add_library(utils-zstd SHARED ${utils_zstd_SOURCES})
	if(WIN32)
	   target_compile_definitions(utils-zstd
		  INTERFACE
			 "_ZSTD_SHARED_API=__declspec(dllimport)"
		  PRIVATE
			 "_ZSTD_SHARED_API=__declspec(dllexport)"
	   )
	else()
	   target_compile_definitions(utils-zstd
		  INTERFACE
			 "_ZSTD_SHARED_API=__attribute__((visibility(\"default\")))"
	   )
	endif()
	install(
		TARGETS   utils-zstd
		LIBRARY   DESTINATION "${CMAKE_INSTALL_LIBDIR}"
		ARCHIVE   DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	)
else()
	add_library(utils-zstd STATIC ${utils_zstd_SOURCES})
endif()

target_link_libraries(utils-zstd PRIVATE zstd::zstd)
target_include_directories(utils-zstd PRIVATE zstd::zstd)