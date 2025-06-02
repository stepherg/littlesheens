// Helper: Generate random integer between min and max (inclusive)
function getRandomInt(min, max) {
   min = Math.ceil(min);
   max = Math.floor(max);
   return Math.floor(Math.random() * (max - min + 1)) + min;
}

// Helper: Generate random string of given length
function generateRandomString(length) {
   var result = '';
   var characters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
   var charactersLength = characters.length;
   for (var i = 0; i < length; i++) {
      result += characters.charAt(Math.floor(Math.random() * charactersLength));
   }
   return result;
}

// Event constructor
function Event(component, name, use) {
   var e = {
      component: component,
      name: name,
      use: use,
      startTime: Date.now()
   };

   // Default use to "absolute" if undefined
   if (typeof e.use === 'undefined') {
      e.use = 'absolute';
   }

   // Bind _getValue
   e.binding = Event.prototype._getValue.bind(null, e);

   return e;
}

// Event methods
Event.prototype._getValue = function (e) {
   var obj = {};
   obj[e.name] = getRandomInt(0, 100); // Random value for simulation
   return obj;
};

Event.prototype.unRegisterEvent = function (e) {
   console.log("STOP monitoring for " + e.use + " for " + e.name + " from " + e.component);
};

Event.prototype.dispose = function (e) {
   this.unRegisterEvent(e);
};

// DataModel constructor
function DataModel(name, reference, reportEmpty) {
   var d = {
      name: name,
      reference: reference,
      reportEmpty: reportEmpty,
      startTime: Date.now()
   };

   // Default name to reference if null
   if (d.name === null) {
      d.name = d.reference;
   }

   // Bind _getValue
   d.binding = DataModel.prototype._getValue.bind(null, d);

   return d;
}

// DataModel methods
DataModel.prototype._getValue = function (d, ctx) {
   var obj = {};

   // Local references
   if (Object.prototype.hasOwnProperty.call(ctx, d.reference)) {
      obj[d.name] = ctx[d.reference];
   } else {
      obj[d.name] = generateRandomString(getRandomInt(5, 30));
   }

   if (d.reportEmpty === true) {
      return obj;
   } else {
      return null;
   }
};

DataModel.prototype.dispose = function (d) {
   // No-op
};

// Grep constructor
function Grep(name, search, logfile) {
   var g = {
      name: name,
      search: search,
      logfile: logfile,
      startTime: Date.now()
   };

   // Bind _getValue
   g.binding = Grep.prototype._getValue.bind(null, g);

   return g;
}

// Grep methods
Grep.prototype._getValue = function (g) {
   var obj = {};
   obj[g.name] = getRandomInt(0, 100); // Random value for simulation
   return obj;
};

Grep.prototype.dispose = function (g) {
   // No-op
};

// CommonJS exports for Duktape
if (typeof module !== 'undefined' && module.exports) {
   module.exports = {
      Event: Event,
      DataModel: DataModel,
      Grep: Grep
   };
}