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


#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif


ws_MessageQueue* ws_createMessageQueue( void ){
	ws_MessageQueue* mq = (ws_MessageQueue*)WebserverMalloc( sizeof(ws_MessageQueue) );
	PlatformCreateMutex(&mq->lock);
	ws_list_init(&mq->entry_list);
	PlatformCreateSem(&mq->semid, 0);
	return mq;
}

void ws_destroyMessageQueue(ws_MessageQueue* mq){
	if (mq == NULL) return;
	ws_list_destroy(&mq->entry_list);
	PlatformDestroyMutex(&mq->lock);
	PlatformDestroySem(&mq->semid);
	WebserverFree(mq);
}


void *ws_popQueue(ws_MessageQueue* mq){
	void* ret;
	PlatformWaitSem(&mq->semid);
	PlatformLockMutex(&mq->lock);
	ret = ws_list_extract_at(&mq->entry_list,0);
	PlatformUnlockMutex(&mq->lock);
	return ret;
}


void ws_pushQueue(ws_MessageQueue* mq,void* element){
	PlatformLockMutex(&mq->lock);
	ws_list_append(&mq->entry_list,element,0);
	PlatformUnlockMutex(&mq->lock);
	PlatformPostSem(&mq->semid);
}

