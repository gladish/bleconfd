cmake_minimum_required (VERSION 2.8)

project (bleconfd)
include (ExternalProject)

macro (set_define def)
  if (${def})
    if (CMAKE_VERSION VERSION_LESS "3.0")
      set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D${def}=1")
      set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -D${def}=1")
      set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D${def}=1")
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -D${def}=1")
    else()
      add_compile_options("-D${def}")
    endif()
  endif()
endmacro()


add_definitions("-DCONFIG_CTRL_IFACE")
add_definitions("-DCONFIG_CTRL_IFACE_UNIX")
add_definitions("-DWITH_BLUEZ=1")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}) 

if (CMAKE_VERSION VERSION_LESS "3.1")
  # message("CMAKE ${CMAKE_CXX_COMPILER_ID}")
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set (CMAKE_CXX_FLAGS "-std=gnu++11 ${CMAKE_CXX_FLAGS}")
  endif ()
else ()
  set (CMAKE_CXX_STANDARD 11)
endif ()

ExternalProject_Add (
  bluez
  URL http://www.kernel.org/pub/linux/bluetooth/bluez-5.50.tar.xz
  CONFIGURE_COMMAND ./configure
  INSTALL_COMMAND ""
  BUILD_IN_SOURCE 1
  PREFIX deps
  BUILD_COMMAND make)

ExternalProject_Add (
  cJSON
  GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
  GIT_TAG v1.6.0
  INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}
  INSTALL_COMMAND ""
  CONFIGURE_COMMAND ""
  BUILD_IN_SOURCE 1
  PREFIX deps
  BUILD_COMMAND make)

ExternalProject_Add (
  hostapd
  GIT_REPOSITORY http://w1.fi/hostap.git
  GIT_TAG hostap_2_8
  INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}
  INSTALL_COMMAND ""
  CONFIGURE_COMMAND ""
  BUILD_IN_SOURCE 1
  PREFIX deps
  BUILD_COMMAND "")

link_directories(
  ${CMAKE_CURRENT_BINARY_DIR}/deps/src/cJSON
  ${CMAKE_CURRENT_BINARY_DIR}/deps/src/bluez/src/.libs
  ${CMAKE_CURRENT_BINARY_DIR}/deps/src/bluez/lib/.libs)

add_executable (bleconfd
	main.cc
	jsonrpc.cc
	rpclogger.cc
	util.cc
	rpcserver.cc
	ecdh.cc
	services/wifiservice.cc
	services/netservice.cc
	services/netservice.cc
	services/appsettings.cc
  bluez/beacon.cc
  bluez/bleclass.cc
  bluez/gattServer.cc
  ${CMAKE_CURRENT_BINARY_DIR}/deps/src/hostapd/src/common/wpa_ctrl.c
  ${CMAKE_CURRENT_BINARY_DIR}/deps/src/hostapd/src/utils/os_unix.c)

add_dependencies (bleconfd cJSON hostapd bluez)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/deps/src/cJSON
    ${CMAKE_CURRENT_BINARY_DIR}/deps/src/hostapd/src/common
    ${CMAKE_CURRENT_BINARY_DIR}/deps/src/hostapd/src/utils
    ${CMAKE_CURRENT_BINARY_DIR}/deps/src/bluez
    /usr/include/glib-2.0
    /usr/lib/arm-linux-gnueabihf/glib-2.0/include)

target_link_libraries (bleconfd
  ${LIBRARY_LINKER_OPTIONS}
  -pthread
  -lcrypto
  -lglib-2.0
  -lshared-mainloop
  -lbluetooth-internal
  -lcjson)
