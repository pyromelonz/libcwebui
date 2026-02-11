# Common source files for all config variants
# Include this with: include ../files.mk

FILES= \
	../../../lib/src/base64.c                       ../../../lib/src/sha1.c \
	../../../lib/src/builtinFunctions.c             ../../../lib/src/convert.c \
	../../../lib/src/cookie.c                       ../../../lib/src/conditions.c \
	../../../lib/src/engine.c                       ../../../lib/src/engine_api_extensions.c \
	../../../lib/src/engine_functions.c             ../../../lib/src/engine_if.c \
	../../../lib/src/engine_tags.c                  ../../../lib/src/engine_parser.c \
	../../../lib/src/file_manager.c                 ../../../lib/src/globals.c \
	../../../lib/src/header.c                       ../../../lib/src/header_parser.c \
	../../../lib/src/header_send.c                  ../../../lib/src/log.c \
	../../../lib/src/linked_list.c                  ../../../lib/src/message_queue.c \
	../../../lib/src/session.c                      ../../../lib/src/server.c \
	../../../lib/src/server_config.c                ../../../lib/src/server_chunk_functions.c \
	../../../lib/src/server_error_pages.c           ../../../lib/src/system.c \
	../../../lib/src/system_file_access.c           ../../../lib/src/system_file_access_fs.c \
	../../../lib/src/system_file_access_wnfs.c      ../../../lib/src/system_file_cache.c \
	../../../lib/src/system_file_access_utils.c     ../../../lib/src/system_file_access_binary.c \
	../../../lib/src/system_sockets.c               ../../../lib/src/system_sockets_container.c \
	../../../lib/src/system_sockets_events.c        ../../../lib/src/system_sockets_select.c \
	../../../lib/src/system_sockets_epoll.c         ../../../lib/src/system_sockets_send.c \
	../../../lib/src/system_sockets_recv.c          ../../../lib/src/system_memory.c \
	../../../lib/src/system_memory_gcc.c            ../../../lib/src/utils.c \
	../../../lib/src/variable.c                     ../../../lib/src/variable_global.c \
	../../../lib/src/variable_render.c              ../../../lib/src/variable_store.c \
	../../../lib/src/webserver_api_functions.c      ../../../lib/src/websocket_handler.c \
	../../../lib/src/websockets.c                   ../../../lib/src/websockets_send_recv.c \
	../../../lib/src/websockets_streaming.c         ../../../lib/src/reverse_proxy.c \
	../../../lib/ssl/openssl.c \
	../../../lib/ssl/mbedtls2.c \
	../../../lib/ssl/mbedtls3.c \
	../../../lib/ssl/wolfssl.c \
	../../../lib/src/hashmap.c \
	../../../lib/platform/linux/platform-file-access.c \
	../../../lib/platform/linux/platform-sockets.c \
	../../../lib/platform/linux/system-unix.c \
	../../../lib/python/py_utils.c \
	../../../lib/python/py_plugin.c \
	../../../lib/python/py_api_functions.c \
	../../../lib/third_party/src/rb_tree/red_black_tree.c \
	../../../lib/third_party/src/rb_tree/stack.c \
	../../../lib/third_party/src/rb_tree/misc.c \
	../../../lib/third_party/src/is_utf8/is_utf8.c \
	../../../lib/third_party/src/miniz/miniz.c
