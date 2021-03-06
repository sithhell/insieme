add_module_library(frontend)

target_link_libraries(frontend annotations)
target_link_libraries(frontend common)
target_link_libraries(frontend core)
target_link_libraries(frontend transform)
target_link_libraries(frontend utils)

# Boost
target_link_libraries(frontend ${Boost_LIBRARIES})
target_include_directories(frontend SYSTEM PUBLIC ${Boost_INCLUDE_DIR})

# LLVM / CLANG
target_link_libraries(frontend ${LLVM_LIBRARIES})
target_link_libraries(frontend ${CLANG_LIBRARIES})
target_include_directories(frontend PUBLIC ${LLVM_INCLUDE_DIRS})
target_include_directories(frontend PUBLIC ${CLANG_INCLUDE_DIRS})

# Required for LLVM
add_definitions(-D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS)

# Generate Builtins
set(generate_builtins_output_dir ${CMAKE_CURRENT_BINARY_DIR}/assets/include/insieme/frontend/builtin_headers)
file(MAKE_DIRECTORY ${generate_builtins_output_dir})
run_ruby(
	${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_builtins.rb
	--quiet
	--detect-via-compiler ${INSIEME_CXX_BACKEND_COMPILER}
	--output ${generate_builtins_output_dir}
)
file(GLOB_RECURSE generate_builtins_output ${generate_builtins_output_dir}/*)
target_include_directories(frontend PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/assets/include)
target_sources(frontend PRIVATE ${generate_builtins_output})
msvc_source_group("Generated Builtins" "${generate_builtins_output}" STRIP_ABS ${CMAKE_CURRENT_BINARY_DIR}/assets/include)
add_definitions(-DGENERATED_BUILTINS_DIR="${generate_builtins_output_dir}/")

glob_tests(frontend_tests test)
foreach(test ${frontend_tests})
	string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/test/" "" test_relpath ${test})
	if(${test_relpath} MATCHES ".*fragment_conversion.*")
		add_module_unittest(frontend ${test} PARALLEL)
	else()
		add_module_unittest(frontend ${test})
	endif()
endforeach(test)

# add test input sources
if(MSVC)
	glob_headers(frontend_test_input_incs test/inputs)
	glob_sources(frontend_test_input_srcs test/inputs)
	set(frontend_test_input_srcs ${frontend_test_input_srcs} ${frontend_test_input_incs})
	add_custom_target(frontend_test_input SOURCES ${frontend_test_input_srcs})
	msvc_source_group("Input Files" "${frontend_test_input_srcs}" STRIP test/inputs)
	set_target_properties(frontend_test_input PROPERTIES FOLDER "frontend/Tests")
endif()
