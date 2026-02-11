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

#ifdef WEBSERVER_USE_PYTHON

#include "intern/py_utils.h"


void py_print_value ( PyObject* value ){
	//https://docs.python.org/2/c-api/int.html
	//https://docs.python.org/2/c-api/string.html
	//https://docs.python.org/2/c-api/dict.html

	if ( value == 0 ){
		printf("Null Pointer \n");
		return;
	}

	if ( PyString_Check ( value ) ){
		printf("Str  : %s\n", PyString_AsString( value) );
		return;
	}

#if PY_MAJOR_VERSION >= 3
	if( PyInt_Check( value ) ){
		printf("Int  : %ld\n",PyLong_AsLong( value) );
		return;
	}
#else
	if( PyLong_Check( value ) ){
		printf("Int  : %ld\n",PyInt_AsLong( value) );
		return;
	}
#endif

	if( PyDict_Check( value ) ){
		printf("Dict : %zd\n",PyDict_Size( value) );
		PyObject *key, *value_e;
		Py_ssize_t pos = 0;

		while (PyDict_Next( value, &pos, &key, &value_e)) {
			printf("Key : ");py_print_value(key);
			printf("Val : ");py_print_value(value_e);
		}
		return;
	}



	PyObject* type = PyObject_Str( value ) ;
	printf("Unkown Type : %s\n",PyString_AsString( type ) );
}


void py_print_dict_value ( PyObject* dict, char* key ){

	PyObject* value = PyDict_GetItemString( dict, key);

	if( value ){
		py_print_value( value );
	}else{
		printf("%s not found in dict\n",key);
	}
}

const char* py_get_string_param( PyObject *args ){

	PyObject *temp;
	if (! PyArg_ParseTuple(args, "O:set_callback", &temp)){
		PyErr_SetString(PyExc_TypeError, "error parsing parameter");
        return NULL;
	}

	if ( ! PyString_Check ( temp ) ){
		PyErr_SetString(PyExc_TypeError, "argument must ba a string");
        return NULL;
	}

	return PyString_AsString( temp );
}


void py_print_trace( void ){

	PyThreadState *tstate = PyThreadState_GET();

#if PY_VERSION_HEX >= 0x030D0000 // Python 3.13+
	if (NULL != tstate && NULL != PyThreadState_GetFrame(tstate)) {
		PyFrameObject *frame = PyThreadState_GetFrame(tstate);
		const char *filename;
		const char *funcname;
		PyObject* list = PyList_New(0);
		const char* last_func = NULL;

		printf("Traceback (most recent call last):\n");
		while (NULL != frame) {
			char buffer[200];
			PyCodeObject *code = PyFrame_GetCode(frame);
			
			filename = PyUnicode_AsUTF8(code->co_filename);
			funcname = PyUnicode_AsUTF8(code->co_name);

			snprintf(buffer, 200, "  File \"%s\", line %d, in %s\n    %s",
					filename,
					PyFrame_GetLineNumber(frame),
					funcname, last_func ? last_func : "register_function");

			last_func = funcname;
			
			PyList_Append(list, PyUnicode_FromString(buffer));
			
			PyFrameObject *prev_frame = frame;
			frame = PyFrame_GetBack(frame);
			Py_DECREF(prev_frame);
			Py_DECREF(code);
		}

		PyList_Reverse(list);

		for (int i = 0; i < PyList_Size(list); i++) {
			PyObject* item = PyList_GetItem(list, i);
			puts(PyUnicode_AsUTF8(item));
		}
		
		Py_DECREF(list);
	}
#elif PY_VERSION_HEX >= 0x030B0000 // Python 3.11+
	if (NULL != tstate && NULL != PyThreadState_GetFrame(tstate)) {
		PyFrameObject *frame = PyThreadState_GetFrame(tstate);
		const char *filename;
		const char *funcname;
		PyObject* list = PyList_New(0);
		const char* last_func = NULL;

		printf("Traceback (most recent call last):\n");
		while (NULL != frame) {
			char buffer[200];
			PyCodeObject *code = PyFrame_GetCode(frame);
			
			filename = PyUnicode_AsUTF8(code->co_filename);
			funcname = PyUnicode_AsUTF8(code->co_name);

			snprintf(buffer, 200, "  File \"%s\", line %d, in %s\n    %s",
					filename,
					PyFrame_GetLineNumber(frame),
					funcname, last_func ? last_func : "register_function");

			last_func = funcname;
			
			PyList_Append(list, PyUnicode_FromString(buffer));
			
			PyFrameObject *prev_frame = frame;
			frame = PyFrame_GetBack(frame);
			Py_DECREF(prev_frame);
			Py_DECREF(code);
		}

		PyList_Reverse(list);

		for (int i = 0; i < PyList_Size(list); i++) {
			PyObject* item = PyList_GetItem(list, i);
			puts(PyUnicode_AsUTF8(item));
		}
		
		Py_DECREF(list);
	}
#else // Python < 3.11
	if (NULL != tstate && NULL != tstate->frame) {
		PyFrameObject *frame = tstate->frame;
		const char *filename;
		const char *funcname;
		PyObject* list = PyList_New(0);
		const char* last_func = NULL;

		printf("Traceback (most recent call last):\n");
		while (NULL != frame) {
			char buffer[200];

			filename = PyString_AsString(frame->f_code->co_filename);
			funcname = PyString_AsString(frame->f_code->co_name);

			snprintf(buffer, 200, "  File \"%s\", line %d, in %s\n    %s",
					filename,
					PyFrame_GetLineNumber(frame),
					funcname, last_func ? last_func : "register_function");

			last_func = funcname;

			PyList_Append(list, PyString_FromString(buffer));

			frame = frame->f_back;
		}

		PyList_Reverse(list);

		for (int i = 0; i < PyList_Size(list); i++) {
			PyObject* item = PyList_GetItem(list, i);
			puts(PyString_AsString(item));
		}
	}
#endif
}

