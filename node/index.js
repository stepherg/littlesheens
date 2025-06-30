var SHEENS = null;
try {
   SHEENS = require('littlesheens');
} catch (e) {}

if (SHEENS) {
   var fs = require('fs');
   SHEENS.times.enable();
   walk = SHEENS.walk;
   Times = SHEENS.times;
}

var rbus = null;
try {
   var rbus = require('./rbus');
} catch (e) {}

const WebSocket = require('ws');
const ws = new WebSocket('ws://localhost:8080');

Times.enable();

var Cfg = {
   MaxSteps: 100,
   debug: false,
   timers: [],
   fire: function (id, dest) {
      var ev= {to: dest, event: id};
      console.log("EVENT TIMER: ", ev);
      crew_js = process_event(crew_js, ev);
   },
   // rbus_event params
   action_callback: function(id, dest) {
      var ev= {to: dest, event: id};
      console.log("EVENT ACTION: ", ev);
      crew_js = process_event(crew_js, ev);
   }
};

var Stats = {
   GetSpec: 0,
   ParseSpec: 0,
   Process: 0,
   CrewProcess: 0,
   CrewUpdate: 0,
   SpecCacheHits: 0,
   SpecCacheMisses: 0
};

function GetSpec(filename, id) {
   // do some simple templating
   var spec = fs.readFileSync(filename, 'utf8');
   spec = spec.replaceAll('${id}', id);
   return JSON.parse(spec);
}

//
// CrewProcess
//
function CrewProcess(crew_js, message_js) {
   Stats.CrewProcess++;

   try {
      var crew = JSON.parse(crew_js);
      var message = JSON.parse(message_js);

      // Optionally direct the message to a single machine as
      // specified in the message's (optional) "to" property.  For
      // example, if the message has the form {"to":"m42",...}, then
      // that message will be sent to machine m42 only.  Generalize
      // to accept an array: "to":["m42","m43"].

      var targets = message.to;
      if (targets) {
         // Routing to specific machine(s).
         if (typeof targets == 'string') {
            targets = [targets];
         }
         console.log("CrewProcess routing: ", JSON.stringify(targets));
      } else {
         console.log("CrewProcess routing to all");
         // RDL:  Could add a no unsolicited flag
         // The entire crew will see this message.
         targets = [];
         for (var mid in crew.machines) {
            targets.push(mid);
         }
      }
      //console.log(targets);

      var steppeds = {};
      for (var i = 0; i < targets.length; i++) {
         var mid = targets[i];
         var machine = crew.machines[mid];
         if (machine) {
            // get the spec file
            var spec = GetSpec(machine.spec, machine.bs._id);

            // stage the state from the crew
            var state = {
               node: machine.node,
               bs: machine.bs
            };

            //
            // WALK the spec
            //
            steppeds[mid] = walk(Cfg, spec, state, message);
         } // Otherwise just move on.
      }

      return JSON.stringify(steppeds);
   } catch (err) {
      console.log("CrewProcess error", err, JSON.stringify(err));
      throw JSON.stringify({err: err, errstr: JSON.stringify(err)});
   }
}

//
// GetEmitted
//
function GetEmitted(steppeds_js) {
   try {

      var steppeds = JSON.parse(steppeds_js);
      var emitted = [];
      for (var mid in steppeds) {
         var stepped = steppeds[mid];
         var msgs = stepped.emitted;
         for (var i = 0; i < msgs.length; i++) {
            emitted.push(JSON.stringify(msgs[i]));
         }
         if (steppeds[mid].stoppedBecause == "limited") {
            console.log("ERROR: Hit limit");
         }
      }
      return emitted;
   } catch (err) {
      console.log("GetEmitted error", err, JSON.stringify(err));
      throw JSON.stringify({err: err, errstr: JSON.stringify(err)});
   }
}

// sandbox('JSON.stringify({"bs":{"x":1+2},"emitted":["test"]})');

//
// CrewUpdate
//
function CrewUpdate(crew_js, steppeds_js) {
   Stats.CrewUpdate++;
   try {

      var crew = JSON.parse(crew_js);
      var steppeds = JSON.parse(steppeds_js);
      for (var mid in steppeds) {
         var stepped = steppeds[mid];
         crew.machines[mid].node = stepped.to.node;
         crew.machines[mid].bs = stepped.to.bs;
      }

      return JSON.stringify(crew);
   } catch (err) {
      console.log("CrewUpdate error", err, JSON.stringify(err));
      throw JSON.stringify({err: err, errstr: JSON.stringify(err)});
   }
}

