cmake_minimum_required(VERSION 3.10)

#*****************************************************************#
# Create the master machine .js file. This file will
# then be converted into a hex array that may be
# compiled into a C file.
#
# TODO: The master file needs to be "minimized".
#*****************************************************************#

set(JSFILES "js/match.js" "js/sandbox.js" "js/step.js" "js/prof.js" "driver.js")

add_custom_command(OUTPUT machines_js.c
                   COMMAND cat ${JSFILES} > mach_machines_jssrc
                   COMMAND truncate -s +1 mach_machines_jssrc
                   COMMAND xxd -i mach_machines_jssrc machines_js.c
                   COMMAND ${CMAKE_COMMAND} -E echo "unsigned char* mach_machines_js() { return mach_machines_jssrc; }" >> machines_js.c
                   COMMAND ${CMAKE_COMMAND} -E remove mach_machines_jssrc
                   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                   DEPENDS ${JSFILES}
                   VERBATIM)

add_custom_target(littlesheens_js DEPENDS machines_js.c)

# Find all YAML files
set(SPECS_DIR "${CMAKE_BINARY_DIR}/specs")
file(MAKE_DIRECTORY ${SPECS_DIR})
file(GLOB YAML_FILES "specs/*.yaml")
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


#*****************************************************************#
# The main library that will be generated for littlesheens.
# This depends on Duktape being installed.
#*****************************************************************#

set(LITTLESHEENS_SOURCE
    ${CMAKE_SOURCE_DIR}/machines.c
    ${CMAKE_SOURCE_DIR}/machines_js.c)

add_library(littlesheens SHARED ${LITTLESHEENS_SOURCE})

# Force in the master .js file creation
add_dependencies(littlesheens littlesheens_js)
set_source_files_properties("${CMAKE_SOURCE_DIR}/machines_js.c" PROPERTIES GENERATED TRUE)

target_include_directories(littlesheens PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(littlesheens duktape)

set_target_properties(littlesheens PROPERTIES VERSION 1.0.0)

install(TARGETS littlesheens LIBRARY DESTINATION lib)
install(FILES ${CMAKE_SOURCE_DIR}/machines.h DESTINATION include/littlesheens)


#*****************************************************************#
# If we are NOT building through YOCTO then we want to build
# a sample application.
#*****************************************************************#

if (NOT TARGET_PLATFORM STREQUAL "yocto")
  include_directories("${CMAKE_INSTALL_PREFIX}/include" "${CMAKE_INSTALL_PREFIX}/include/duktape")

  if (NOT TARGET_PLATFORM STREQUAL "darwin")
    set(CMAKE_C_FLAGS "-DOSX -Wno-unused-but-set-variable ${CMAKE_C_FLAGS}")
  endif()

  set(CMAKE_C_FLAGS "-Wall -std=c99 -fno-asynchronous-unwind-tables -ffunction-sections -fPIC ${CMAKE_C_FLAGS}")
  set(CMAKE_SHARED_LINKER_FLAGS "-L${CMAKE_INSTALL_PREFIX}/lib ${CMAKE_SHARED_LINKER_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "-L${CMAKE_INSTALL_PREFIX}/lib ${CMAKE_EXE_LINKER_FLAGS}")
  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

  # 3rd party libraries (duktape)
  include(CMakeLists-ExternalProjects.txt)

  add_dependencies(littlesheens duktape-ext)
  target_link_libraries(littlesheens duktape)

  add_executable(demo demo.c util.c util.h)
  target_link_libraries(demo littlesheens duktape m)

  add_executable(driver driver.c util.c util.h)
  target_link_libraries(driver littlesheens duktape m)

  add_executable(sheensio sheensio.c)
  target_link_libraries(sheensio littlesheens duktape m)
      
  install(TARGETS demo DESTINATION bin)
  install(TARGETS driver DESTINATION bin)
  install(TARGETS sheensio DESTINATION bin)
endif()
