const express = require('express');
const router = express.Router()
const { execFileSync } = require('child_process');
const multer = require('multer');
const path = require('path');
const fs = require('fs');

const uploadsDir = path.join(__dirname, '../uploads');
if(!fs.existsSync(uploadsDir)){
    fs.mkdirSync(uploadsDir);
}

const storage = multer.diskStorage({
    destination: (req, file, cb) => {
        cb(null, uploadsDir);
    },
    filename: (req, file, cb) => {
        cb(null, file.originalname);
    }
});

const upload = multer({ storage });

router.post('/upload', upload.single('file'), (req, res) => {
    try {
        if (!req.file)
        { return res.status(400).json({ error: 'No file path provoided' }); }

        const filePath = req.file.path;

        res.json({ filePath });
    }
    catch (err) {
        res.status(500).json({ error: err.message });
    }
});

router.post('/analyze', (req, res) => {
    try {
        const filePath = req.body.filePath;

        if (!filePath) {
            return res.status(400).json({ error: 'No file path provoided' });
        }

        const binaryPath = path.join(__dirname, '../../rbda.exe');
        const output = execFileSync(binaryPath, [filePath]).toString();

        const stats = {};
        output.trim().split('\n').forEach(line => {
            if (line.includes('=')) {
                const [key, value] = line.split('=');
                stats[key] = isNaN(value) ? value : Number(value);
            }
        });

        res.json(stats);
    }
    catch (err) {
        res.status(500).json({ error: err.message });
    }
});

module.exports = router;