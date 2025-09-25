
var SHEENS = null;
try {
   const prof = require('../js/prof');
   const step = require('../js/step');
   const safeEval = require('safe-eval');
   global.safeEval = safeEval;
   global.sandbox = (code, ctx) => safeEval(code, ctx || {});
   global.Times = prof;
   global.print = console.log.bind(console);
   global.match = require('../js/match');
   const sandboxMod = require('../js/sandbox');
   global.sandboxedAction = sandboxMod.sandboxedAction || global.sandboxedAction;
   global.sandboxedExpression = sandboxMod.sandboxedExpression || global.sandboxedExpression;
   SHEENS = {walk: step.walk, times: prof};
} catch (e) {
   try {
      SHEENS = require('littlesheens');
   } catch (e2) {}
}

if (!SHEENS) {
   console.error('Unable to load littlesheens engine');
   process.exit(1);
}

const fs = require('fs');
const {BlizzardWsClient} = require('./src/blizzard_ws_client');
const useBlizzard = !!process.env.BLIZZARD_WS_URL;
const ws = useBlizzard
   ? new BlizzardWsClient(process.env.BLIZZARD_WS_URL, {token: process.env.BLIZZARD_TOKEN})
   : new (require('ws'))(process.env.LOCAL_WS_URL || `ws://localhost:${process.env.LOCAL_WS_PORT || 8820}`);

const ENABLE_VERBOSE = !!process.env.VERBOSE;
function vLog() {
   if (ENABLE_VERBOSE) console.log.apply(console, arguments);
}

// Engine helpers
const walk = SHEENS.walk;
const Times = SHEENS.times;
Times.enable();

const Cfg = {
   MaxSteps: 100,
   debug: false,
   timers: [],
   fire: function (id, dest) {
      const ev = {to: dest, event: id};
      console.log('EVENT TIMER:', ev);
      crew_js = process_event(crew_js, ev);
   }
};

function GetSpec(filename, id) {
   let path = filename;
   if (!/^(\.|\/|[A-Za-z]:\\)/.test(filename) || filename.startsWith('specs/')) {
      path = require('path').join(__dirname, filename);
   }
   if (path.endsWith('.js')) {
      delete require.cache[require.resolve(path)];
      const mod = require(path);
      return typeof mod === 'function' ? mod(id) : mod;
   }
   let txt = fs.readFileSync(path, 'utf8');
   txt = txt.replaceAll('${id}', id);
   return JSON.parse(txt);
}

function CrewProcess(crew_js, message_js) {
   try {
      const crew = JSON.parse(crew_js);
      const message = JSON.parse(message_js);

      let targets = message.to;
      if (targets) {
         if (typeof targets === 'string') targets = [targets];
         console.log('CrewProcess routing:', JSON.stringify(targets));
      } else {
         console.log('CrewProcess routing to all');
         targets = Object.keys(crew.machines);
      }

      const steppeds = {};
      for (const mid of targets) {
         const m = crew.machines[mid];
         if (!m) continue;
         const spec = GetSpec(m.spec, m.bs._id);
         const state = {node: m.node, bs: m.bs};
         steppeds[mid] = walk(Cfg, spec, state, message);
      }
      return JSON.stringify(steppeds);
   } catch (err) {
      console.log('CrewProcess error', err);
      throw JSON.stringify({err: err, errstr: String(err)});
   }
}

function GetEmitted(steppeds_js) {
   const steppeds = JSON.parse(steppeds_js);
   const out = [];
   for (const mid in steppeds) {
      const stepped = steppeds[mid];
      for (const msg of stepped.emitted) {
         out.push(JSON.stringify(msg));
      }
      if (steppeds[mid].stoppedBecause === 'limited') {
         console.log('ERROR: Hit limit');
      }
   }
   return out;
}

function CrewUpdate(crew_js, steppeds_js) {
   const crew = JSON.parse(crew_js);
   const steppeds = JSON.parse(steppeds_js);
   for (const mid in steppeds) {
      const stepped = steppeds[mid];
      crew.machines[mid].node = stepped.to.node;
      crew.machines[mid].bs = steppeds[mid].to.bs;
   }
   return JSON.stringify(crew);
}

