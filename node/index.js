
var SHEENS = null;
// Timestamp + monotonic elapsed helper (gated by LOG_TS)
const __LOG_TS_ENABLED = (process.env.LOG_TS || '1').toLowerCase() !== '0' && (process.env.LOG_TS || '').toLowerCase() !== 'false';
let __logStart;
if (__LOG_TS_ENABLED) {
  __logStart = process.hrtime.bigint();
  function __ts() {
     const iso = new Date().toISOString();
     const diffNs = Number(process.hrtime.bigint() - __logStart);
     const secs = (diffNs / 1e9).toFixed(6); // microsecond precision
     return iso + ' +' + secs + 's';
  }
  const __origConsole = {
     log: console.log.bind(console),
     error: console.error.bind(console),
     warn: console.warn.bind(console),
     info: console.info.bind(console),
     debug: console.debug ? console.debug.bind(console) : console.log.bind(console)
  };
  ['log','error','warn','info','debug'].forEach(fn => {
     const orig = __origConsole[fn];
     console[fn] = function () {
        if (arguments.length === 0) return orig();
        const first = arguments[0];
        const rest = Array.prototype.slice.call(arguments, 1);
        if (typeof first === 'string' && /^\d{4}-\d{2}-\d{2}T/.test(first)) {
           return orig.apply(console, arguments); // already tagged
        }
        orig.apply(console, [__ts(), '-', first, ...rest]);
     };
  });
}
try {
   const prof = require('../js/prof');
   const step = require('../js/step');
   const safeEval = require('safe-eval');
   global.safeEval = safeEval;
   global.sandbox = (code, ctx) => safeEval(code, ctx || {});
   global.Times = prof;
   global.print = function(){ console.log.apply(console, arguments); };
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
const WS_URL = process.env.BLIZZARD_WS_URL || 'ws://localhost:8820';
const ws = new BlizzardWsClient(WS_URL, {token: process.env.BLIZZARD_TOKEN});

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
// Stress spec specific environment overrides
if (SPEC_NAME === 'blizzard_timer_stress') {
   function intEnv(name, def) {
      if (process.env[name] == null) return def;
      const v = parseInt(process.env[name], 10);
      return isNaN(v) ? def : v;
   }
   function boolEnv(name, def) {
      if (process.env[name] == null) return def;
      const val = process.env[name].toLowerCase();
      return !(val === '0' || val === 'false' || val === 'no');
   }
   baseBindings.concurrency = intEnv('CONCURRENCY', 5);
   baseBindings.base_delay_ms = intEnv('BASE_DELAY_MS', 200);
   baseBindings.jitter_ms = intEnv('JITTER_MS', 50);
   baseBindings.max_iters_per_timer = intEnv('MAX_ITERS_PER_TIMER', 50);
   baseBindings.require_pong_event = boolEnv('REQUIRE_PONG_EVENT', true);
   if (process.env.SEED != null) {
      baseBindings.seed = parseInt(process.env.SEED, 10) || 1;
   }
   // Optional periodic stats dump interval (ms)
   const STRESS_DUMP_MS = intEnv('STRESS_DUMP_MS', 5000);
   if (STRESS_DUMP_MS > 0) {
      setInterval(() => {
         try {
            const c = JSON.parse(crew_js);
            const mid = Object.keys(c.machines)[0];
            if (!mid) return;
            const bs = c.machines[mid].bs;
            if (!bs || bs.timers == null) return;
            const timers = bs.timers;
            let active = 0, done = 0;
            const stats = {};
            for (const k in timers) {
               const t = timers[k];
               if (t.done) done++; else active++;
               stats[k] = {iter: t.iter, done: !!t.done, outstanding: !!t.outstanding};
            }
            console.log('[stress-progress]', JSON.stringify({active, done, total: active+done, done_count: bs.done_count, total_pings: bs.total_pings, total_pongs: bs.total_pongs, sample: stats}));
         } catch (e) {
            console.warn('Periodic stress dump failed:', e.message);
         }
      }, STRESS_DUMP_MS).unref();
   }
}
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
   const crewB = {id: 'blizzard-provider-demo', machines: {}};
   const id1 = genRandomId(6);
   // Merge previously prepared baseBindings so stress-specific overrides propagate
   const merged = Object.assign({}, baseBindings, {_id: id1});
   crewB.machines[id1] = {spec: specFile, node: 'stop', bs: merged};
   crew_js = JSON.stringify(crewB);
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
