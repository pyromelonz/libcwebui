/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/


#ifndef _WEBSERVER_H_
#define _WEBSERVER_H_


#define MINIZ_NO_ZLIB_APIS



#include "WebserverConfig.h"
#include "platform-defines.h"



#if defined( LINUX ) || defined( NUTTX )
	#if !defined( USE_LIBEVENT ) && !defined( USE_SELECT ) && !defined( USE_EPOLL )
		#error USE_LIBEVENT or USE_SELECT or USE_EPOLL required
	#endif
	#define EVENT_CHECKED
#endif

#ifndef EVENT_CHECKED
	#if !defined( USE_LIBEVENT ) && !defined( USE_SELECT )
		#error USE_LIBEVENT or USE_SELECT required
	#endif
#endif



#include "red_black_tree.h"

#include "intern/builtinFunctions.h"
#include "intern/base64.h"
#include "intern/convert.h"
#include "intern/cookie.h"
#include "intern/dataTypes.h"
#include "intern/engine.h"
#include "intern/engine_parser.h"
#include "intern/globals.h"
#include "intern/header.h"
#include "intern/helper.h"
#include "intern/linked_list.h"
#include "intern/message_queue.h"
#include "intern/server.h"
#include "intern/server_config.h"
#include "intern/session.h"
#include "intern/sha1.h"
#include "intern/system.h"
#include "intern/system_file_access_binary.h"
#include "intern/system_file_access_utils.h"
#include "intern/system_file_cache.h"
#include "intern/system_sockets.h"
#include "intern/system_sockets_container.h"
#include "intern/system_sockets_events.h"
#include "intern/variable.h"
#include "intern/variable_render.h"
#include "intern/variable_store.h"
#include "intern/variables_globals.h"
#include "intern/webserver_log.h"
#include "intern/webserver_utils.h"
#include "intern/hashmap.h"
#include "intern/websockets.h"

#include "is_utf8.h"
#include "miniz.h"

#include "intern/webserver_ssl.h"

#ifdef WEBSERVER_USE_PYTHON
	#include "intern/py_plugin.h"
#endif


#ifdef USE_LIBEVENT
	#include <event2/event.h>
	#include <event2/thread.h>
#endif

#include "webserver_api_functions.h"


#endif
