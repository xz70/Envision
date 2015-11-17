cmake_minimum_required(VERSION 3.2.2)

# This is specific for using a Make-based build
# find_program(CCACHE_FOUND ccache)
# if(CCACHE_FOUND)
# 	set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
# 	set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
# endif(CCACHE_FOUND)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED Core Gui Widgets)
add_definitions(-D_REENTRANT)
add_definitions(-DDEBUG) # -DQT_NO_DEBUG <-- get rid of this

set(ENVISION_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(BUILD_DIR ${ENVISION_ROOT_DIR}/DebugBuild)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(BUILD_DIR ${ENVISION_ROOT_DIR}/DebugBuild)
endif()
if (CMAKE_BUILD_TYPE STREQUAL "Release")
	set(BUILD_DIR ${ENVISION_ROOT_DIR}/ReleaseBuild)
endif()

set(PLUGINS_DIR ${BUILD_DIR}/plugins)
include_directories(${ENVISION_ROOT_DIR})
link_directories(${BUILD_DIR} ${PLUGINS_DIR})

# ???
# CONFIG(debug, debug|release):DEFINES += DEBUG

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -pedantic-errors -Wall -W -Werror -Wextra -O2 -fno-omit-frame-pointer -Woverloaded-virtual -Winvalid-pch")

# ???
# clang:QMAKE_CXXFLAGS_WARN_ON += -Wno-unused-private-field

# Enable linking to custom Qt version and libCore from both main executable and plugins
set(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/..:$ORIGIN/qt:$ORIGIN/../qt")

if( WIN32 )
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--export-all-symbols'") #Export all symbols in Windows to save a hassle
endif()

set(COMMON_LIBS "")
if( UNIX )
	set(COMMON_LIBS "-lprofiler")
endif()

function(use_precompiled_header targetName)
	message("Using precompiled header for ${targetName} ")
	
	#TODO: This shouldn't really belong here, but what's a better place?
	add_custom_command(TARGET ${targetName} PRE_LINK
		COMMAND ${ENVISION_ROOT_DIR}/checkers/vera++-check-dir ${CMAKE_CURRENT_SOURCE_DIR}
	)
    
	# A custom target name for the precompiled header
	set(precompiled ${targetName}-Precompiled-Header)
	
	# Create a cpp file which cmake can build to get the precompiled header file
	set(precompiled_cpp ${CMAKE_CURRENT_BINARY_DIR}/precompiled_cpp.cpp)
	if(NOT EXISTS ${precompiled_cpp})
		file(WRITE ${precompiled_cpp} "\n")
	endif()
	
	# The precompiled header target
	add_library(${precompiled} OBJECT src/precompiled.h ${precompiled_cpp})
	set_target_properties(${precompiled} PROPERTIES AUTOMOC OFF)
	target_compile_options(${precompiled} PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/src/precompiled.h -x c++-header)
	target_compile_definitions(${precompiled} PRIVATE ${targetName}_EXPORTS)
	
	# Inherited compilation flags
	get_transitive_link_dependencies_for_precompiled_header(${targetName} ${precompiled})

	# Add another target that will copy the object file generated by the precompiled target
	# and rename it to a suitable .gch file
	set(precompiledCopy ${targetName}-Precompiled-Header-CopyAndRename)
	set(precompiledSubDir ${precompiled})
	add_custom_target(${precompiledCopy}
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${precompiled}.dir/precompiled_cpp.cpp.o ${CMAKE_BINARY_DIR}/${precompiledSubDir}/precompiled.h.gch
		DEPENDS ${precompiled}
	)
	
	add_dependencies(${targetName} ${precompiledCopy})
	target_compile_options(${targetName} PRIVATE -include ${CMAKE_BINARY_DIR}/${precompiledSubDir}/precompiled.h)
endfunction(use_precompiled_header)

function(get_transitive_link_dependencies_for_precompiled_header targetName precompiledTargetName)
	# Look at all the libraries that the target uses and import their settings
	get_target_property(_link_libraries ${targetName} LINK_LIBRARIES)
	foreach(_library IN ITEMS ${_link_libraries})
		if (TARGET ${_library})
			get_target_property(_includes ${_library} INTERFACE_INCLUDE_DIRECTORIES)
			if(_includes)
				target_include_directories(${precompiledTargetName} SYSTEM PRIVATE ${_includes})
			endif()

			get_target_property(_definitions ${_library} INTERFACE_COMPILE_DEFINITIONS)
			if(_definitions)
				target_compile_definitions(${precompiledTargetName} PRIVATE ${_definitions})
			endif()

			get_target_property(_options ${_library} INTERFACE_COMPILE_OPTIONS)
			if(_options)
				target_compile_options(${precompiledTargetName} PRIVATE ${_options})
			endif()

			get_transitive_link_dependencies_for_precompiled_header(${_library} ${precompiledTargetName})
		endif()
	endforeach(_library)
endfunction(get_transitive_link_dependencies_for_precompiled_header)

function(envision_plugin targetName)
	message("Configuring Plugin: ${targetName} ")
	
	target_link_libraries(${targetName} Core)
	target_compile_definitions(${targetName} PRIVATE -DQT_PLUGIN)
	use_precompiled_header(${targetName})
	
	install(TARGETS ${targetName} DESTINATION ${PLUGINS_DIR})
	
	string(TOLOWER ${targetName} _lower_case_name)
	install(FILES "${_lower_case_name}.plugin" DESTINATION ${PLUGINS_DIR})
	
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/styles")
		message("Plugin ${targetName} contains styles")
		install(DIRECTORY styles/ DESTINATION ${BUILD_DIR}/styles)
	endif()
endfunction(envision_plugin)