//
// process_event
//
function process_event(crew, ev) {
   var ev= JSON.stringify(ev);

   //console.log("Processing: ", ev);

   // process
   var steppeds = CrewProcess(crew, ev);
   var result = {};

   // get result
   result.emitted = GetEmitted(steppeds);
   for (var i = 0; i < result.emitted.length; i++) {
      //console.log(result.emitted[i]);
      var emit = JSON.parse(result.emitted[i]);
      if(emit.hasOwnProperty("actions")) {
         // if there's an action
         for (var j = 0; j < emit.actions.length; j++) {
            var action = emit.actions[j];
            console.log(action);
            if(action.hasOwnProperty("jsonrpc")) {
               ws.send(JSON.stringify(action));
            }
         }
      } else {
         console.log(emit);
      }
      
   }

   // update crew
   result.crew = CrewUpdate(crew, steppeds);
   return result.crew;
}

function genRandomId(length) {
   return Math.random().toString(36).substring(2, length + 2);
}

//var id = "t2example-"+genRandomId(5);

var parameters = JSON.parse('{"Parameters": [{"type":"dataModel","reference":"Profile.Name","reportEmpty":true},{"type":"dataModel","reference":"Profile.Version","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_SystemTime","name":"timestamp","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.ProductClass","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_COMCAST-COM_CM_MAC","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_GatewayManagement.Gateway.1.MacAddress","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_GatewayManagement.Gateway.2.MacAddress","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_GatewayManagement.GatewayRestoreAttemptCount","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_GatewayManagement.Gateway.1.OperationStatus","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_GatewayManagement.Gateway.2.OperationStatus","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_GatewayManagement.Gateway.1.ActiveStatus","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_GatewayManagement.Gateway.2.ActiveStatus","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_WanManager.InterfaceAvailableStatus","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_WanManager.InterfaceActiveStatus","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_Remote.Device.2.Status","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.supported","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.batteryHealth","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.batteryStatus","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.chargingStatus","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.estimatedChargeRemaining","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.estimatedMinutesRemaining","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.secondsOnBattery","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.batteryCapacity","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.serialNumber","reportEmpty":true},{"type":"dataModel","reference":"Device.Cellular.X_RDK_Status","reportEmpty":true},{"type":"dataModel","reference":"Device.Cellular.X_RDK_Enable","reportEmpty":true},{"type":"dataModel","reference":"Device.Cellular.Interface.1.X_RDK_RadioEnvConditions","reportEmpty":true},{"type":"dataModel","reference":"Device.Cellular.Interface.1.RSSI","reportEmpty":true},{"type":"dataModel","reference":"Device.Cellular.Interface.1.X_RDK_SNR","reportEmpty":true},{"type":"dataModel","reference":"Device.Cellular.Interface.1.RSRP","reportEmpty":true},{"type":"dataModel","reference":"Device.Cellular.Interface.1.RSRQ","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.SerialNumber","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.SoftwareVersion","reportEmpty":true}]}');
//var parameters = JSON.parse('{"Parameters": [{"type":"dataModel","reference":"Profile.Name","reportEmpty":true}]}');

var crew = {
   id: "simpsons",
   machines: {}
};

var timers = {
   generateNow: true,
   firstInterval: 0,
   periodicInterval: 0,
};
var id = genRandomId(5);
crew.machines[id] = {spec: "specs/t2example.js", node: "stop", bs: {_id: id, timers: Object.assign({}, timers), parameters: parameters['Parameters']}};

timers.generateNow = false;
timers.periodicInterval = 10000;
id = genRandomId(5);
crew.machines[id] = {spec: "specs/t2example.js", node: "stop", bs: {_id: id, timers: Object.assign({}, timers), parameters: parameters['Parameters']}};

var crew_js = JSON.stringify(crew);
console.log(crew_js);

ws.on('open', () => {
   crew_js = process_event(crew_js, {event: 'start'});

   // Clear all tasks after 60seconds
   setTimeout(function () {
      console.log('shutting down crews');
      crew_js = process_event(crew_js, {event: 'stop'});

      timers = Cfg.timers;
      for (var i = 0; i < timers.length; i++) {
         clearInterval(timers[i]);
      }

      console.log();
      console.log();
      console.log("Performance summary: ", Times.summary());
      // TODO: need to unsubscribe
      ws.close();
   }, 60000);
});

ws.on('message', (data) => {
   try {
      const response = JSON.parse(data);
      console.log('Received response:');
      console.log(JSON.stringify(response, null, 2));

      // Handle notifications (no id)
      //if (!response.id && response.method === 'rbus_event') {
      if (!response.id) {
         console.log(`Event received: ${JSON.stringify(response)}`);
         var ev= {event: "jrpc-event"};
         crew_js = process_event(crew_js, ev);
         return;
      } else if (response.id && response.result){
         var ev= {to: response.id, 'jrpc-response': response.result};
         crew_js = process_event(crew_js, ev);
         return;
      }

      if (response.error) {
         console.error(`failed: ${response.error.message}`);
      }

   } catch (error) {
      console.error('Error parsing response:', error.message);
      ws.close();
   }
});

ws.on('error', (error) => {
   console.error('WebSocket error:', error.message);
});

ws.on('close', () => {
   console.log('WebSocket connection closed');
});
