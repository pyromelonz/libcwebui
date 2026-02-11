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


#include <libgen.h>

#include "intern/py_plugin.h"
#include "intern/py_utils.h"
#include "intern/py_api_functions.h"
#include "intern/engine.h"

#include "intern/py_plugin.h"


/*
 http://python3porting.com/cextensions.html
 https://docs.python.org/2/extending/extending.html
 https://mathieu.fenniak.net/embedding-python-tips/
 https://docs.python.org/2/c-api/capsule.html#capsules
 https://docs.python.org/2/reference/datamodel.html
 https://docs.python.org/2/c-api/
 http://www.tutorialspoint.com/python/python_further_extensions.htm
*/


extern list_t plugin_liste;
extern plugin_s *current_plugin;


static struct web_py_plugin *context= NULL;
static char* plugin_name = NULL;
static char* plugin_name_buff = NULL;
http_request *py_cur_s = NULL;


static const char* last_py_plugin_path;
static struct web_py_plugin *last_py_plugin;

static int reload_py_plugin=-1;


PyObject* py_register_function( PyObject* self, PyObject *args )
{
	//PyObject *result = NULL;
	PyObject *func_obj = NULL;
    PyObject *temp;
    const char* func_name;

    if ( plugin_name == NULL ){
		PyErr_SetString(PyExc_TypeError, "plugin name must be set first with set_plugin_name");
        return NULL;
	}

    if (! PyArg_ParseTuple(args, "O:set_callback", &temp)){
		PyErr_SetString(PyExc_TypeError, "error parsing parameter");
        return NULL;
	}

	if (!PyCallable_Check(temp)) {
		PyErr_SetString(PyExc_TypeError, "parameter must be callable");
		return NULL;
	}


	const char *filename;
	int fileline;


	Py_XINCREF(temp);         /* Add a reference to new callback */


	func_obj =  PyObject_GenericGetAttr( temp, PyString_FromString( "__code__" ) );
	filename = PyString_AsString( PyObject_GenericGetAttr( func_obj, PyString_FromString( "co_filename" ) ) );
	fileline = PyInt_AsLong( PyObject_GenericGetAttr( func_obj, PyString_FromString( "co_firstlineno" ) ) );

    func_obj =  PyObject_GenericGetAttr( temp, PyString_FromString( "__name__" ) );
	func_name = PyString_AsString( func_obj );


	printf("register python function <%s> \n",func_name);

	if ( context == 0 ){
		py_print_trace();

		PyErr_SetString(PyExc_TypeError, "libcwebuiError: no Context, skipping python function register\n");
		Py_RETURN_NONE;
	}

	register_py_function(func_name, temp, filename, fileline);

	Py_RETURN_NONE;
}


PyObject* py_set_plugin_name( PyObject* self, PyObject *args )
{
	if ( context == NULL ){
		PyErr_SetString(PyExc_TypeError, "internal server error:  set_plugin_name : context == NULL\n");
		return NULL;
	}

	if ( plugin_name_buff == NULL ){
		PyErr_SetString(PyExc_TypeError, "internal server error:  set_plugin_name : plugin_name_buff == NULL\n");
		return NULL;
	}

	const char* param = py_get_string_param( args );

	plugin_name = plugin_name_buff;
	strncpy(plugin_name_buff, param , 59);

	printf("python plugin name set to <%s>\n",plugin_name);

	plugin_s *plugin, *plugin_tmp;

	/* Pruefen on Plugin schon geladen wurde */
	ws_list_iterator_start(&plugin_liste);
	while ((plugin_tmp = (plugin_s*) ws_list_iterator_next(&plugin_liste))) {
		if (0 == strcmp(plugin_tmp->name, plugin_name)) {
			//PyErr_SetString(PyExc_TypeError,"Plugin Name already exists\n");
			printf("Python Plugin Name exists\n");
			ws_list_iterator_stop(&plugin_liste);
			current_plugin = plugin_tmp;
			Py_RETURN_NONE;
		}
	}
	ws_list_iterator_stop(&plugin_liste);

	/* Plugin Struktur erzeugen */
	plugin = (plugin_s*) WebserverMalloc( sizeof(plugin_s) );
	plugin->name = (char*) WebserverMalloc( strlen(plugin_name) + 1 );
	strcpy(plugin->name, plugin_name);
	plugin->path = (char*) WebserverMalloc( strlen(last_py_plugin_path) + 1 );
	strcpy(plugin->path, last_py_plugin_path);
	plugin->type = 1; // Python

	ws_list_append(&plugin_liste, plugin, 0);

	current_plugin = plugin;

	plugin->error = (char*) WebserverMalloc( strlen("loaded") +1 );
	strcpy(plugin->error,"loaded");

	plugin->py_plugin = last_py_plugin;

	Py_RETURN_NONE;
}

