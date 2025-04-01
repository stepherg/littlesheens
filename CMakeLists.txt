cmake_minimum_required(VERSION 3.10)
project(littlesheens C)

set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=c99 -fno-asynchronous-unwind-tables -ffunction-sections -I ${CMAKE_SOURCE_DIR} -fPIC")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lm -ldl")

if (APPLE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DOSX -Wno-unused-but-set-variable")
  # Adding debug flags to MAC builds by default
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fno-inline -O0 -g3 -ggdb3")
endif()

find_program(YAML2JSON yaml2json)
if(YAML2JSON)
  message(STATUS "Found yaml2json at: ${YAML2JSON}")
else()
  message(FATAL_ERROR "yaml2json is required but not found.")
endif()

find_program(MINIFY minify)
if(MINIFY )
  message(STATUS "Found minify at: ${MINIFY}")
else()
  message(FATAL_ERROR "minify is required but not found.")
endif()

# Duktape configuration
set(DUKVERSION "duktape-2.7.0")
set(DUK_SRC "${CMAKE_SOURCE_DIR}/${DUKVERSION}/src")
set(DUK_EXTRAS "${CMAKE_SOURCE_DIR}/${DUKVERSION}/extras/print-alert")

# Directories
set(LIB_DIR "${CMAKE_SOURCE_DIR}/lib")
set(OUT_DIR "${CMAKE_BINARY_DIR}/lib")
set(JS_DIR "${CMAKE_SOURCE_DIR}/js")
set(SPEC_DIR "${CMAKE_SOURCE_DIR}/specs")

# Create output directory
file(MAKE_DIRECTORY ${OUT_DIR})

# Copy test_js directory
file(COPY ${CMAKE_SOURCE_DIR}/test_js DESTINATION ${CMAKE_BINARY_DIR})

# Download and extract Duktape
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${DUKVERSION}")
    file(DOWNLOAD "http://duktape.org/${DUKVERSION}.tar.xz" "${CMAKE_BINARY_DIR}/${DUKVERSION}.tar.xz")
    execute_process(COMMAND tar xf "${CMAKE_BINARY_DIR}/${DUKVERSION}.tar.xz" WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

# libduktape shared library
add_library(duktape SHARED 
    ${DUK_SRC}/duktape.c 
    ${DUK_EXTRAS}/duk_print_alert.c
)
target_include_directories(duktape PRIVATE ${DUK_SRC} ${DUK_EXTRAS})
target_link_libraries(duktape PRIVATE m)

# libmachines shared library
add_library(machines SHARED 
    machines.c 
    machines_js.c
)
target_include_directories(machines PRIVATE ${DUK_SRC} ${DUK_EXTRAS})
target_link_libraries(machines PRIVATE duktape)

file(GLOB JS_FILES "${JS_DIR}/*.js")
file(GLOB LIB_JS_FILES "${LIB_DIR}/*.js")
set(JSFILES "${JS_FILES}" "${LIB_JS_FILES}" "${CMAKE_SOURCE_DIR}/driver.js")

# Custom command for machines.js and machines_js.c
add_custom_command(
    OUTPUT machines.js
    COMMAND cat ${JSFILES} > machines.js
    DEPENDS ${JSFILES}
)

add_custom_command(
    OUTPUT machines_js.c
    COMMAND minify machines.js > machines.js.terminated
    COMMAND truncate -s +1 machines.js.terminated
    COMMAND ${CMAKE_SOURCE_DIR}/embedstr.sh mach_machines_js machines.js.terminated machines_js.c
    COMMAND rm machines.js.terminated
    DEPENDS machines.js ${CMAKE_SOURCE_DIR}/embedstr.sh
)

# Convert YAML specs to JS
set(SPECS_DIR "${CMAKE_BINARY_DIR}/specs")
file(MAKE_DIRECTORY ${SPECS_DIR})
file(GLOB YAML_FILES "${SPEC_DIR}/*.yaml")
foreach(YAML_FILE ${YAML_FILES})
    get_filename_component(FILE_NAME ${YAML_FILE} NAME_WE)
    set(JS_FILE "${SPECS_DIR}/${FILE_NAME}.js")
    add_custom_command(
        OUTPUT ${JS_FILE}
        COMMAND cat ${YAML_FILE} | yaml2json | jq . > ${JS_FILE}
        DEPENDS ${YAML_FILE}
        COMMENT "Converting ${FILE_NAME}.yaml to ${FILE_NAME}.js"
        VERBATIM
    )
    list(APPEND JS_FILES ${JS_FILE})
endforeach()

add_custom_target(ConvertYamlToJson ALL DEPENDS ${JS_FILES})

# Executables
add_executable(demo demo.c util.c)
target_include_directories(demo PRIVATE ${DUK_SRC})
target_link_libraries(demo PRIVATE machines duktape)

add_executable(sheensio sheensio.c)
target_include_directories(sheensio PRIVATE ${DUK_SRC})
target_link_libraries(sheensio PRIVATE machines duktape)

add_executable(driver driver.c util.c)
target_include_directories(driver PRIVATE ${DUK_SRC})
target_link_libraries(driver PRIVATE machines duktape)

# Dynamic libraries from lib directory
file(GLOB LIB_SRCS "${LIB_DIR}/*.c")
foreach(src ${LIB_SRCS})
    get_filename_component(filename ${src} NAME_WE)
    add_library(${filename} SHARED ${src})
    target_include_directories(${filename} PRIVATE ${DUK_SRC})
    target_link_libraries(${filename} PRIVATE duktape)
    set_target_properties(${filename} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR})
endforeach()

# Main target
add_custom_target(build_all ALL DEPENDS 
    duktape 
    machines 
    demo sheensio driver
)

# Test target
add_custom_target(test
    COMMAND make -C ${CMAKE_BINARY_DIR}/test_js
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/test_js
    DEPENDS driver
    COMMENT "Running make in test_js directory"
)

# Clean target (CMake already provides 'clean' target)
add_custom_target(distclean
    COMMAND ${CMAKE_COMMAND} -E remove *.so machines.js machines_js.c demo sheensio driver
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_SOURCE_DIR}/${DUKVERSION}.tar.xz
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_SOURCE_DIR}/${DUKVERSION}
    DEPENDS clean
)
