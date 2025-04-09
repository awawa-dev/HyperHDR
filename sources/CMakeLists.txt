if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/precompiled_hyperhdr_headers.cpp "void precompiledHeadersHyperHDR(){};")
	add_library(precompiled_hyperhdr_headers ${CMAKE_CURRENT_BINARY_DIR}/precompiled_hyperhdr_headers.cpp)
	target_link_libraries(precompiled_hyperhdr_headers PRIVATE Qt${Qt_VERSION}::Network Qt${Qt_VERSION}::Core)
	target_precompile_headers(precompiled_hyperhdr_headers PRIVATE ${CMAKE_SOURCE_DIR}/include/precompiled_hyperhdr_headers/AllHeaders_pch.h )
endif()

# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources)

add_subdirectory(api)
add_subdirectory(base)
add_subdirectory(blackborder)
add_subdirectory(commandline)
add_subdirectory(db)
add_subdirectory(effects)
add_subdirectory(flatbuffers)
add_subdirectory(grabber)
add_subdirectory(hyperimage)
add_subdirectory(image)
add_subdirectory(jsonserver)
add_subdirectory(json-utils)
add_subdirectory(led-drivers)
add_subdirectory(led-strip)
add_subdirectory(lut-calibrator)
add_subdirectory(performance-counters)
add_subdirectory(sound-capture)
add_subdirectory(ssdp)
add_subdirectory(suspend-handler)
add_subdirectory(systray)
add_subdirectory(utils)
add_subdirectory(utils-image)
add_subdirectory(webserver)

if(ENABLE_ZSTD)
	add_subdirectory(utils-zstd)
endif()


if (ENABLE_PROTOBUF)
	add_subdirectory(proto-nano-server)
endif()

if (ENABLE_CEC)
	add_subdirectory(cec)
endif()

if(ENABLE_BONJOUR)
	add_subdirectory(bonjour)
endif()

if(ENABLE_MQTT)
	add_subdirectory(mqtt)
endif()

# Executables
add_subdirectory(hyperhdr)


