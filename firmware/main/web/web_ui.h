#pragma once

namespace Web {

// Bundled HTML5/CSS3/JS ChatGPT Dark Mode Web UI
// Stored in Flash DROM (Zero SRAM allocation)
static const char CHAT_HTML[] = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32-S3 Micro-LLM Generative AI</title>
    <style>
* { 
    box-sizing: border-box; 
    margin: 0; 
    padding: 0; 
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; 
}

body { 
    background-color: #131314; 
    color: #e3e3e3; 
    display: flex; 
    flex-direction: column; 
    height: 100vh; 
    overflow: hidden; 
}

/* Header */
header { 
    background: #1e1f20; 
    padding: 12px 16px; 
    display: flex; 
    align-items: center; 
    justify-content: space-between; 
    border-bottom: 1px solid #333538; 
}

.logo { 
    display: flex; 
    align-items: center; 
    gap: 10px; 
    font-weight: 600; 
    font-size: 1.1rem; 
    color: #fff; 
}

.badge { 
    background: #005a36; 
    color: #4ee09b; 
    font-size: 0.72rem; 
    padding: 3px 8px; 
    border-radius: 12px; 
    font-weight: 500; 
}

.stats-pill { 
    font-size: 0.78rem; 
    color: #aaa; 
    background: #282a2d; 
    padding: 4px 10px; 
    border-radius: 8px; 
}

/* Chat Box */
#chat-container { 
    flex: 1; 
    overflow-y: auto; 
    padding: 16px; 
    display: flex; 
    flex-direction: column; 
    gap: 14px; 
}

.msg { 
    display: flex; 
    flex-direction: column; 
    max-width: 85%; 
    animation: fadeIn 0.2s ease-in-out; 
}

.msg.user { align-self: flex-end; }
.msg.bot { align-self: flex-start; }

.bubble { 
    padding: 12px 16px; 
    border-radius: 18px; 
    line-height: 1.5; 
    font-size: 0.95rem; 
    word-break: break-word; 
    white-space: pre-line; 
}

.msg.user .bubble { 
    background: #2b5c8f; 
    color: #fff; 
    border-bottom-right-radius: 4px; 
}

.msg.bot .bubble { 
    background: #1e1f20; 
    color: #e3e3e3; 
    border: 1px solid #333538; 
    border-bottom-left-radius: 4px; 
}

.meta { 
    font-size: 0.72rem; 
    color: #888; 
    margin-top: 4px; 
    display: flex; 
    gap: 8px; 
}

.msg.user .meta { justify-content: flex-end; }
.meta span { 
    background: #282a2d; 
    padding: 2px 6px; 
    border-radius: 4px; 
}

/* Quick Prompts */
.quick-chips { 
    display: flex; 
    gap: 8px; 
    overflow-x: auto; 
    padding: 8px 16px; 
    scrollbar-width: none; 
    background: #131314; 
}

.quick-chips::-webkit-scrollbar { display: none; }

.chip { 
    background: #1e1f20; 
    border: 1px solid #333538; 
    color: #c4c7c5; 
    padding: 6px 12px; 
    border-radius: 16px; 
    font-size: 0.82rem; 
    white-space: nowrap; 
    cursor: pointer; 
    transition: 0.15s; 
}

.chip:hover { 
    background: #2b2c2f; 
    border-color: #555; 
}

/* Input Bar */
.input-bar { 
    background: #1e1f20; 
    padding: 12px 16px; 
    display: flex; 
    gap: 10px; 
    border-top: 1px solid #333538; 
}

input[type="text"] { 
    flex: 1; 
    background: #131314; 
    border: 1px solid #444746; 
    color: #fff; 
    padding: 12px 16px; 
    border-radius: 24px; 
    font-size: 0.95rem; 
    outline: none; 
    transition: 0.2s; 
}

