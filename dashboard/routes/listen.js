const express = require('express');
const router = express.Router();
const chokidar = require('chokidar');
const { execFileSync } = require('child_process');
const path = require('path');
const fs = require('fs');

let watcher = null;
let sseClients = [];

function broadcast(payload) {
    const msg = `data: ${JSON.stringify(payload)}\n\n`;
    sseClients.forEach(res => res.write(msg));
}

function analyzeFile(filePath) {
    const binaryPath = path.join(__dirname, '../../rbda.exe');
    try {
        const output = execFileSync(binaryPath, [filePath]).toString();
        const stats = {};
        output.trim().split('\n').forEach(line => {
            if (line.includes('=')) {
                const [key, ...rest] = line.split('=');
                const value = rest.join('=');
                stats[key.trim()] = isNaN(value) ? value.trim() : Number(value);
            }
        });
        return { ok: true, file: path.basename(filePath), stats };
    } catch (err) {
        return { ok: false, file: path.basename(filePath), error: err.message };
    }
}

/* SSE stream — browser subscribes here and receives pushed events */
router.get('/listen/stream', (req, res) => {
    res.setHeader('Content-Type', 'text/event-stream');
    res.setHeader('Cache-Control', 'no-cache');
    res.setHeader('Connection', 'keep-alive');
    res.flushHeaders();

    /* Keep-alive ping every 20s to prevent proxy/browser timeout */
    const ping = setInterval(() => res.write(': ping\n\n'), 20000);
    sseClients.push(res);

    req.on('close', () => {
        clearInterval(ping);
        sseClients = sseClients.filter(c => c !== res);
    });
});

router.post('/listen/start', (req, res) => {
    if (watcher) return res.json({ status: 'already_listening' });

    const dataDir = path.join(__dirname, '../../data');
    if (!fs.existsSync(dataDir)) fs.mkdirSync(dataDir, { recursive: true });

    watcher = chokidar.watch(dataDir, {
        persistent: true,
        ignoreInitial: true,
        awaitWriteFinish: {
            stabilityThreshold: 2000,
            pollInterval: 500,
        },
    });

    watcher.on('add', filePath => {
        if (filePath.endsWith('.dat'))
            broadcast({ event: 'new_file', ...analyzeFile(filePath) });
    });

    watcher.on('change', filePath => {
        if (filePath.endsWith('.dat'))
            broadcast({ event: 'file_changed', ...analyzeFile(filePath) });
    });

    watcher.on('error', err => {
        broadcast({ event: 'watcher_error', error: err.message });
    });

    res.json({ status: 'listening' });
});

router.post('/listen/stop', (req, res) => {
    if (watcher) { watcher.close(); watcher = null; }
    broadcast({ event: 'stopped' });
    res.json({ status: 'stopped' });
});

router.get('/listen/status', (req, res) => {
    res.json({ listening: watcher !== null });
});

module.exports = router;
