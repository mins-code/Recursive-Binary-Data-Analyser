# Recursive Binary Data Analyser (RBDA)

A Bitcoin blockchain `.dat` file parser built in C that uses recursive descent to traverse the binary structure of block files and extract detailed statistics. Comes with a web dashboard to upload files and visualise results.

---

## What it does

Bitcoin Core stores its blockchain in binary `blk*.dat` files. RBDA loads one of these files, scans for block magic numbers, and recursively parses every block → transaction → input/output → script inside it, collecting statistics along the way.

**Parsed stats:**
- Total blocks, transactions, inputs, outputs, scripts
- Maximum recursion depth reached
- Total bytes processed
- Wall-clock parse time (ms)

---

## Project Structure

```
Recursive-Binary-Data-Analyser/
├── src/
│   ├── main.c        # Entry point — file loading, magic scan, stats output
│   ├── parser.c      # Recursive descent parser (block → tx → input/output → script)
│   ├── parser.h      # Stats and ParseCtx structs, function declarations
│   ├── varint.c      # Bitcoin variable-length integer decoder
│   ├── varint.h
│   └── scanner.c     # Standalone utility to verify magic number detection
├── dashboard/
│   ├── server.js     # Express.js server
│   ├── routes/
│   │   └── analyze.js  # POST /upload and POST /analyze endpoints
│   ├── public/
│   │   └── index.html  # Frontend (vanilla HTML/CSS/JS)
│   └── package.json
├── data/             # Place your blk*.dat files here (gitignored)
├── Makefile
└── README.md
```

---

## Prerequisites

| Tool | Purpose |
|------|---------|
| GCC  | Compile the C parser |
| Make | Build system |
| Node.js >= 18 | Run the dashboard server |

---

## Quick Start

### 1. Build the C parser

```bash
make
```

This produces `rbda.exe` (Windows) in the project root.

### 2. Run the parser directly (CLI)

```bash
./rbda.exe data/blk00001.dat
```

Output:
```
max_depth=4
total_blocks=11273
total_transactions=319370
total_inputs=594548
total_outputs=738204
total_scripts=1332752
total_bytes=134201449
elapsed_ms=44.00
```

### 3. Start the dashboard

```bash
cd dashboard
npm install      # first time only
node server.js
```

Then open **http://localhost:5000** in your browser.

---

## Dashboard Usage

1. **Drop or select** a `.dat` file in the upload zone
2. Click **Upload** — the file is sent to the server
3. Click **Analyse** — the parser runs and results appear as stat cards + raw JSON

---

## How the Parser Works

The parser implements a classic recursive descent over Bitcoin's binary block format:

```
parse_block()
  └─ parse_tx_list()
       └─ parse_tx()            (per transaction)
            ├─ parse_inputs()
            │    └─ parse_script()   (per input)
            └─ parse_outputs()
                 └─ parse_script()   (per output)
```

Each level reads from a shared `ParseCtx` cursor, advances the offset, and calls the next level down. The maximum allowed depth is capped at 64 to prevent stack overflow.

**Magic numbers supported:**

| Network   | Magic       |
|-----------|-------------|
| Mainnet   | 0xD9B4BEF9  |
| Testnet3  | 0x0709110B  |
| Testnet4  | 0x1C163F28  |
| Regtest   | 0xDAB5BFFA  |
| Signet    | 0x40CF030A  |

If a block fails to parse (truncated or corrupted), the scanner skips one byte and continues — this "bulletproof gap-skip" ensures partial files are still processed.

---

## API Endpoints

| Method | Path       | Body                        | Returns                  |
|--------|------------|-----------------------------|--------------------------|
| POST   | `/upload`  | `multipart/form-data` (file) | `{ filePath: "..." }`   |
| POST   | `/analyze` | `{ "filePath": "..." }`     | Stats JSON object        |

---

## Data Files

Place Bitcoin Core `blk*.dat` files inside the `data/` directory (gitignored due to size). The parser works with **unobfuscated** block files — i.e. standard files exported from Bitcoin Core without the v28+ XOR obfuscation key applied.
