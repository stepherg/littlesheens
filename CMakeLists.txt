cmake_minimum_required(VERSION 3.10)
project(littlesheens C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused-value -fno-asynchronous-unwind-tables -ffunction-sections -I ${CMAKE_SOURCE_DIR} -fPIC")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lm -ldl")
add_definitions(-D_POSIX_C_SOURCE=200809L)
add_definitions(-D_XOPEN_SOURCE=500)
add_definitions(-D__USE_GNU)

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
if(MINIFY)
  message(STATUS "Found minify at: ${MINIFY}")
else()
  message(FATAL_ERROR "minify is required but not found.")
endif()

find_package(CURL)
if(NOT CURL_FOUND)
  message(FATAL_ERROR "libcurl is required but not found.")
endif()

find_library(YAML_LIBRARY NAMES yaml libyaml)
find_path(YAML_INCLUDE_DIR yaml.h)
if (NOT YAML_LIBRARY OR NOT YAML_INCLUDE_DIR)
    message(FATAL_ERROR "libyaml-dev not found.")
endif()

find_library(RBUS_LIBRARY NAMES rbus HINTS "/usr/lib")
find_path(RBUS_INCLUDE_DIR NAMES rbus.h HINTS "/usr/include/rbus")
find_path(RTMSG_INCLUDE_DIR NAMES rtMessage.h HINTS "/usr/include/rtmessage")


# Duktape configuration
set(DUKVERSION "duktape-2.7.0")
set(DUK_SRC "${CMAKE_SOURCE_DIR}/${DUKVERSION}/src")
set(DUK_EXTRAS "${CMAKE_SOURCE_DIR}/${DUKVERSION}/extras")

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
    ${DUK_EXTRAS}/print-alert/duk_print_alert.c
    ${DUK_EXTRAS}/module-duktape/duk_module_duktape.c
    ${DUK_EXTRAS}/module-node/duk_module_node.c
    ${DUK_EXTRAS}/console/duk_console.c
)
target_include_directories(duktape PRIVATE ${DUK_SRC} ${DUK_EXTRAS})
target_link_libraries(duktape PRIVATE m)

# libmachines shared library
add_library(machines SHARED 
    machines.c 
    machines_js.c
)
target_include_directories(machines PRIVATE ${DUK_SRC} ${DUK_EXTRAS}/print-alert ${DUK_EXTRAS}/module-duktape ${DUK_EXTRAS}/module-node ${DUK_EXTRAS}/console)
target_link_libraries(machines PRIVATE duktape)

file(GLOB JS_FILES "${JS_DIR}/*.js")
file(GLOB LIB_JS_FILES "${LIB_DIR}/*.js")
set(JSFILES "${JS_FILES}" "${LIB_JS_FILES}" "${CMAKE_SOURCE_DIR}/driver.js")

file(COPY "${CMAKE_SOURCE_DIR}/telemetry/" DESTINATION ${CMAKE_BINARY_DIR})

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

if (NOT RBUS_LIBRARY OR NOT RBUS_INCLUDE_DIR OR NOT RTMSG_INCLUDE_DIR)
    message(WARNING "RBUS not found... building stubs")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-typedef-redefinition")
    set(RBUS_INCLUDE_DIR "${LIB_DIR}/rbus/include")
    set(RTMSG_INCLUDE_DIR "${LIB_DIR}/rbus/include")
    file(GLOB RBUS_SRCS "${LIB_DIR}/rbus/*.c")
    add_library(rbus SHARED ${RBUS_SRCS})
    target_include_directories(rbus PRIVATE ${RBUS_INCLUDE_DIR} ${RTMSG_INCLUDE_DIR})
    set(RBUS_LIBRARY rbus)
endif()

# Dynamic libraries from lib directory
file(GLOB LIB_SRCS "${LIB_DIR}/*.c")
foreach(src ${LIB_SRCS})
    get_filename_component(filename ${src} NAME_WE)
    add_library(${filename} SHARED ${src})
    target_include_directories(${filename} PRIVATE ${DUK_SRC} ${YAML_INCLUDE_DIR} ${RBUS_INCLUDE_DIR} ${RTMSG_INCLUDE_DIR})
    target_link_libraries(${filename} PRIVATE duktape curl ${YAML_LIBRARY} ${RBUS_LIBRARY})
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
