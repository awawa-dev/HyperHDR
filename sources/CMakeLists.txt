
# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources)

add_subdirectory(base)
add_subdirectory(commandline)
add_subdirectory(blackborder)
add_subdirectory(jsonserver)
add_subdirectory(flatbufserver)
if (ENABLE_PROTOBUF)
	add_subdirectory(proto-nano-server)
endif()
add_subdirectory(ssdp)
if(ENABLE_BOBLIGHT)
	add_subdirectory(boblightserver)
endif()
add_subdirectory(leddevice)
add_subdirectory(utils)
add_subdirectory(effectengine)
add_subdirectory(grabber)
add_subdirectory(webserver)
add_subdirectory(db)
add_subdirectory(api)

if (ENABLE_CEC)
	add_subdirectory(cec)
endif()

if(ENABLE_AVAHI)
	add_subdirectory(bonjour)
endif()

# Executables
add_subdirectory(hyperhdr)
if (NOT APPLE)
	add_subdirectory(hyperhdr-remote)
endif()


