# Define the current source locations

SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/utils-xz)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/utils-xz)

FILE ( GLOB_RECURSE utils_xz_SOURCES "${CURRENT_HEADER_DIR}/*.h" "${CURRENT_SOURCE_DIR}/*.cpp" )

if (USE_SHARED_LIBS)
	add_library(utils-xz SHARED ${utils_xz_SOURCES})
	if(WIN32)
	   target_compile_definitions(utils-xz
		  INTERFACE
			 "_XZ_SHARED_API=__declspec(dllimport)"
		  PRIVATE
			 "_XZ_SHARED_API=__declspec(dllexport)"
	   )
	else()
	   target_compile_definitions(utils-xz
		  INTERFACE
			 "_XZ_SHARED_API=__attribute__((visibility(\"default\")))"
	   )
	endif()
	install(
		TARGETS   utils-xz
		LIBRARY   DESTINATION "${CMAKE_INSTALL_LIBDIR}"
		ARCHIVE   DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	)
else()
	add_library(utils-xz STATIC ${utils_xz_SOURCES})
endif()

target_link_libraries(utils-xz PRIVATE LibLZMA::LibLZMA)
target_include_directories(utils-xz PRIVATE LibLZMA::LibLZMA)

