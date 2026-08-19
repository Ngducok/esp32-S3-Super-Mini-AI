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
