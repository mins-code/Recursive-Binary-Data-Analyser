const express = require('express');
const router = express.Router();
const { execFileSync } = require('child_process');
const path = require('path');
const fs = require('fs');
const os = require('os');

const API_BASE = 'https://blockstream.info/api';
const MAINNET_MAGIC = Buffer.from([0xF9, 0xBE, 0xB4, 0xD9]);
const POLL_INTERVAL_MS = 30_000; // poll every 30s; new blocks arrive ~every 10 min

let pollTimer = null;
let lastHash = null;
let sseClients = [];

function broadcast(payload) {
    const msg = `data: ${JSON.stringify(payload)}\n\n`;
    sseClients.forEach(res => res.write(msg));
}

async function fetchLatestTip() {
    const [hashRes, heightRes] = await Promise.all([
        fetch(`${API_BASE}/blocks/tip/hash`),
        fetch(`${API_BASE}/blocks/tip/height`),
    ]);
    if (!hashRes.ok || !heightRes.ok) throw new Error('Blockstream API unreachable');
    const hash   = (await hashRes.text()).trim();
    const height = parseInt(await heightRes.text(), 10);
    return { hash, height };
}

async function fetchRawBlock(hash) {
    const res = await fetch(`${API_BASE}/block/${hash}/raw`);
    if (!res.ok) throw new Error(`Raw block fetch failed: HTTP ${res.status}`);
    return Buffer.from(await res.arrayBuffer());
}

/* Wrap raw block bytes into the .dat format that rbda.exe expects:
   [4-byte mainnet magic][4-byte LE block size][raw block bytes] */
function wrapAsDat(rawBlock) {
    const size = Buffer.alloc(4);
    size.writeUInt32LE(rawBlock.length, 0);
    return Buffer.concat([MAINNET_MAGIC, size, rawBlock]);
}

function runParser(tmpFile) {
    const binaryPath = path.join(__dirname, '../../rbda.exe');
    const output = execFileSync(binaryPath, [tmpFile], { timeout: 60_000 }).toString();
    const stats = {};
    output.trim().split('\n').forEach(line => {
        if (line.includes('=')) {
            const [key, ...rest] = line.split('=');
            const value = rest.join('=');
            stats[key.trim()] = isNaN(value) ? value.trim() : Number(value);
        }
    });
    return stats;
}

async function analyzeBlock(hash, height) {
    const tmpFile = path.join(os.tmpdir(), `rbda_${hash.slice(0, 12)}.dat`);
    try {
        const rawBlock = await fetchRawBlock(hash);
        fs.writeFileSync(tmpFile, wrapAsDat(rawBlock));
        const stats = runParser(tmpFile);
        return { ok: true, hash, height, stats };
    } catch (err) {
        return { ok: false, hash, height, error: err.message };
    } finally {
        try { fs.unlinkSync(tmpFile); } catch {}
    }
}

async function pollChain() {
    try {
        const { hash, height } = await fetchLatestTip();
        if (hash === lastHash) return; // no new block yet

        broadcast({ event: 'new_block_detected', hash, height });
        const result = await analyzeBlock(hash, height);
        lastHash = hash;
        broadcast({ event: 'block_analysed', ...result });
    } catch (err) {
        broadcast({ event: 'poll_error', error: err.message });
    }
}

/* SSE stream — browser subscribes once and receives all pushed events */
router.get('/chain/stream', (req, res) => {
    res.setHeader('Content-Type', 'text/event-stream');
    res.setHeader('Cache-Control', 'no-cache');
    res.setHeader('Connection', 'keep-alive');
    res.flushHeaders();

    const ping = setInterval(() => res.write(': ping\n\n'), 20_000);
    sseClients.push(res);
    req.on('close', () => {
        clearInterval(ping);
        sseClients = sseClients.filter(c => c !== res);
    });
});

router.post('/chain/start', async (req, res) => {
    if (pollTimer) return res.json({ status: 'already_running' });

    res.json({ status: 'started', poll_interval_ms: POLL_INTERVAL_MS });

    // Check immediately, then on interval
    await pollChain();
    pollTimer = setInterval(pollChain, POLL_INTERVAL_MS);
});

router.post('/chain/stop', (req, res) => {
    if (pollTimer) { clearInterval(pollTimer); pollTimer = null; }
    lastHash = null;
    broadcast({ event: 'stopped' });
    res.json({ status: 'stopped' });
});

router.get('/chain/status', (req, res) => {
    res.json({ running: pollTimer !== null, last_hash: lastHash });
});

module.exports = router;
