TEMPLATE_V1

<!-- Site Header -->
<h1 class="site-title">libcwebui</h1>
<p class="site-subtitle">Template Engine Demo</p>

<!-- Memory Stats - shows {f:...} function calls -->
<div class="memory-stats">
    <div class="memory-stats-title">Memory</div>
    {f:memoryInfos}
</div>

<!-- Navigation - shows {if:...} conditions and {get:render;"..."} -->
<nav>

    <div class="nav-section">
        <div class="nav-section-title">General</div>
        <a class="nav-link {get:render;"nav_index"}" href="{b:link}/index.html">Index</a>
        <a class="nav-link {get:render;"nav_memory"}" href="{b:link}/memory_info.html?menu=0">System Info</a>
    </div>

    <div class="nav-section">
        <div class="nav-section-title">Template Features</div>
        <a class="nav-link {get:render;"nav_render"}" href="{b:link}/render_variablen.html">Variables</a>
        <a class="nav-link {get:render;"nav_session"}" href="{b:link}/session_storage.html">Session Storage</a>
        <a class="nav-link {get:render;"nav_if"}" href="{b:link}/if_test.html">Conditions</a>
    </div>

{if:is_ssl_available}
    <div class="nav-section">
        <div class="nav-section-title">SSL</div>
        <a class="nav-link {get:render;"nav_register"}" href="{b:link_ssl}/register.html">Register</a>
        <a class="nav-link {get:render;"nav_unregister"}" href="{b:link_ssl}/unregister.html">Unregister</a>
    </div>
{endif}

{if:is_websockets_available}
    <div class="nav-section">
        <div class="nav-section-title">Interactive</div>
        <a class="nav-link {get:render;"nav_websocket"}" href="{b:link}/websocket.html">WebSocket</a>
        <a class="nav-link {get:render;"nav_ajax"}" href="{b:link}/ajax_test.html">AJAX</a>
    </div>
{endif}

    <div class="nav-section">
        <div class="nav-section-title">Extensions</div>
        <a class="nav-link {get:render;"nav_python"}" href="{b:link}/python.html">Python API</a>
        <a class="nav-link {get:render;"nav_upload"}" href="{b:link}/file_upload.html">File Upload</a>
    </div>

</nav>