int py_load_python_plugin( const char* path ){

	/* Open file first to avoid TOCTOU race condition (no separate access() check) */
	FILE* fp = fopen(path, "r");
	if ( !fp ){
		printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
		printf("loading python plugin : %s  not found \n",path);
		printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
		return 0;
	}

	last_py_plugin_path = path;

	struct web_py_plugin * mod = malloc( sizeof( struct web_py_plugin ) );

	last_py_plugin = mod;


	mod->thread_state = Py_NewInterpreter();
	mod->global_namespace = PyModule_GetDict( PyImport_AddModule("__main__") );
	
	//mod->local_namespace  = PyDict_New();

	char cwd[PATH_MAX];
	getcwd(cwd, sizeof(cwd));

	char exec_path[PATH_MAX*2];
	snprintf( exec_path, sizeof( exec_path ) , "%s/%s", cwd, path );
	dirname( exec_path );

	mod->exec_path = strdup( exec_path );
	mod->path = strdup( path );

	plugin_name = NULL;
	plugin_name_buff = mod->plugin_name;

	WebserverFileInfo file;
	memset(&file,0,sizeof( WebserverFileInfo ));

	int time_changed;
	int new_size;

	file.FilePath = path;
	PlatformGetFileInfo( &file, &time_changed, &new_size);
	WebserverFree(file.lastmodified);


	mod->last_mod_sec = file.last_mod_sec;
	mod->last_mod_nsec = file.last_mod_nsec;
	mod->size = new_size;

	printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
	printf("loading python plugin : %s  start \n",path);
	printf("size : %d\n",mod->size);
	printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");

	context = mod;

	char run[1000];
	sprintf( run, "import sys\n" "sys.path.append('%s')\n", mod->exec_path );

	PyRun_SimpleString( run );
	PyRun_File( fp, path, Py_file_input, mod->global_namespace, mod->global_namespace );

	context = NULL;

	fclose(fp);

	PyErr_Print();

	current_plugin = 0;


	printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
	printf("loading python plugin : %s finished\n",path);
	printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");

	return 0;

}



void py_call_engine_function( http_request *s, user_func_s *func , FUNCTION_PARAS* paras ){
	PyObject *result;

	if ( reload_py_plugin == -1 ){
		reload_py_plugin = getConfigInt("reload_py_modules");
	}

	if ( 1 == reload_py_plugin ){

		WebserverFileInfo file;
		int time_changed;
		int new_size;

		file.lastmodified = 0;
		file.FilePath = func->plugin->path;
		file.last_mod_sec = func->plugin->py_plugin->last_mod_sec;
		file.last_mod_nsec = func->plugin->py_plugin->last_mod_nsec;
		PlatformGetFileInfo( &file, &time_changed, &new_size);
		WebserverFree(file.lastmodified);

		if ( ( file.last_mod_sec != func->plugin->py_plugin->last_mod_sec ) ||
			 ( file.last_mod_nsec != func->plugin->py_plugin->last_mod_nsec ) ||
			 ( new_size != func->plugin->py_plugin->size) ){

			printf("reload Python Plugin: %s\n",func->plugin->path);

			func->plugin->py_plugin->last_mod_sec = file.last_mod_sec;
			func->plugin->py_plugin->last_mod_nsec = file.last_mod_nsec;
			func->plugin->py_plugin->size = new_size;


			//py_load_python_plugin( func->plugin->path );
			FILE* fp = fopen(func->plugin->path,"r");
			if ( fp ){

				context = func->plugin->py_plugin;

				/*
				 * TODO: Memory Leak beim Plugin-Reload
				 *
				 * Problem: Neuer Namespace wird geholt, alter geht verloren.
				 * - PyModule_GetDict() gibt borrowed reference zurück (kein DECREF möglich)
				 * - Alter Namespace enthält noch Funktionsobjekte
				 * - Engine hält noch func->py_func Referenzen auf alte Funktionen
				 *
				 * Warum __main__ Namespace statt PyDict_New()?
				 * Mit leerem Dict funktioniert "import" nicht - __builtins__ fehlt.
				 * __main__ hat alles was Python braucht, daher wurde das verwendet.
				 *
				 * Warum neuer Namespace beim Reload? Um alte Funktionen zu entfernen
				 * die im neuen Code nicht mehr existieren (sauberer Zustand).
				 *
				 * Richtige Lösung:
				 * 1. unregister_plugin_functions(plugin) - alle Funktionen des Plugins entfernen
				 * 2. PyDict_New() - eigenen Namespace erstellen (owned, nicht borrowed)
				 * 3. Namespace initialisieren (testen was wirklich gebraucht wird):
				 *    - __builtins__: PyEval_GetBuiltins() - KRITISCH für import/print/etc
				 *    - __name__: "__main__" - für "if __name__ == '__main__'" checks
				 *    - __file__: Pfad zum Script - falls Script seinen Pfad braucht
				 *    - __package__: NULL/None - für relative imports
				 * 4. PyRun_File() mit neuem Namespace
				 * 5. Py_XDECREF(old_namespace) - jetzt sicher freigeben
				 *
				 * Einfache Alternative: Bestehenden Namespace wiederverwenden statt
				 * neuen holen. Dann überschreiben neue Definitionen die alten,
				 * aber alte Funktionen bleiben erhalten.
				 */
				#warning mem leak wegen neuem namespace - siehe TODO Kommentar

				context->global_namespace = PyModule_GetDict( PyImport_AddModule("__main__") );

				PyRun_File( fp,func->plugin->path , Py_file_input, context->global_namespace,  context->global_namespace );

				PyErr_Print();

				context = NULL;

				fclose(fp);
			}
		}
	}


	PyObject *function = func->py_func;
	py_cur_s = s;

	result = PyObject_CallObject(function, NULL);
	if ( result == NULL){
		printf("py_call_engine_function: Python Error\n");
	}
	Py_XDECREF(result);
	PyErr_Print();

	py_cur_s = 0;
}

