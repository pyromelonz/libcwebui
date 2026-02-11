cmake_minimum_required(VERSION 2.6)


PROJECT(webserver)

#-Wall

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Debug CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel MyOne."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)
#wie setzt man den build type sonst noch ?


# Flags ohne build type gelten immer
# und die build type flags werden angehängt

#SET(CMAKE_C_COMPILER "icc")

#SET(CMAKE_C_FLAGS " -O3 -Wlogical-op -Wl,--version-script=webserver.map -fvisibility=hidden -g -Wall -DLINUX -Wc++-compat --std=c99 -D_GNU_SOURCE   "  ) # -pedantic
#SET(CMAKE_C_FLAGS " -O0 -Wlogical-op -Wl,--version-script=webserver.map -fvisibility=hidden -g -Wall -DLINUX -Wc++-compat --std=c99 -D_GNU_SOURCE   "  ) # -pedantic
#SET(CMAKE_C_FLAGS "-g -Wall -DLINUX -D_GNU_SOURCE" ) #-pedantic
#SET(CMAKE_C_FLAGS_DEBUG -g3)
SET(CMAKE_C_FLAGS_RELEASE )

SET(CMAKE_CXX_FLAGS "-pipe -Wall -DWIN32 ")# -Wall  -Wextra -Wfatal-errors")# stehen bleiben bei error
SET(CMAKE_CXX_FLAGS_DEBUG -ggdb3)
SET(CMAKE_CXX_FLAGS_RELEASE -O2)
SET(CMAKE_CXX_FLAGS_MYONE -My)

#set(CMAKE_C_FLAGS_DEBUG "-Wall -g -DLINUX")
#set(CMAKE_C_FLAGS_RELEASE "-Wall -DLINUX")

SET( MY_SOURCES
src/builtinFunctions.c
src/convert.c
src/cookie.c
src/conditions.c
src/engine.c
src/engine_api_extensions.c
src/engine_functions.c
src/engine_if.c
src/engine_parser.c
src/engine_tags.c
src/globals.c
src/header.c
src/header_parser.c
src/header_send.c
src/linked_list.c
src/log.c
src/message_queue.c
src/file_manager.c
src/server.c
src/server_config.c
src/server_chunk_functions.c
src/server_error_pages.c
src/session.c
src/system.c
src/system_file_access.c
src/system_file_access_binary.c
src/system_file_access_fs.c
src/system_file_access_nfs.c
src/system_file_access_utils.c
src/system_file_access_wnfs.c
src/system_file_cache.c
src/system_memory.c
src/system_memory_msc.c
src/system_sockets.c
src/system_sockets_container.c
src/system_sockets_events.c
src/system_sockets_select.c
src/system_sockets_recv.c
src/system_sockets_send.c
src/utils.c
src/hashmap.c
src/variable.c
src/variable_store.c
src/variable_render.c
src/variable_global.c
src/webserver_api_functions.c
src/websockets.c
src/websockets_send_recv.c
src/websocket_handler.c
#platform/linux/platform-sockets.c
#platform/linux/system-unix.c
#platform/linux/platform-file-access.c

platform/win/platform-sockets.c
platform/win/platform-win.c
platform/win/platform-file-access.c

ssl/openssl.c
ssl/yassl.c
ssl/gnutls.c

third_party/src/rb_tree/red_black_tree.c
third_party/src/rb_tree/misc.c
third_party/src/rb_tree/stack.c

third_party/src/is_utf8/is_utf8.c

third_party/src/miniz/miniz.c

)

include_directories(include platform/include)
include_directories(include platform/win)
include_directories(third_party/include)
include_directories(third_party/src/rb_tree)
include_directories(third_party/src/is_utf8)
include_directories(third_party/src/miniz)
include_directories(../libevent/include)
include_directories(.)

#add_subdirectory (src)


ADD_LIBRARY(WebserverDebugShared SHARED ${MY_SOURCES} )
set_target_properties(WebserverDebugShared PROPERTIES COMPILE_FLAGS ${CMAKE_C_FLAGS_DEBUG})

#ADD_LIBRARY(WebserverDebugStatic STATIC ${MY_SOURCES})
#set_target_properties(WebserverDebugStatic PROPERTIES COMPILE_FLAGS ${CMAKE_C_FLAGS_DEBUG})

#ADD_LIBRARY(WebserverReleaseShared SHARED ${MY_SOURCES} )
#set_target_properties(WebserverReleaseShared PROPERTIES COMPILE_FLAGS ${CMAKE_C_FLAGS_RELEASE})

#ADD_LIBRARY(WebserverReleaseStatic STATIC ${MY_SOURCES})
#set_target_properties(WebserverReleaseStatic PROPERTIES COMPILE_FLAGS ${CMAKE_C_FLAGS_RELEASE})



