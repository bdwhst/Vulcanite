# Function for building single example
function(buildExample EXAMPLE_NAME SPIRV_DEPENDENCIES)
	SET(EXAMPLE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_NAME})
	message(STATUS "Generating project file for example in ${EXAMPLE_FOLDER}")
	# Main
	file(GLOB SOURCE *.cpp ${BASE_HEADERS} ${EXAMPLE_FOLDER}/*.cpp)
	SET(MAIN_CPP ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.cpp)
	if(EXISTS ${EXAMPLE_FOLDER}/main.cpp)
		SET(MAIN_CPP ${EXAMPLE_FOLDER}/main.cpp)
	ENDIF()
	if(EXISTS ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.h)
		SET(MAIN_HEADER ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.h)
	ENDIF()
	# imgui example requires additional source files
	IF(${EXAMPLE_NAME} STREQUAL "imgui")
		file(GLOB ADD_SOURCE "../external/imgui/*.cpp")
		SET(SOURCE ${SOURCE} ${ADD_SOURCE})
	ENDIF()
	# wayland requires additional source files
	IF(USE_WAYLAND_WSI)
		SET(SOURCE ${SOURCE} ${CMAKE_BINARY_DIR}/xdg-shell-client-protocol.h ${CMAKE_BINARY_DIR}/xdg-shell-protocol.c)
	ENDIF()
	# Add shaders
	set(SHADER_DIR_GLSL "../shaders/glsl/${EXAMPLE_NAME}")
	file(GLOB SHADERS_GLSL "${SHADER_DIR_GLSL}/*.vert" "${SHADER_DIR_GLSL}/*.frag" "${SHADER_DIR_GLSL}/*.comp" "${SHADER_DIR_GLSL}/*.geom" "${SHADER_DIR_GLSL}/*.tesc" "${SHADER_DIR_GLSL}/*.tese" "${SHADER_DIR_GLSL}/*.mesh" "${SHADER_DIR_GLSL}/*.task" "${SHADER_DIR_GLSL}/*.rgen" "${SHADER_DIR_GLSL}/*.rchit" "${SHADER_DIR_GLSL}/*.rmiss" "${SHADER_DIR_GLSL}/*.rcall" "${SHADER_DIR_GLSL}/*.rahit" "${SHADER_DIR_GLSL}/*.rint" "${SHADER_DIR_GLSL}/*.glsl")
	set(SHADER_DIR_HLSL "../shaders/hlsl/${EXAMPLE_NAME}")
	file(GLOB SHADERS_HLSL "${SHADER_DIR_HLSL}/*.vert" "${SHADER_DIR_HLSL}/*.frag" "${SHADER_DIR_HLSL}/*.comp" "${SHADER_DIR_HLSL}/*.geom" "${SHADER_DIR_HLSL}/*.tesc" "${SHADER_DIR_HLSL}/*.tese" "${SHADER_DIR_HLSL}/*.mesh" "${SHADER_DIR_HLSL}/*.task" "${SHADER_DIR_HLSL}/*.rgen" "${SHADER_DIR_HLSL}/*.rchit" "${SHADER_DIR_HLSL}/*.rmiss" "${SHADER_DIR_HLSL}/*.rcall" "${SHADER_DIR_HLSL}/*.rahit" "${SHADER_DIR_HLSL}/*.rint")
	source_group("Shaders\\GLSL" FILES ${SHADERS_GLSL})
	source_group("Shaders\\HLSL" FILES ${SHADERS_HLSL})
	# Add optional readme / tutorial
	file(GLOB README_FILES "${EXAMPLE_FOLDER}/*.md")

	function(ExternalTarget folder target)
  	set_property(TARGET ${target} PROPERTY FOLDER ${folder})
	endfunction(ExternalTarget)

	if(WIN32)
		add_executable(${EXAMPLE_NAME} WIN32 ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS_GLSL} ${SHADERS_HLSL} ${README_FILES})
		target_link_libraries(${EXAMPLE_NAME} base ${Vulkan_LIBRARY} ${WINLIBS})
		foreach(SHADER_SOURCE ${SHADERS_GLSL})
		
		get_filename_component(fname ${SHADER_SOURCE} NAME)
		get_filename_component(shader_dir ${SHADER_SOURCE} DIRECTORY)
		
		set(SPIRV_OUTPUT "${shader_dir}/${fname}.spv")
		
		# Use add_custom_command to specify how to generate the SPIR-V file.
		add_custom_command(
		OUTPUT ${SPIRV_OUTPUT}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${shader_dir}
		COMMAND $ENV{VK_SDK_PATH}/Bin/glslangValidator.exe -V ${SHADER_SOURCE} -o ${SPIRV_OUTPUT} -g
		DEPENDS ${SHADER_SOURCE}
		COMMENT "Compiling ${SHADER_SOURCE} to SPIR-V"
		)
		
		list(APPEND SPIRV_DEPENDENCIES ${SPIRV_OUTPUT})
		
		endforeach()

		

		# TODO: Build shaders on not windows


	else(WIN32)
		add_executable(${EXAMPLE_NAME} ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS_GLSL} ${SHADERS_HLSL} ${README_FILES})
		target_link_libraries(${EXAMPLE_NAME} base )
	endif(WIN32)

	file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
	set_target_properties(${EXAMPLE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
	if(${EXAMPLE_NAME} STREQUAL "texture3d")
		if(APPLE)
			# SRS - Use MacPorts paths as default since the same on x86 and Apple Silicon, can override for homebrew on cmake command line
			if(NOT OpenMP_omp_LIBRARY AND EXISTS /opt/local/lib/libomp/libomp.dylib)
				set(OpenMP_omp_LIBRARY /opt/local/lib/libomp/libomp.dylib)
			endif()
			if(CMAKE_C_COMPILER_ID MATCHES "Clang\$")
				set(OpenMP_C_FLAGS "-Xclang -fopenmp")
				set(OpenMP_C_LIB_NAMES "omp")
				if(NOT OpenMP_C_INCLUDE_DIR AND EXISTS /opt/local/include/libomp)
					set(OpenMP_C_INCLUDE_DIR /opt/local/include/libomp)
				endif()
			endif()
			if(CMAKE_CXX_COMPILER_ID MATCHES "Clang\$")
				set(OpenMP_CXX_FLAGS "-Xclang -fopenmp")
				set(OpenMP_CXX_LIB_NAMES "omp")
				if(NOT OpenMP_CXX_INCLUDE_DIR AND EXISTS /opt/local/include/libomp)
					set(OpenMP_CXX_INCLUDE_DIR /opt/local/include/libomp)
				endif()
			endif()
		endif()
		find_package(OpenMP)
		if(OpenMP_CXX_FOUND)
			link_directories(${OpenMP_CXX_LIBRARY_DIRS})
		endif()
	endif()

	if(RESOURCE_INSTALL_DIR)
		install(TARGETS ${EXAMPLE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()

	set(${SPIRV_DEPENDENCIES} "${${SPIRV_DEPENDENCIES}}" PARENT_SCOPE)
endfunction(buildExample)

# Build all examples
function(buildExamples)
	
	foreach(EXAMPLE ${EXAMPLES})
		set(SPIRV_DEPENDENCIES "")
		buildExample(${EXAMPLE} SPIRV_DEPENDENCIES)
		add_custom_target(${EXAMPLE}_shader_spv DEPENDS ${SPIRV_DEPENDENCIES})
		add_dependencies(${EXAMPLE} ${EXAMPLE}_shader_spv base_shader_spv)

		set_target_properties(${EXAMPLE}_shader_spv PROPERTIES FOLDER "CMakePredefinedTargets")
	endforeach(EXAMPLE)
	
endfunction(buildExamples)

set(EXAMPLES
	pbribl
	pbrtexture
)

function(compileBaseShaders)
	set(SHADER_DIR_GLSL "../shaders/glsl/base")
	file(GLOB SHADERS_GLSL "${SHADER_DIR_GLSL}/*.vert" "${SHADER_DIR_GLSL}/*.frag" "${SHADER_DIR_GLSL}/*.comp" "${SHADER_DIR_GLSL}/*.geom" "${SHADER_DIR_GLSL}/*.tesc" "${SHADER_DIR_GLSL}/*.tese" "${SHADER_DIR_GLSL}/*.mesh" "${SHADER_DIR_GLSL}/*.task" "${SHADER_DIR_GLSL}/*.rgen" "${SHADER_DIR_GLSL}/*.rchit" "${SHADER_DIR_GLSL}/*.rmiss" "${SHADER_DIR_GLSL}/*.rcall" "${SHADER_DIR_GLSL}/*.rahit" "${SHADER_DIR_GLSL}/*.rint" "${SHADER_DIR_GLSL}/*.glsl")
	set(SPIRV_DEPENDENCIES "")
	foreach(SHADER_SOURCE ${SHADERS_GLSL})
		
		get_filename_component(fname ${SHADER_SOURCE} NAME)
		get_filename_component(shader_dir ${SHADER_SOURCE} DIRECTORY)
		
		set(SPIRV_OUTPUT "${shader_dir}/${fname}.spv")
		
		# Use add_custom_command to specify how to generate the SPIR-V file.
		add_custom_command(
		OUTPUT ${SPIRV_OUTPUT}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${shader_dir}
		COMMAND $ENV{VK_SDK_PATH}/Bin/glslangValidator.exe -V ${SHADER_SOURCE} -o ${SPIRV_OUTPUT} -g
		DEPENDS ${SHADER_SOURCE}
		COMMENT "Compiling ${SHADER_SOURCE} to SPIR-V"
		)
		
		list(APPEND SPIRV_DEPENDENCIES ${SPIRV_OUTPUT})
		
		endforeach()
	add_custom_target(base_shader_spv DEPENDS ${SPIRV_DEPENDENCIES})

endfunction(compileBaseShaders)


compileBaseShaders()
buildExamples()
