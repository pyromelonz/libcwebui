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


struct template_tag {
    unsigned int id;
    const char* tag;
    unsigned int tag_size;
};

#define IF_TAG_TEXT "{if:"
#define ELSE_TAG_TEXT "{else}"
#define ENDIF_TAG_TEXT "{endif}"

#define MAKE_TAG_INFO( a ) { a , a##_TEXT, sizeof(a##_TEXT ) -1 }

struct template_tag if_tags[3]={
    MAKE_TAG_INFO ( IF_TAG ),
    MAKE_TAG_INFO ( ELSE_TAG ),
    MAKE_TAG_INFO ( ENDIF_TAG )
};

#define LOOP_TAG_TEXT "{loop:"
#define ENDLOOP_TAG_TEXT "{endloop}"

struct template_tag loop_tags[2]={
    MAKE_TAG_INFO ( LOOP_TAG ),
    MAKE_TAG_INFO ( ENDLOOP_TAG )
};


int getNextTag ( const char* pagedata,const int datalength, tag_groups search_tag_group,TAG_IDS *tag ) {
    int i,i2;
    int tags_in_group;
    int pos=0;
    const char *data = pagedata;
    struct template_tag *search_tags = 0;
    struct template_tag *tag_info =0;

    switch ( search_tag_group ) {
        case IF_TAG_GROUP:
            search_tags = if_tags;
            tags_in_group = sizeof ( if_tags );
            break;

        case LOOP_TAG_GROUP:
            search_tags = loop_tags;
            tags_in_group = sizeof ( loop_tags );
            break;
    }

    if ( search_tags == 0 ) {
        return -1;
    }

    tags_in_group /= sizeof ( struct template_tag );

    for ( i=0;i<datalength;i++ ) {
        data = &pagedata[i];
        if ( data[0] != '{' ){
            continue;
        }
        for ( i2=0;i2<tags_in_group;i2++ ) {
            tag_info = &search_tags[i2];

            pos = strncmp ( data,tag_info->tag,tag_info->tag_size );
            if ( 0 == pos ) {
                *tag = (TAG_IDS)tag_info->id;
                return i+tag_info->tag_size;
            }
        }

    }

    return -1;
}


int find_tag_end_pos ( const char *pagedata,int datalenght,const char *start_tag,const char *end_tag ) {
  int pos1=0,pos2=0,level=1,offset=0;

  /*
  char bb1[10000];
  memcpy(bb1,pagedata,datalenght);
  bb1[datalenght]='\0';
  bb1[datalenght]='\0';
  */

  do {
    pos1 = stringnfind ( ( char* ) pagedata+offset,end_tag,datalenght-offset );
    if ( pos1 <= 0 ) {
      LOG ( TEMPLATE_LOG,NOTICE_LEVEL,0,"Endtag < %s > not found",end_tag );
      return -1;
    }
    level--;

    pos2 = stringnfind ( ( char* ) pagedata+offset,start_tag,pos1 );
    if ( pos2>0 ){
      level++;
    }

    /* Overflow check: ensure offset + pos1 stays within bounds and doesn't overflow */
    if ( offset > datalenght - pos1 ) {
      LOG ( TEMPLATE_LOG,ERROR_LEVEL,0,"%s","Tag parsing overflow detected" );
      return -1;
    }
    offset += pos1;
  } while ( level > 0 );

  /* offset <= datalenght is guaranteed, check offset+1 won't overflow */
  if ( offset > INT_MAX - 1 ) {
    return -1;
  }
  return offset + 1;

}
