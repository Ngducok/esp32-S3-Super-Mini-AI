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

async function sendMessage() {
    const text = msgInput.value.trim();
    if (!text) return;
    
    msgInput.value = '';
    appendMessage('user', text, '<span>User Prompt</span>');
    sendBtn.disabled = true;

    try {
        const response = await fetch('/api/chat', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ message: text })
        });
        
        const data = await response.json();
        const meta = `<span>⚡ ${(data.tokens_sec || 20).toFixed(1)} tok/s</span><span>⏱ ${(data.latency_us / 1000).toFixed(1)} ms</span><span>🧠 Micro-Transformer</span>`;
        appendMessage('bot', data.reply, meta);
        
        if (data.free_sram) {
            chipStatus.innerText = `Free SRAM: ${(data.free_sram / 1024).toFixed(1)} KB`;
        }
    } catch (err) {
        appendMessage('bot', 'Connection error to ESP32-S3. Please verify your WiFi hotspot connection!', '<span>Error</span>');
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
