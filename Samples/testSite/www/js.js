/**
 * libcwebui Test Site JavaScript
 * Vanilla JS - no external dependencies
 */

// File upload via XMLHttpRequest
function upload_file() {
    const fileInput = document.getElementById("form_file");
    if (!fileInput || fileInput.files.length === 0) {
        return;
    }

    const formData = new FormData();
    formData.append(fileInput.value, fileInput.files[0]);

    const xhr = new XMLHttpRequest();
    xhr.open("POST", "/file_upload.html");
    xhr.withCredentials = true;
    xhr.send(formData);
}

// WebSocket connection for commands
let commandSocket = null;

// Create WebSocket with correct protocol
function createWebSocketInstance(handle) {
    const protocol = location.protocol === "https:" ? "wss:" : "ws:";
    const url = `${protocol}//${location.host}/${handle}`;

    try {
        return new WebSocket(url);
    } catch (err) {
        console.error("WebSocket error:", err);
        return null;
    }
}

// Create command socket for clock and ping
function createCommandSocket() {
    const ws = createWebSocketInstance("CommandSocket");
    if (!ws) return null;

    ws.onopen = () => {
        console.log("Connected CommandSocket");
        sendCommand("connect_clock");
    };

    ws.onclose = () => {
        console.log("Closed CommandSocket");
    };

    ws.onmessage = (event) => {
        parseCommand(event.data);
    };

    commandSocket = ws;
    return ws;
}

// Parse messages from server
function parseCommand(text) {
    // Time update
    if (text.startsWith("time:")) {
        const time = text.substring(5);
        const timeEl = document.getElementById("uhr_zeit");
        const timeDivEl = document.getElementById("uhr_zeit_div");
        if (timeEl) timeEl.textContent = time;
        if (timeDivEl) timeDivEl.textContent = time;
        return;
    }

    console.log("recv ->", text);

    // Pong response
    if (text.startsWith("pong")) {
        alert("Pong");
        return;
    }

    // Echo response
    if (text.startsWith("echo:")) {
        const echoEl = document.getElementById("echo_answer");
        if (echoEl) echoEl.value = text.substring(5);
        return;
    }

    console.log("Unknown command:", text);
}

// Send command via WebSocket
function sendCommand(command) {
    if (!commandSocket) return;
    console.log("send ->", command);
    commandSocket.send(command);
}

// Send echo command
function sendEcho() {
    const textEl = document.getElementById("echo_text");
    if (textEl) {
        sendCommand("echo:" + textEl.value);
    }
}

// Create simple WebSocket for buttons
function createSocket(handle, elementId) {
    const ws = createWebSocketInstance(handle);
    if (!ws) return null;

    ws.onopen = () => {
        console.log(`Connected to ${handle} -> element: ${elementId}`);
    };

    ws.onmessage = (event) => {
        if (elementId) {
            const el = document.getElementById(elementId);
            if (el) el.textContent = event.data;
        } else {
            console.log("recv ->", event.data);
        }
    };

    ws.onclose = () => {
        console.log("Closed:", ws.url);
    };

    return ws;
}

// Button click handler for simple websockets
function startSimpleWebsocket(event) {
    const el = event.target;
    createSocket(el.dataset.handle, el.dataset.element);
}

// Initialize on DOM ready
document.addEventListener("DOMContentLoaded", () => {
    // WebSocket buttons
    ["ws_create1", "ws_create2", "ws_create3"].forEach(id => {
        const el = document.getElementById(id);
        if (el) el.onclick = startSimpleWebsocket;
    });

    // Echo button
    const echoBtn = document.getElementById("echo_text_button");
    if (echoBtn) echoBtn.onclick = sendEcho;

    // Ping button
    const pingBtn = document.getElementById("ws_send_ping");
    if (pingBtn) pingBtn.onclick = () => sendCommand("ping");

    // Start command socket
    createCommandSocket();
});
