cmake_minimum_required(VERSION 3.16)
project(HyperHdrBlackBorderTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(Threads REQUIRED)
find_package(Qt6 COMPONENTS Core QUIET)
if(Qt6Core_FOUND)
	set(QT_CORE Qt6::Core)
else()
	find_package(Qt5 COMPONENTS Core REQUIRED)
	set(QT_CORE Qt5::Core)
endif()

set(HYPERHDR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
add_executable(BlackBorderTest
	main.cpp
	${HYPERHDR_ROOT}/sources/blackborder/BlackBorderDetector.cpp
	${HYPERHDR_ROOT}/sources/image/ColorRgb.cpp
	${HYPERHDR_ROOT}/sources/image/Image.cpp
	${HYPERHDR_ROOT}/sources/image/ImageData.cpp
	${HYPERHDR_ROOT}/sources/image/MemoryBuffer.cpp
	${HYPERHDR_ROOT}/sources/image/VideoMemoryManager.cpp
)
target_include_directories(BlackBorderTest PRIVATE ${HYPERHDR_ROOT}/include)
target_link_libraries(BlackBorderTest PRIVATE Threads::Threads ${QT_CORE})
add_test(NAME subtitle_blackborder COMMAND BlackBorderTest)
