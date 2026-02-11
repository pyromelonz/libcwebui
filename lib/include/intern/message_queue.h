/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/



#include "webserver.h"


typedef struct {
	WS_MUTEX lock;
	list_t entry_list;
	WS_SEMAPHORE_TYPE semid;
}ws_MessageQueue;

ws_MessageQueue* ws_createMessageQueue(void);
void ws_destroyMessageQueue(ws_MessageQueue* mq);
void *ws_popQueue(ws_MessageQueue* mq);
void ws_pushQueue(ws_MessageQueue* mq,void* element);
