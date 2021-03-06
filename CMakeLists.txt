cmake_minimum_required( VERSION 3.10 FATAL_ERROR )

message( STATUS "==========================================================================\n" )
message( STATUS "project 'FrameGraph' generation started" )

project( "FrameGraph"
		 VERSION 2020.3.2
		 LANGUAGES C CXX )

set_property( GLOBAL PROPERTY USE_FOLDERS ON )
message( STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} (${CMAKE_CXX_COMPILER_VERSION})" )
message( STATUS "target system: ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_VERSION})" )
message( STATUS "host system: ${CMAKE_HOST_SYSTEM_NAME} (${CMAKE_HOST_SYSTEM_VERSION})" )

if (DEFINED ANDROID)
	set( FG_DEFAULT_ENABLE_GLSL_TRACE OFF )
else ()
	set( FG_DEFAULT_ENABLE_GLSL_TRACE ON )
endif ()

if (WIN32)
	set( FG_DEFAULT_ENABLE_OPENVR OFF )
	set( FG_DEFAULT_ENABLE_ASSIMP ON )
	set( FG_DEFAULT_ENABLE_DEVIL ON )
	set( FG_DEFAULT_ALLOW_GPL OFF )
	set( FG_DEFAULT_ENABLE_FFMPEG OFF )
else ()
	set( FG_DEFAULT_ENABLE_OPENVR OFF )
	set( FG_DEFAULT_ENABLE_ASSIMP OFF )
	set( FG_DEFAULT_ENABLE_DEVIL OFF )
	set( FG_DEFAULT_ALLOW_GPL OFF )
	set( FG_DEFAULT_ENABLE_FFMPEG OFF ) # not supported
endif ()

set( FG_NO_EXCEPTIONS ON CACHE BOOL "disable C++ exceptions" )
#set( FG_ENABLE_STDALLOC OFF CACHE BOOL "custom std allocators (optional)" )
set( FG_ENABLE_GLSLANG ON CACHE BOOL "use glslang (optional, required for glsl compilation)" )
set( FG_ENABLE_VMA ON CACHE BOOL "use Vulkan Memory Allocator (required)" )
set( FG_ENABLE_OPENVR ${FG_DEFAULT_ENABLE_OPENVR} CACHE BOOL "use OpenVR (optional)" )
set( FG_ENABLE_SIMPLE_COMPILER_OPTIONS OFF CACHE BOOL "use simplified compiler settings if you have problem with it" )
set( FG_EXTERNALS_USE_STABLE_VERSIONS ON CACHE BOOL "use last stable version instead of master branch" )
set( FG_EXTERNALS_USE_PREBUILD OFF CACHE BOOL "use stable prebuild libraries" )
set( FG_ENABLE_GLSL_TRACE ${FG_DEFAULT_ENABLE_GLSL_TRACE} CACHE BOOL "used for shader debugging and profiling" )
set( FG_VULKAN_VERSION "110" CACHE STRING "choose target Vulkan API version" )
set( FG_ENABLE_MEMLEAK_CHECKS ON CACHE BOOL "" )

# test & samples dependencies
set( FG_ENABLE_TESTS ON CACHE BOOL "enable tests" )
set( FG_ENABLE_GLFW ON CACHE BOOL "use glfw (optional, required for tests)" )
set( FG_ENABLE_SDL2 OFF CACHE BOOL "use SDL2 (optional, required for tests)" )
set( FG_ENABLE_LODEPNG OFF CACHE BOOL "use lodepng (optional, may be used in tests)" )
set( FG_ENABLE_GRAPHVIZ OFF CACHE BOOL "use graphviz (optional)" )
set( FG_ENABLE_SPIRVREFLECT ON CACHE BOOL "for runtime spirv shader reflection (optional)" )

# UI dependencies
set( FG_ENABLE_IMGUI ON CACHE BOOL "use imgui (optional, required for UI extension)" )

# Scene dependencies
set( FG_ENABLE_ASSIMP ${FG_DEFAULT_ENABLE_ASSIMP} CACHE BOOL "use Assimp (optional, required for Scene extension)" )
set( FG_ENABLE_GLM ON CACHE BOOL "use glm (optional, required for Scene extension)" )
set( FG_ENABLE_DEVIL ${FG_DEFAULT_ENABLE_DEVIL} CACHE BOOL "use DevIL (optional, required for Scene extension)" )
set( FG_ENABLE_FFMPEG ${FG_DEFAULT_ENABLE_FFMPEG} CACHE BOOL "use FFmpeg (optional, LGPL license)" )
set( FG_ALLOW_GPL ${FG_DEFAULT_ALLOW_GPL} CACHE BOOL "allow GNU GPL code and libraries" )

set( MAIN_BINARY_DIR "${CMAKE_BINARY_DIR}/bin" CACHE INTERNAL "" FORCE )

if (DEFINED ANDROID)
	set( CMAKE_DEBUG_POSTFIX "" )
	set( CMAKE_RELEASE_POSTFIX "" )
else ()
	set( CMAKE_DEBUG_POSTFIX "d" )
	set( CMAKE_RELEASE_POSTFIX "" )
	
	# this settings breaks native debugger on android
	set( CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "installation directory" )
	set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${MAIN_BINARY_DIR}" CACHE INTERNAL "" FORCE )
	set( CMAKE_LIBRARY_OUTPUT_DIRECTORY "${MAIN_BINARY_DIR}" CACHE INTERNAL "" FORCE )
	set( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${MAIN_BINARY_DIR}" CACHE INTERNAL "" FORCE )
endif ()

if (${FG_ENABLE_SIMPLE_COMPILER_OPTIONS})
	include( "cmake/compilers_minimal.cmake" )
else()
	include( "cmake/compilers.cmake" )
endif()
include( "cmake/compiler_tests.cmake" )
include( "cmake/ci_build.cmake" )
include( "cmake/version.cmake" )

if (DEFINED ANDROID)
	add_subdirectory( "external/android" )
else ()
	add_subdirectory( "external" )
endif ()

include( "cmake/project_template.cmake" )
add_subdirectory( "stl" )
add_subdirectory( "extensions/vulkan_loader" )
add_subdirectory( "framegraph" )
add_subdirectory( "extensions/framework" )
add_subdirectory( "extensions/pipeline_compiler" )
add_subdirectory( "extensions/pipeline_reflection" )
add_subdirectory( "extensions/scene" )
add_subdirectory( "extensions/ui" )
add_subdirectory( "extensions/graphviz" )
add_subdirectory( "extensions/video" )

if (${FG_ENABLE_TESTS})
	if (NOT DEFINED ANDROID)
		enable_testing()
	endif ()
	add_subdirectory( "tests/stl" )
	add_subdirectory( "tests/framegraph" )
	add_subdirectory( "tests/framework" )
	add_subdirectory( "tests/pipeline_compiler" )
	add_subdirectory( "tests/pipeline_reflection" )
	add_subdirectory( "tests/scene" )
	add_subdirectory( "tests/ui" )
	add_subdirectory( "tests/android" )
endif ()

message( STATUS "project 'FrameGraph' generation ended" )
message( STATUS "\n==========================================================================" )

