var fs = require('fs');
const SHEENS = require('littlesheens');

SHEENS.times.enable();

function print() {
  console.log(...arguments);
}

var Cfg = {
   MaxSteps: 100,
   debug: false,
   timers: [],
   fire: function (id, dest) { 
      var message = {to: dest, event: id};
      print("FIRE: ", message);
      var result= process_input(crew_js, message);
      print(result.emitted);
      crew_js = result.crew;
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
         print("CrewProcess routing: ", JSON.stringify(targets));
      } else {
         // RDL:  Could add a no unsolicited flag
         // The entire crew will see this message.
         targets = [];
         for (var mid in crew.machines) {
            targets.push(mid);
         }
      }
      //print(targets);

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
            steppeds[mid] = SHEENS.walk(Cfg, spec, state, message);
         } // Otherwise just move on.
      }

      return JSON.stringify(steppeds);
   } catch (err) {
      print("CrewProcess error", err, JSON.stringify(err));
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
            print("ERROR: Hit limit");
         }
      }
      return emitted;
   } catch (err) {
      print("GetEmitted error", err, JSON.stringify(err));
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
      print("CrewUpdate error", err, JSON.stringify(err));
      throw JSON.stringify({err: err, errstr: JSON.stringify(err)});
   }
}

//
// process_input
//
function process_input(crew, message) {
   var message = JSON.stringify(message);

   // process
   var steppeds = CrewProcess(crew, message); 
   var result = {};

   // get result
   result.emitted = GetEmitted(steppeds);

   // update crew
   result.crew = CrewUpdate(crew, steppeds); 
   return result;
}

function genRandomId(length) { 
   return Math.random().toString(36).substring(2, length + 2); 
}

//var id = "t2example-"+genRandomId(5);

//var parameters = JSON.parse('{"Parameters": [{"type":"dataModel","reference":"Profile.Name","reportEmpty":true},{"type":"dataModel","reference":"Profile.Version","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_SystemTime","name":"timestamp","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.ProductClass","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_COMCAST-COM_CM_MAC","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_GatewayManagement.Gateway.1.MacAddress","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_GatewayManagement.Gateway.2.MacAddress","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_GatewayManagement.GatewayRestoreAttemptCount","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_GatewayManagement.Gateway.1.OperationStatus","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_GatewayManagement.Gateway.2.OperationStatus","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_GatewayManagement.Gateway.1.ActiveStatus","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_GatewayManagement.Gateway.2.ActiveStatus","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_WanManager.InterfaceAvailableStatus","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_WanManager.InterfaceActiveStatus","reportEmpty":true},{"type":"dataModel","reference":"Device.X_RDK_Remote.Device.2.Status","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.supported","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.batteryHealth","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.batteryStatus","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.chargingStatus","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.estimatedChargeRemaining","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.estimatedMinutesRemaining","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.secondsOnBattery","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.batteryCapacity","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.serialNumber","reportEmpty":true},{"type":"dataModel","reference":"Device.Cellular.X_RDK_Status","reportEmpty":true},{"type":"dataModel","reference":"Device.Cellular.X_RDK_Enable","reportEmpty":true},{"type":"dataModel","reference":"Device.Cellular.Interface.1.X_RDK_RadioEnvConditions","reportEmpty":true},{"type":"dataModel","reference":"Device.Cellular.Interface.1.RSSI","reportEmpty":true},{"type":"dataModel","reference":"Device.Cellular.Interface.1.X_RDK_SNR","reportEmpty":true},{"type":"dataModel","reference":"Device.Cellular.Interface.1.RSRP","reportEmpty":true},{"type":"dataModel","reference":"Device.Cellular.Interface.1.RSRQ","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.SerialNumber","reportEmpty":true},{"type":"dataModel","reference":"Device.DeviceInfo.SoftwareVersion","reportEmpty":true}]}');
var parameters = JSON.parse('{"Parameters": [{"type":"dataModel","reference":"Profile.Name","reportEmpty":true}]}');

var timers = {
   generateNow: true,
   firstInterval: 0,
   periodicInterval: 0,
};

var crew = {
   id:"simpsons",
   machines: {}
};

var id = genRandomId(5);
crew.machines[id] = {spec:"specs/t2example.js",node:"stop", bs:{_id: id, timers: Object.assign({}, timers), parameters:parameters['Parameters']}};

timers.generateNow = false;
id = genRandomId(5);
crew.machines[id] = {spec:"specs/t2example.js",node:"stop", bs:{_id: id, timers: Object.assign({}, timers), parameters:parameters['Parameters']}};

var crew_js = JSON.stringify(crew);
print(crew_js);

/*
result = process_input(crew, {to: ["doubler"], double:1});
*/

result = process_input(crew_js, {event: 'start'});
print(result.emitted);
crew_js = result.crew;

// Clear all tasks after 15 seconds
setTimeout(function () {
   console.log('shutting down crews');
   var result= process_input(crew_js, {event: 'stop'});
   print(result.emitted);
   crew_js = result.crew;

   timers = Cfg.timers;
   for (var i = 0; i < timers.length; i++) {
      clearInterval(timers[i]);
   }

   print();
   print();
   print("Performance summary:  ",SHEENS.times.summary());
}, 10000);
