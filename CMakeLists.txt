cmake_minimum_required(VERSION 2.8.12)
project(gifdec C)

option(BUILD_SHARED "Build shared library [ON/OFF]" ON)
option(BUILD_STATIC "Build static library [ON/OFF]" OFF)
option(BUILD_EXAMPLE "Build example executable [ON/OFF]" OFF)

if(BUILD_SHARED)
	add_library(gifdec SHARED gifdec.c gifdec.h)
endif()

if(BUILD_STATIC)
	add_library(gifdec STATIC gifdec.c gifdec.h)
endif()

if(BUILD_EXAMPLE)
	include(FindPkgConfig)
	pkg_search_module(SDL2 sdl2)

	add_executable(example example.c)
	target_include_directories(example PUBLIC "${SDL2_INCLUDE_DIRS}")
	target_link_libraries(example gifdec "${SDL2_LIBRARIES}")

	enable_testing()
	add_test(ExampleTest example "${CMAKE_SOURCE_DIR}/test.gif")
endif()
