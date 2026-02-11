# libcwebui

[![Build Matrix](https://github.com/lordrasmus/libcwebui/actions/workflows/linux-builds.yml/badge.svg)](https://github.com/lordrasmus/libcwebui/actions/workflows/linux-builds.yml)
[![Unit Tests](https://github.com/lordrasmus/libcwebui/actions/workflows/unit-tests.yml/badge.svg)](https://github.com/lordrasmus/libcwebui/actions/workflows/unit-tests.yml)
![CodeQL](https://github.com/lordrasmus/libcwebui/actions/workflows/github-code-scanning/codeql/badge.svg)

[![Scan Status](https://scan.coverity.com/projects/7148/badge.svg?flat=1)](https://scan.coverity.com/projects/lordrasmus-libcwebui)

![license](https://img.shields.io/badge/license-MPL2-orange.svg)
![language](https://img.shields.io/badge/language-c-blue.svg)

compact webserver library for WebUIs

##Features:
<ul>
<li>    Small ( usable in Embedded Systems )
<li>	Fast
<li>	SSL
<li>	Websocket Support
<li>	Template Engine
<li>    Python Plugin Support
<li>    Portable
</ul>


## Sample

### Files

 *   main.c
 *   plugin.py
 *   www/index.html
 *   img/img.jpg

### main.c

```
int main(int argc, char **argv) {
	
	   if (0 == WebserverInit()) {

       		WebserverConfigSetInt("port",8080);
    		WebserverAddFileDir("", "www");
    		WebserverAddFileDir("img", "img");

            WebserverInitPython();
            WebserverLoadPyPlugin( "plugin.py" );

    		WebserverStart();
    	}
    	return 0;
}

```

### plugin.py

```
import libcwebui

def func1(  ):
    libcwebui.send( "  render b : " )
    libcwebui.setRenderVar( "render1" )

libcwebui.set_plugin_name("PyPlugin")
libcwebui.register_function( func1 )
```

### www/index.html

```
TEMPLATE_V1
Call Function   : {f:func1}
Render Variable : {get:render;"render1"}
```
