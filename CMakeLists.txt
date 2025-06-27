cmake_minimum_required(VERSION 3.10)
project(littlesheens C)

set(PLUGIN_FOLDER "sheens-plugin")
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused-value -fno-asynchronous-unwind-tables -ffunction-sections -I ${CMAKE_SOURCE_DIR} -fPIC")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lm -ldl")
add_definitions(-D_POSIX_C_SOURCE=200809L)
add_definitions(-D_XOPEN_SOURCE=500)
add_definitions(-D__USE_GNU)
add_definitions(-DPLUGIN_FOLDER="${PLUGIN_FOLDER}")
add_definitions(-DPLUGIN_SEARCH_PATHS=".,../,/usr/lib")

if (APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DOSX -Wno-typedef-redefinition -Wno-unused-but-set-variable")
    # Adding debug flags to MAC builds by default
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fno-inline -O0 -g3 -ggdb3")
endif()

find_program(YAML2JSON yaml2json)
if(NOT YAML2JSON)
    message(FATAL_ERROR "yaml2json not found, run:\ngo install github.com/bronze1man/yaml2json@latest")
endif()

find_program(MINIFY minify)
if(NOT MINIFY)
    message(FATAL_ERROR "minify not found, run:\ngo install github.com/tdewolff/minify/cmd/minify@latest")
endif()

find_package(CURL)
if(NOT CURL_FOUND)
    message(FATAL_ERROR "libcurl is required but not found.")
endif()

find_library(YAML_LIBRARY NAMES yaml libyaml)
find_path(YAML_INCLUDE_DIR yaml.h)
if (NOT YAML_LIBRARY OR NOT YAML_INCLUDE_DIR)
    if (APPLE)
        message(FATAL_ERROR "libyaml not found, run:\nbrew install libyaml")
    else()
        message(FATAL_ERROR "libyaml not found, run:\napt-get install libyaml-dev")
    endif()
endif()

find_library(RBUS_LIBRARY NAMES rbus)
find_library(RBUS_CORE_LIBRARY NAMES rbuscore)
find_path(RBUS_INCLUDE_DIR NAMES rbus.h PATH_SUFFIXES rbus)
find_path(RTMSG_INCLUDE_DIR NAMES rtMessage.h PATH_SUFFIXES rtmessage)
if(NOT RBUS_LIBRARY OR NOT RBUS_CORE_LIBRARY OR NOT RBUS_INCLUDE_DIR OR NOT RBUS_INCLUDE_DIR)
    if (APPLE)
        message(FATAL_ERROR "rbus not found, run:\nbrew tap stepherg/tap\nbrew install rbus")
    else()
        message(FATAL_ERROR "rbus not found")
    endif()
endif()

find_library(CJSON_LIBRARY NAMES cjson)
find_path(CJSON_INCLUDE_DIR NAMES cJSON.h PATH_SUFFIXES cjson)
if(NOT CJSON_LIBRARY OR NOT CJSON_INCLUDE_DIR)
   message(FATAL_ERROR "cjson library not found")
endif()

# Duktape configuration
set(DUKVERSION "duktape-2.7.0")
set(DUK_SRC "${CMAKE_SOURCE_DIR}/${DUKVERSION}/src")
set(DUK_EXTRAS "${CMAKE_SOURCE_DIR}/${DUKVERSION}/extras")

set(SHEENS_SRC "${CMAKE_SOURCE_DIR}/src")

# Directories
set(LIB_DIR "${CMAKE_SOURCE_DIR}/lib")
set(OUT_DIR "${CMAKE_BINARY_DIR}/${PLUGIN_FOLDER}")
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

if (BUILD_DUKTAPE_SRC)
    file(GLOB DUK_FILES "${CMAKE_SOURCE_DIR}/${DUKVERSION}/src-separate/*.c")
else()
    file(GLOB DUK_FILES "${CMAKE_SOURCE_DIR}/${DUKVERSION}/src/*.c")
endif()

# libduktape shared library
add_library(duktape SHARED 
    ${DUK_FILES}
    ${DUK_EXTRAS}/print-alert/duk_print_alert.c
    ${DUK_EXTRAS}/module-duktape/duk_module_duktape.c
    ${DUK_EXTRAS}/module-node/duk_module_node.c
)
target_include_directories(duktape PRIVATE ${DUK_SRC} ${DUK_EXTRAS})
target_link_libraries(duktape PRIVATE m)

# libmachines shared library
add_library(machines SHARED
   ${SHEENS_SRC}/machines.c 
   ${SHEENS_SRC}/duk_console.c 
   ${SHEENS_SRC}/rbus_duktape.c
   machines_js.c
)

target_include_directories(machines PRIVATE ${DUK_SRC} ${DUK_EXTRAS}/print-alert ${DUK_EXTRAS}/module-duktape ${DUK_EXTRAS}/module-node ${DUK_EXTRAS}/console ${RBUS_INCLUDE_DIR} ${RTMSG_INCLUDE_DIR})
target_link_libraries(machines PRIVATE duktape ${RBUS_LIBRARY} ${RBUS_CORE_LIBRARY})

file(GLOB JS_FILES "${JS_DIR}/*.js")
file(GLOB LIB_JS_FILES "${LIB_DIR}/*.js")
set(JSFILES "${JS_FILES}" "${LIB_JS_FILES}")

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
    COMMAND ${SHEENS_SRC}/embedstr.sh mach_machines_js machines.js.terminated machines_js.c
    COMMAND rm machines.js.terminated
    DEPENDS machines.js ${SHEENS_SRC}/embedstr.sh
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

# Convert node YAML specs to JS
set(SPECS_DIR "${CMAKE_SOURCE_DIR}/node/specs")
set(SPECS_SRC_DIR "${CMAKE_SOURCE_DIR}/node/spec_source")
file(MAKE_DIRECTORY ${SPECS_DIR})
file(GLOB YAML_FILES "${SPECS_SRC_DIR}/*.yaml")
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
add_executable(demo ${SHEENS_SRC}/demo.c ${SHEENS_SRC}/util.c)
target_include_directories(demo PRIVATE ${DUK_SRC})
target_link_libraries(demo PRIVATE machines duktape)

add_executable(sheensio ${SHEENS_SRC}/sheensio.c)
target_include_directories(sheensio PRIVATE ${DUK_SRC})
target_link_libraries(sheensio PRIVATE machines duktape)

add_executable(driver ${SHEENS_SRC}/driver.c ${SHEENS_SRC}/util.c)
target_include_directories(driver PRIVATE ${DUK_SRC})
target_link_libraries(driver PRIVATE machines duktape)

# Dynamic libraries from lib directory
file(GLOB LIB_SRCS "${LIB_DIR}/*.c")
foreach(src ${LIB_SRCS})
    get_filename_component(filename ${src} NAME_WE)
    add_library(${filename} SHARED ${src})
    target_include_directories(${filename} PRIVATE ${SHEENS_SRC} ${DUK_SRC} ${YAML_INCLUDE_DIR} ${RBUS_INCLUDE_DIR} ${RTMSG_INCLUDE_DIR})
    target_link_libraries(${filename} PRIVATE duktape curl ${YAML_LIBRARY} ${RBUS_LIBRARY} ${RBUS_CORE_LIBRARY})
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
