// CommonJS require statements
var Scheduler = require('./scheduler');
var Protocol = require('./t2Protocol');
var Encoding = require('./t2Encoding');
var Trigger = require('./t2Trigger');
var Parameter = require('./t2Parameter');

// Global scheduler and trigger instances
var scheduler = new Scheduler();
var trigger = new Trigger();
var tasks = [];

// Profile constructor
function Profile(profile) {
   this._profile = profile;
   this._protocol = new Protocol(profile);
   this._encoding = new Encoding(profile);
   this._parameters = [];
   this._tasks = [];

   var generateNow = false;

   // Set up dataModel, event, and grep bindings
   var parameters = profile['Parameter'];
   for (var i = 0; i < parameters.length; i++) {
      var element = parameters[i];
      if (element['type'] === 'dataModel') {
         this._parameters.push(new Parameter.DataModel(element['name'], element['reference'], element['reportEmpty']));
      }
      if (element['type'] === 'event') {
         this._parameters.push(new Parameter.Event(element['component'], element['eventName'], element['use']));
      }
      if (element['type'] === 'grep') {
         this._parameters.push(new Parameter.Grep(element['marker'], element['search'], element['logfile']));
      }
   }

   // Generate NOW
   if (Object.prototype.hasOwnProperty.call(profile, 'GenerateNow')) {
      generateNow = profile['GenerateNow'];
      if (generateNow == 'true') {
         this._tasks.push(scheduler.addTask(Profile.prototype._generateReport.bind(null, this), 0));
      }
   }

   // Reporting Interval (periodic)
   if (Object.prototype.hasOwnProperty.call(profile, 'ReportingInterval')) {
      var interval = profile['ReportingInterval'] * 10; // Sped up for simulation
      this._tasks.push(scheduler.addTask(Profile.prototype._generateReport.bind(null, this), interval, true));
   }

   // Register Notify (Trigger on condition)
   if (Object.prototype.hasOwnProperty.call(profile, 'TriggerCondition')) {
      trigger.addTrigger(Profile.prototype._generateReport.bind(null, this), profile['TriggerCondition']);
   }
}

// Profile methods
Profile.prototype._generateReport = function (_this) {
   console.log('Task \'' + _this._profile['Description'] + '\' executed');

   var ctx = {};
   ctx['Profile.Name'] = _this._profile.Name;
   ctx['Profile.Version'] = _this._profile.Version;

   // TimeReference: '0001-01-01T00:00:00Z'
   if (Object.prototype.hasOwnProperty.call(_this._profile, 'TimeReference')) {
      var now = new Date();
      if (_this._profile['TimeReference'] === '0001-01-01T00:00:00Z') {
         var formattedTime = now.toISOString().slice(0, -5) + 'Z';
         ctx['Device.DeviceInfo.X_RDKCENTRAL-COM_SystemTime'] = formattedTime;
      }
   }

   var body = {};
   for (var i = 0; i < _this._parameters.length; i++) {
      var p = _this._parameters[i];
      var result = p.binding(ctx);
      for (var key in result) {
         if (Object.prototype.hasOwnProperty.call(result, key)) {
            body[key] = result[key];
         }
      }
   }

   _this._protocol.sendData({}, _this._encoding.encode(body));
};

Profile.prototype._processProfile = function (_this, ctx) {
   var profile = _this._profile;
   var context = {};
   var body = {};

   ctx['Profile.Name'] = profile.Name;
   ctx['Profile.Version'] = profile.Version;

   // TimeReference: '0001-01-01T00:00:00Z'
   if (Object.prototype.hasOwnProperty.call(profile, 'TimeReference')) {
      var now = new Date();
      if (profile['TimeReference'] === '0001-01-01T00:00:00Z') {
         var formattedTime = now.toISOString().slice(0, -5) + 'Z';
         context['Device.DeviceInfo.X_RDKCENTRAL-COM_SystemTime'] = formattedTime;
      }
   }
};

// Profiles constructor
function Profiles(directoryPath) {
   this._profiles = [];
   this._dirPath = directoryPath;
}

// Profiles methods
Profiles.prototype._getProfiles = function (directoryPath) {
   try {
      var files = fs.readdirSync(directoryPath);
      var fileContents = [];
      for (var i = 0; i < files.length; i++) {
         var file = files[i];
         var filePath = path.join(directoryPath, file);
         var fileStats = fs.statSync(filePath);
         if (fileStats.isFile()) {
            var content = yaml.load(fs.readFileSync(filePath, 'utf-8'));
            fileContents.push({ name: file, content: content });
         }
      }
      return fileContents;
   } catch (err) {
      console.error("Error reading directory:", err);
      return [];
   }
};

Profiles.prototype.addProfile = function (name, content) {
   // No-op in original
};

Profiles.prototype.processProfiles = function () {
   var profile_bundle = this._getProfiles(this._dirPath);
   for (var i = 0; i < profile_bundle.length; i++) {
      var profile_info = profile_bundle[i];
      if (Object.prototype.hasOwnProperty.call(profile_info.content, 'subdoc_name')) {
         var profiles = profile_info.content.profiles;
         for (var j = 0; j < profiles.length; j++) {
            var p2 = profiles[j];
            var p3 = p2.value;
            if (Object.prototype.hasOwnProperty.call(p2, 'name')) {
               p3.Name = p2.name;
            } else {
               p3.Name = 'NO_NAME';
            }
            new Profile(p3);
         }
      } else {
         profile_info.content.Name = profile_info.name;
         new Profile(profile_info.content);
      }
   }
};

// Clear all tasks after 15 seconds
setTimeout(function () {
   console.log('Clearing all tasks after 15 seconds');
   scheduler.clearAllTasks();
   console.log('Clearing all triggers after 15 seconds');
   trigger.clearAllTriggers();
}, 15000);
 
// CommonJS exports for Duktape
if (typeof module !== 'undefined' && module.exports) {
   module.exports = {
      Profile: Profile,
      Profiles: Profiles
   };
}