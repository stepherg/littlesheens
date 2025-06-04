var fs = require('fs');
const SHEENS = require('littlesheens');

function print() {
  console.log(...arguments);
}


var Cfg = {
   MaxSteps: 100,
   debug: false,
   fire: function (id) { 
      //console.log("got it: "+id); 
      var result= process_input(crew, {event: id});
      print(result.emitted);
      crew = result.crew;
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

function GetSpec(filename) {
   return JSON.parse(fs.readFileSync(filename, 'utf8'));
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
            var spec = GetSpec(machine.spec);

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


var id = "t2example-"+genRandomId(5);

var crew = JSON.stringify({
   id:"simpsons",
   machines:{
//      doubler:{spec:"specs/double.js",node:"listen",bs:{count:0}},
//      turnstile:{spec:"specs/turnstile.js",node:"locked",bs:{}},
      t2example:{spec:"specs/t2example.js",node:"start",bs:{_id: id}}
   }
});

/*
result = process_input(crew, {to: ["doubler"], double:1});
print(result.emitted);
crew = result.crew;

result = process_input(crew, {to: ["doubler"], double:10});
print(result.emitted);
crew = result.crew;

result = process_input(crew, {to: ["doubler"], double:100});
print(result.emitted);
crew = result.crew;

result = process_input(crew, {double:100});
//print(result.emitted);
crew = result.crew;
//print(crew);

result = process_input(crew, {double:100});
//print(result.emitted);
crew = result.crew;
//print(crew);
*/

result = process_input(crew, {});
print(result.emitted);
crew = result.crew;

/*

message = JSON.stringify({double:100});
steppeds = CrewProcess(crew, message); 
print(GetEmitted(steppeds));
crew = CrewUpdate(crew, steppeds); 

message = JSON.stringify({input:"push"});
steppeds = CrewProcess(crew, message); 
print(GetEmitted(steppeds));
crew = CrewUpdate(crew, steppeds); 

message = JSON.stringify({input:"coin"});
steppeds = CrewProcess(crew, message); 
print(GetEmitted(steppeds));
crew = CrewUpdate(crew, steppeds); 

message = JSON.stringify({input:"coin"});
steppeds = CrewProcess(crew, message); 
print(GetEmitted(steppeds));
crew = CrewUpdate(crew, steppeds); 

message = JSON.stringify({input:"push"});
steppeds = CrewProcess(crew, message); 
print(GetEmitted(steppeds));
crew = CrewUpdate(crew, steppeds); 
*/

// Clear all tasks after 15 seconds
setTimeout(function () {
   console.log('wrapping up');
   exit();
}, 30000);
