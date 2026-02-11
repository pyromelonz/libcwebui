TEMPLATE_V1
<span class="text-muted">
    {f:server_name} &middot;
    {f:compiler} &middot;
    {f:build_time} &middot;
    {f:host}:{f:server_port}{if:ssl_supported}/{f:server_ssl_port}{endif}
</span>
