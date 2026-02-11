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


ws_variable* NEED_RESUL_CHECK parseVariable(http_request *s,char* buffer) {
	int i;
	int l;
	int name_space = 0;
	int offset = 0;
	char error_buffer[130];
	char error_index[100];

	ws_variable *var,*tmp,*tmp2;

	if(buffer == 0){
		return 0;
	}

	char* buffer_end = buffer + strlen( buffer );

#ifdef _WEBSERVER_TEMPLATE_DEBUG_ 
	LOG (TEMPLATE_LOG,NOTICE_LEVEL,0,"parseVariable string %s",buffer);
#endif

	if(0 == strncmp(buffer,"global;",7)){
#ifdef _WEBSERVER_TEMPLATE_DEBUG_
	LOG (TEMPLATE_LOG,NOTICE_LEVEL,0,"parseVariable Namespace global");
#endif
		name_space = 1;
		offset=7;
	}
	if(0 == strncmp(buffer,"render;",7)){
#ifdef _WEBSERVER_TEMPLATE_DEBUG_
	LOG (TEMPLATE_LOG,NOTICE_LEVEL,0,"parseVariable Namespace render");
#endif
		name_space = 2;
		offset=7;
	}
	if(0 == strncmp(buffer,"session;",8)){
#ifdef _WEBSERVER_TEMPLATE_DEBUG_
	LOG (TEMPLATE_LOG,NOTICE_LEVEL,0,"parseVariable Namespace session");
#endif
		name_space = 3;
		offset=8;
	}
	if(0 == strncmp(buffer,"parameter;",10)){
#ifdef _WEBSERVER_TEMPLATE_DEBUG_
	LOG (TEMPLATE_LOG,NOTICE_LEVEL,0,"parseVariable Namespace parameter");
#endif
		name_space = 4;
		offset=10;
	}

	
	var = newWSVariable("tmp");

	l = strlen(buffer);
#ifdef _WEBSERVER_TEMPLATE_DEBUG_ 
		LOG (TEMPLATE_LOG,NOTICE_LEVEL,0,"parseVariable Name %s",&buffer[offset]);
#endif

	if(0 == strncmp(&buffer[offset],"\"",1)){
		offset++;
		for(i=offset;i<l;i++){
			if(buffer[i] == '\"'){
				break;
			}
		}
		buffer[i] = '\0';
		buffer = &buffer[offset];
#ifdef _WEBSERVER_TEMPLATE_DEBUG_ 
		LOG (TEMPLATE_LOG,NOTICE_LEVEL,0,"parseVariable Name %s",buffer);
#endif
		tmp = 0;
		switch(name_space){
			case 0 :	setWSVariableString(var,buffer);
						return var;
			case 1 :	tmp = getGlobalVariable(buffer); 
						break;
			case 2 :	tmp = getRenderVariable(s,buffer);
						if(tmp->type == VAR_TYPE_EMPTY){
							WebserverFree(var->name);
							var->name_len=strlen(buffer);
							var->name = (char*)WebserverMalloc( var->name_len + 1 );
							strcpy(var->name,buffer);
							tmp = 0;
						}
						break;
			case 3 :	tmp = getSessionValue(s,SESSION_STORE, buffer); 
						break;
			case 4:		tmp = getParameter(s,buffer);
						break;
		}
		if( tmp == 0){
			switch(name_space){				
				case 1 : snprintf(error_buffer,100,"GLOBAL VAR %s not found",buffer);	break;
				case 2 : snprintf(error_buffer,100,"RENDER VAR %s not found",buffer);	break;
				case 3 : snprintf(error_buffer,100,"SESSION VAR %s not found",buffer);	break;
				case 4 : snprintf(error_buffer,100,"PARAMETER VAR %s not found",buffer);	break;
				default:
					snprintf(error_buffer,100,"UNKNOWN NameSpace VAR %s not found",buffer);	break;
					
			}
			setWSVariableString(var,error_buffer);
			return var;
		}else{
			if(tmp->type == VAR_TYPE_REF){
			    tmp = tmp->val.value_ref;
			}
			if(tmp->type == VAR_TYPE_ARRAY){
				i=strlen(buffer)+1;
				buffer = &buffer[i];
				while(1){
				    tmp2 = 0;
				    if(buffer[0]=='['){
					l = strlen(buffer);
					for(i=0;i<l;i++){
					    if(buffer[i]==']'){
						break;
					    }
					}
					buffer[i] = '\0';
					buffer = &buffer[1];
					if(buffer[0] == '\"'){		/* String Index */
					    buffer[i-2] = '\0';
					    buffer = &buffer[1];
					    tmp2 = getWSVariableArray(tmp,buffer);
					    if( tmp2 == 0 ){
						strcpy(error_index,buffer);
					    }
					}else{						/* Int Index */
					    i = atoi(buffer);
					    tmp2 = getWSVariableArrayIndex(tmp,i);
					    if( tmp2 == 0 ){
						strcpy(error_index,buffer);
					    }
					}
				    }else{
					setWSVariableRef(var,tmp);
					return var;
				    }

				    /* bei verschachtelte arrays nochmal das array element suchen
				     * aber nur wenn der original buffer noch nicht zuende ist */
#ifdef ENABLE_DEVEL_WARNINGS				     
				    #warning verhalten bei verschachtelten arrays nochmal prüfen

#endif				    
				    /* bei dem vorherigen parsen wurde das ende des index [ 
				     * durch \0 ersetzt , darum strlen(buffer)+2 */
				    char * next_index = &buffer[strlen(buffer)+2];
				    if ( ( tmp2 != 0 ) && ( next_index < buffer_end ) ) {
					buffer = next_index;
					if( ( buffer[0] == '[' ) && (tmp2->type == VAR_TYPE_ARRAY) ){
					    tmp = tmp2;
					    continue;
					}
				    }
				    break;
				}
                

				if(tmp2 == 0){
					snprintf(error_buffer,sizeof(error_buffer),"ARRAY %s INDEX %s not found",tmp->name,error_index);
					setWSVariableString(var,error_buffer);
					return var;
				}else{
					setWSVariableRef(var,tmp2);
					return var;
				}
			}
			setWSVariableRef(var,tmp);
		}		
	}else{
		i = atoi(buffer);	
		setWSVariableInt(var,i);
	}
	return var;
}