function process_event(crew, ev) {
   const steppeds = CrewProcess(crew, JSON.stringify(ev));
   const emitted = GetEmitted(steppeds);
   for (const ejs of emitted) {
      const emit = JSON.parse(ejs);
      if (emit.actions) {
         for (const action of emit.actions) {
            if (action.jsonrpc) {
               try {
                  ws.send(JSON.stringify(action));
               } catch (err) {
                  console.error('Failed send action', err);
               }
            }
         }
         if (emit.did === 'shutdown') {
            setTimeout(() => gracefulStop('shutdown'), 50);
         }
      } else {
         vLog(emit);
         if (emit.did === 'shutdown') {
            gracefulStop('shutdown');
         }
      }
   }
   return CrewUpdate(crew, steppeds);
}

function genRandomId(n) {
   return Math.random().toString(36).substring(2, n + 2);
}

let crew = {id: 'simpsons', machines: {}};
let timers = {generateNow: true, firstInterval: 0, periodicInterval: 0};
let id = genRandomId(5);
const SPEC_NAME = process.env.SPEC_NAME || 'blizzard_timer';
let specFile = `specs/${SPEC_NAME}.js`;
try {
   require.resolve(`./${specFile}`);
} catch (_) {
   console.warn('Spec', specFile, 'missing -> fallback');
   specFile = 'specs/blizzard_timer.js';
}

// Allow env overrides for specs that look for these
const ENV_MAX_ITERS = process.env.MAX_ITERS ? parseInt(process.env.MAX_ITERS, 10) : undefined;
const ENV_DELAY_MS = process.env.DELAY_MS ? parseInt(process.env.DELAY_MS, 10) : undefined;
let baseBindings = {_id: id, timers: Object.assign({}, timers)};
if (!isNaN(ENV_MAX_ITERS) && ENV_MAX_ITERS >= 0) {
   baseBindings.max_iters = ENV_MAX_ITERS;
}

if (!isNaN(ENV_DELAY_MS) && ENV_DELAY_MS >= 0) {
   baseBindings.delay_ms = ENV_DELAY_MS;
}

crew.machines[id] = {spec: specFile, node: 'stop', bs: baseBindings};
let crew_js = JSON.stringify(crew);
console.log(crew_js);

const onOpen = () => {
   if (useBlizzard) {
      const crewB = {id: 'blizzard-provider-demo', machines: {}};
      const id1 = genRandomId(6);
      crewB.machines[id1] = {spec: specFile, node: 'stop', bs: {_id: id1, count: 0}};
      crew_js = JSON.stringify(crewB);
   }
   crew_js = process_event(crew_js, {event: 'start'});
};

if (ws instanceof BlizzardWsClient) {
   ws.on('open', onOpen);
   ws.on('message', (d) => handleMessage(d));
   ws.on('error', (e) => {
      console.error('WebSocket error:', e.message || e);
   });
   ws.on('close', () => {
      console.log('WebSocket connection closed');
   });
   ws.connect().catch((e) => console.error('Failed to connect WS:', e.message || e));
} else {
   ws.on('open', onOpen);
   ws.on('message', (d) => handleMessage(d));
   ws.on('error', (e) => {
      console.error('WebSocket error:', e && (e.message || e));
   });
   ws.on('close', (code, reason) => {
      console.log('WebSocket connection closed', code, reason && reason.toString());
   });
}

function handleMessage(data) {
   try {
      const response = JSON.parse(data);
      vLog('Received response:', JSON.stringify(response));
      if (!response.id) {
         crew_js = process_event(crew_js, {event: 'jrpc-event', jrpc: response});
         return;
      }
      if (Object.prototype.hasOwnProperty.call(response, 'result')) {
         crew_js = process_event(crew_js, {to: response.id, 'invoke-response': response.result});
         return;
      }
      if (Object.prototype.hasOwnProperty.call(response, 'error')) {
         console.error('RPC error for id', response.id, response.error);
         return;
      }
   } catch (e) {
      console.error('Error parsing response:', e.message);
      if (!(ws instanceof BlizzardWsClient)) {
         ws.close();
      }
   }
}

let isStopping = false;
function gracefulStop(reason) {
   if (isStopping) {
      return;
   }
   isStopping = true;
   try {
      console.log('auto-stopping:', reason || 'completed');
      crew_js = process_event(crew_js, {event: 'stop'});
      const ts = Cfg.timers;
      for (const t of ts) {
         try {clearTimeout(t);} catch (_) {}
         try {clearInterval(t);} catch (_) {}
      }
      console.log('Performance summary:', Times.summary());
   } finally {
      try {
         ws.close();
      } catch (_) {}
   }
}

// Export for potential external control/tests
module.exports = {
   process_event,
   handleMessage,
   gracefulStop
};
