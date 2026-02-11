TEMPLATE_V1

<h2>Test Functions</h2>
<p class="text-small text-muted">These functions modify session variables to demonstrate the session system.</p>

<div class="info-box">
    <div class="info-row">
        <span class="info-label">Session ID</span>
        <span class="info-value mono">{get:session;"session-id"}</span>
    </div>
    <div class="info-row">
        <span class="info-label">test2</span>
        <span class="info-value">{get:session;"test2"}</span>
    </div>
</div>

<h3>Function Calls</h3>
<table>
    <tr><th>Function</th><th>Output</th></tr>
    <tr>
        <td class="mono">&#123;f:testfunction1&#125;</td>
        <td>{f:testfunction1}</td>
    </tr>
    <tr>
        <td class="mono">&#123;f:testfunction2&#125;</td>
        <td>{f:testfunction2}</td>
    </tr>
    <tr>
        <td class="mono">&#123;f:testfunction3&#125;</td>
        <td>{f:testfunction3}</td>
    </tr>
</table>

<h3>Session Variables After Function Calls</h3>
<table>
    <tr><th>Variable</th><th>Value</th></tr>
    <tr>
        <td class="mono">test1</td>
        <td>{get:session;"test1"}</td>
    </tr>
    <tr>
        <td class="mono">test2</td>
        <td>{get:session;"test2"}</td>
    </tr>
    <tr>
        <td class="mono">test3</td>
        <td>{get:session;"test3"}</td>
    </tr>
</table>