/*
void py_call_function( char* name ){

	int arg = 22;
	PyObject *arglist;
	PyObject *result;

	PyObject *my_callback = PyDict_GetItem( func_dict, PyString_FromString( name ) );

	//PyGILState_STATE gstate;
	//gstate = PyGILState_Ensure();

	arglist = Py_BuildValue("(i)", arg);
	result = PyObject_CallObject(my_callback, arglist);
	Py_XDECREF(arglist);
	Py_XDECREF(result);
	PyErr_Print();


	// so müsste man an die locals kommen
	// PyObject_Dir( my_callback )


	//PyGILState_Release(gstate);

}*/

extern PyMethodDef py_libcwebui_methods[];

#if PY_MAJOR_VERSION >= 3
struct PyModuleDef py_libcwebui_module = {
	PyModuleDef_HEAD_INIT,
	"hello",
	NULL,
	-1,
	py_libcwebui_methods,
	NULL, NULL, NULL, NULL
};
#endif

static void set_prog_name( void ){
	#if PY_VERSION_HEX >= 0x030B0000 // Python 3.11+
		PyConfig config;
		PyConfig_InitPythonConfig(&config);
		PyConfig_SetString(&config, &config.program_name, L"libcwebui");
		Py_InitializeFromConfig(&config);
		PyConfig_Clear(&config);
	#elif PY_MAJOR_VERSION >= 3
		Py_SetProgramName((wchar_t*)"libcwebui");
	#else
		Py_SetProgramName("libcwebui");
	#endif
}

static PyMODINIT_FUNC py_init(void){
	printf("libcwebui py_init\n");

	set_prog_name();

	#if PY_MAJOR_VERSION >= 3
		return PyModule_Create(&py_libcwebui_module);
	#else
		Py_InitModule3("libcwebui", py_libcwebui_methods, "This is a module");
	#endif

   // func_dict = PyDict_New();
}

int py_init_modules( void ){

	if (PyImport_AppendInittab( "libcwebui", py_init) == -1){
		printf("Failed to add libcwebui to the interpreter's builtin modules");
		PyErr_Print();
		return 1;
    }

#if PY_VERSION_HEX >= 0x030B0000
	// Python 3.11+: set_prog_name() uses PyConfig and initializes Python
	set_prog_name();
#elif PY_MAJOR_VERSION >= 3
	// Python 3.0 - 3.10
	Py_SetProgramName(L"libcwebui");
	Py_Initialize();
#else
	// Python 2.x
	Py_SetProgramName("libcwebui");
	Py_Initialize();
#endif
	
	//PyEval_InitThreads();

	/*load_python_plugin("test.py");
	load_python_plugin("test2.py");


	call_py_function( "test" );
	call_py_function( "test2" );
	call_py_function( "test3" );
	call_py_function( "test" );
*/


	return 0;


}



#endif