input[type="text"]:focus { border-color: #388bfd; }

button { 
    background: #388bfd; 
    color: #fff; 
    border: none; 
    border-radius: 50%; 
    width: 44px; 
    height: 44px; 
    display: flex; 
    align-items: center; 
    justify-content: center; 
    cursor: pointer; 
    transition: 0.2s; 
}

button:hover { background: #1d70e2; }
button:disabled { background: #444; cursor: not-allowed; }

@keyframes fadeIn { 
    from { opacity: 0; transform: translateY(6px); } 
    to { opacity: 1; transform: translateY(0); } 
}

.cursor {
    display: inline-block;
    width: 8px;
    height: 15px;
    background: #388bfd;
    vertical-align: -2px;
    margin-left: 2px;
    border-radius: 2px;
    animation: blink 0.7s infinite;
}

@keyframes blink {
    0%, 100% { opacity: 1; }
    50% { opacity: 0; }
}


</style>
</head>
<body>
    <header>
        <div class="logo">
            <span>⚡ JARVIS (ESP32-S3)</span>
            <span class="badge">100% On-Device AI</span>
        </div>
        <div class="stats-pill" id="chip-status">Telemetry Loading...</div>
    </header>

    <div id="chat-container">
        <div class="msg bot">
            <div class="bubble">Greetings sir! I am JARVIS, an autoregressive language model running 100% on-device on the ESP32-S3 microcontroller. How may I assist you today? 🤖</div>
            <div class="meta"><span>Micro-Transformer INT8</span><span>Dual-Core 240MHz</span></div>
        </div>
    </div>

    <div class="quick-chips">
        <div class="chip" onclick="sendPrompt('Hello Jarvis')">👋 Hello Jarvis</div>
        <div class="chip" onclick="sendPrompt('Tell me a story')">📖 Tell me a story</div>
        <div class="chip" onclick="sendPrompt('Tell me a joke')">🎉 Tell me a joke</div>
        <div class="chip" onclick="sendPrompt('How are you')">⚡ How are you</div>
        <div class="chip" onclick="sendPrompt('Who are you')">🤖 Who are you</div>
        <div class="chip" onclick="sendPrompt('System status')">📊 System Status</div>
    </div>

    <div class="input-bar">
        <input type="text" id="msg-input" placeholder="Type prompt for AI generation..." autocomplete="off" onkeypress="if(event.key==='Enter') sendMessage()">
        <button id="send-btn" onclick="sendMessage()">
            <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                <line x1="22" y1="2" x2="11" y2="13"></line>
                <polygon points="22 2 15 22 11 13 2 9 22 2"></polygon>
            </svg>
        </button>
    </div>

    <script>
const chatContainer = document.getElementById('chat-container');
const msgInput = document.getElementById('msg-input');
const sendBtn = document.getElementById('send-btn');
const chipStatus = document.getElementById('chip-status');

function appendMessage(sender, text, metaInfo) {
    const msgDiv = document.createElement('div');
    msgDiv.className = 'msg ' + sender;
    
    let metaHtml = '';
    if (metaInfo) {
        metaHtml = `<div class="meta">${metaInfo}</div>`;
    }
    
    msgDiv.innerHTML = `<div class="bubble">${text}</div>${metaHtml}`;
    chatContainer.appendChild(msgDiv);
    chatContainer.scrollTop = chatContainer.scrollHeight;
    return msgDiv;
}

function escapeHtml(str) {
    return str.replace(/&/g, "&amp;")
              .replace(/</g, "&lt;")
              .replace(/>/g, "&gt;")
              .replace(/"/g, "&quot;")
              .replace(/'/g, "&#039;")
              .replace(/\n/g, "<br>");
}

function streamBotReply(botMsgDiv, fullText, metaInfo, tokSec) {
    const bubble = botMsgDiv.querySelector('.bubble');
    bubble.innerHTML = '<span class="cursor"></span>';
    
    let currentIdx = 0;
    // Cadence calculation based on measured tok/s
    const charDelay = Math.max(14, Math.min(35, Math.floor(1000 / (tokSec * 3.5 || 50))));
    
    const timer = setInterval(() => {
        if (currentIdx < fullText.length) {
            currentIdx++;
            bubble.innerHTML = escapeHtml(fullText.substring(0, currentIdx)) + '<span class="cursor"></span>';
            chatContainer.scrollTop = chatContainer.scrollHeight;
        } else {
            clearInterval(timer);
            bubble.innerHTML = escapeHtml(fullText);
            if (metaInfo) {
                const metaDiv = document.createElement('div');
                metaDiv.className = 'meta';
                metaDiv.innerHTML = metaInfo;
                botMsgDiv.appendChild(metaDiv);
            }
            chatContainer.scrollTop = chatContainer.scrollHeight;
        }
    }, charDelay);
}

async function sendMessage() {
    const text = msgInput.value.trim();
    if (!text) return;
    
    msgInput.value = '';
    appendMessage('user', escapeHtml(text), '<span>User Prompt</span>');
    sendBtn.disabled = true;

    // Create bot bubble with blinking cursor
    const botMsg = appendMessage('bot', '<span class="cursor"></span>');

    try {
        const response = await fetch('/api/chat', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ message: text })
        });
        
        const data = await response.json();
        const meta = `<span>⚡ ${(data.tokens_sec || 20).toFixed(1)} tok/s</span><span>⏱ ${(data.latency_us / 1000).toFixed(1)} ms</span><span>🧠 Micro-Transformer</span>`;
        
        streamBotReply(botMsg, data.reply, meta, data.tokens_sec || 20);
        
        if (data.free_sram) {
            chipStatus.innerText = `Free SRAM: ${(data.free_sram / 1024).toFixed(1)} KB`;
        }
    } catch (err) {
        botMsg.querySelector('.bubble').innerText = 'Connection error to ESP32-S3. Please verify your WiFi connection!';
        const errMeta = document.createElement('div');
        errMeta.className = 'meta';
        errMeta.innerHTML = '<span>Error</span>';
        botMsg.appendChild(errMeta);
    } finally {
        sendBtn.disabled = false;
        msgInput.focus();
    }
}

function sendPrompt(text) {
    msgInput.value = text;
    sendMessage();
}

// Periodic Status Update
async function fetchStatus() {
    try {
        const res = await fetch('/api/status');
        const data = await res.json();
        chipStatus.innerText = `Free SRAM: ${(data.free_sram / 1024).toFixed(1)} KB | Uptime: ${data.uptime_sec}s`;
    } catch (e) {}
}

setInterval(fetchStatus, 5000);
fetchStatus();

</script>
</body>
</html>
)rawliteral";

} // namespace Web
