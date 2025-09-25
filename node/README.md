# Node Automations Runner

This directory contains a minimal Node.js wrapper (`index.js`) around the **littlesheens** state machine engine plus a Blizzard / WebSocket integration. It can run a selected machine spec locally (against a local WebSocket server) or against a remote Blizzard WebSocket endpoint.

## Contents
- `index.js` – main runner
- `src/build_specs.sh` – helper script invoked automatically to (re)generate JavaScript machine specs from YAML
- `spec_source/` – source YAML specs (input)
- `specs/` – generated JavaScript specs (output) used at runtime
- `package.json` – defines scripts (`preinstall`, `start`, `convert`) and dependencies

## Prerequisites
- Node.js ≥ 18 (recommended; earlier modern LTS versions may work but are untested here)
- A working Bash shell (for `src/build_specs.sh`)

## Installation
```bash
# From repository root or directly within this folder
cd node
npm install
```
`npm install` triggers the `preinstall` script (`./src/build_specs.sh`) which converts YAML specs in `spec_source/` into JavaScript specs in `specs/`.

If you add or change YAML specs later, you can regenerate manually:
```bash
npm run convert
# (Same as: ./src/build_specs.sh)
```

## Running
Basic run (local mode using an implicit local WebSocket server on port 8820):
```bash
npm start
```
This executes (see `package.json`):
```
./src/build_specs.sh; node --trace-uncaught index.js
```

### Selecting a Spec
Set `SPEC_NAME` (without extension) to choose which generated spec file (`specs/<name>.js`) is loaded.
```bash
SPEC_NAME=turnstile npm start
```
If the chosen spec is missing it falls back to `specs/blizzard_timer.js`.

### Connection & URL
The runner now always uses the Blizzard WebSocket client implementation. It connects to the URL in `BLIZZARD_WS_URL`, defaulting to:
```
ws://localhost:8820
```
Remote example:
```bash
BLIZZARD_WS_URL=wss://blizzard.example.com/ws \
BLIZZARD_TOKEN=yourAuthTokenHere \
SPEC_NAME=blizzard_timer \
npm start
```
If `BLIZZARD_TOKEN` is omitted the client still attempts the connection (useful for local/dev endpoints). The initial crew id differs slightly from legacy local mode; see `index.js`.

### Verbose Logging
Enable additional diagnostic output:
```bash
VERBOSE=1 npm start
```

### Spec Runtime Parameters
Some specs look for optional numeric overrides:
- `MAX_ITERS` – maximum iterations (added to bindings as `max_iters`)
- `DELAY_MS` – delay between iterations (added to bindings as `delay_ms`)

Example:
```bash
SPEC_NAME=ladder MAX_ITERS=25 DELAY_MS=200 VERBOSE=1 npm start
```

### Graceful Shutdown
Specs can emit a message with `{ did: 'shutdown' }` which triggers an orderly stop (timers cleared, performance summary printed). You can also terminate with Ctrl‑C; the underlying process will attempt to close the WebSocket.

## Performance Metrics
The embedded profiler (`Times`) is enabled by default and prints a summary on shutdown. Use this to identify stepping hotspots.

## Program Flow (High Level)
1. Load / provide the littlesheens engine (local JS implementation first, fallback to installed `littlesheens` package).
2. Open a WebSocket (local or Blizzard) based on environment.
3. Generate a crew with one machine bound to the chosen spec.
4. Emit a `start` event on socket open.
5. For each inbound WebSocket JSON message: route it into the machine(s) as either an invocation response or generic event.
6. For each emitted action containing `jsonrpc` fields: forward over the WebSocket.
7. On `shutdown` emission: stop timers, print performance summary, close the socket.

## Environment Variable Summary
| Variable | Purpose | Default |
|----------|---------|---------|
| `SPEC_NAME` | Base name of spec file in `specs/` (no extension) | `blizzard_timer` |
| `BLIZZARD_WS_URL` | WebSocket URL (local or remote) | `ws://localhost:8820` |
| `BLIZZARD_TOKEN` | Auth token for remote Blizzard endpoint (optional) | (unset) |
| `VERBOSE` | Enable extra logging | (disabled) |
| `MAX_ITERS` | Inject `max_iters` binding | (unset) |
| `DELAY_MS` | Inject `delay_ms` binding | (unset) |


## Direct Execution Without npm Scripts
You can invoke the runner directly once specs are generated:
```bash
# Regenerate specs manually
./src/build_specs.sh

# Run (defaults to ws://localhost:8820)
node index.js
```
Add environment exports inline as needed, for example:
```bash
BLIZZARD_WS_URL=wss://blizzard.example.com/ws BLIZZARD_TOKEN=token node index.js
```

## Troubleshooting
- "Unable to load littlesheens engine": The local JS engine files were not found and the `littlesheens` dependency (symlink / file ref) is missing. Ensure the submodule / local package `node-littlesheens` exists or install the published package if available.
- Spec fallback warning: You requested a spec that is not generated. Run `npm run convert` or check the `spec_source/` YAML name.
- No WebSocket responses: Confirm `BLIZZARD_WS_URL` is reachable and (if required) `BLIZZARD_TOKEN` is valid.

## Contributing / Extending
1. Add or modify a YAML spec under `spec_source/`.
2. Regenerate: `npm run convert`.
3. Run with `SPEC_NAME` pointing to the new spec base name.
4. If adding new action shapes, update consuming WebSocket services accordingly.

---