PyObject *py_ws_var_to_py( ws_variable *var ){

	if ( var == 0 ) return NULL;

	switch ( var->type ){
		case VAR_TYPE_EMPTY:
			Py_RETURN_NONE;
		case VAR_TYPE_STRING:
			return PyString_FromString( var->val.value_string );
		case VAR_TYPE_INT:
			return PyInt_FromLong( var->val.value_int );
		case VAR_TYPE_ULONG:
			return PyLong_FromUnsignedLongLong( var->val.value_uint64_t );
		case VAR_TYPE_LONG:
			return PyLong_FromLongLong( var->val.value_int64_t );
		case VAR_TYPE_REF:
			return py_ws_var_to_py( var->val.value_ref );
		case VAR_TYPE_ARRAY:
			printf("py_ws_var_to_py: VAR_TYPE_ARRAY not convertible\n");
			Py_RETURN_NONE;
		case VAR_TYPE_CUSTOM_DATA:
			printf("py_ws_var_to_py: VAR_TYPE_CUSTOM_DATA not convertible\n");
			Py_RETURN_NONE;
	}

	return NULL;
}

int py_obj_is_convertable( PyObject *value){

	if( PyString_Check ( value ) ){ return 1; }
	if( PyInt_Check    ( value ) ){ return 1; }

	return 0;
}

void py_to_ws_var( ws_variable* var, PyObject *value){

	if( PyString_Check ( value ) ){ setWSVariableString( var, PyString_AsString( value ) ); }
	if( PyInt_Check    ( value ) ){ setWSVariableInt(    var,    PyInt_AsLong  ( value ) ); }

}

#endif


#if 0

PyObject *iterator = PyObject_GetIter(arg);
	PyObject *item;

	if (iterator != NULL) {

		printf("Iterrating\n");

		while (item = PyIter_Next(iterator)) {
			/* do something with item */
			printf("Item : %s\n", PyString_AsString(  PyObject_Str( item ) ) ) ;
			/* release reference when done */
			Py_DECREF(item);
		}

		Py_DECREF(iterator);

		if (PyErr_Occurred()) {
			/* propagate error */
		}
		else {
			/* continue doing useful work */
		}
	}

#endif
