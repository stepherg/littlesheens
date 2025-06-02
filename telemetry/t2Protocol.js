// Constructor function
function Protocol(profile) {
   this._protocols = [];
   this._parseProfile(profile);
}

// Parse RBUS_METHOD headers
Protocol.prototype._parseHeaders = function (element) {
   var protocol = {};
   protocol.type = 'RBUS_METHOD';
   protocol.ep = null;
   protocol.headers = [];

   if (Object.prototype.hasOwnProperty.call(element, 'Method')) {
      protocol.ep = element['Method'];
   }

   var parameters = element['Parameters'];
   for (var i = 0; i < parameters.length; i++) {
      var param = parameters[i];
      var name = '';
      var value = '';
      if (Object.prototype.hasOwnProperty.call(param, 'name')) {
         name = param['name'];
      }
      if (Object.prototype.hasOwnProperty.call(param, 'value')) {
         value = param['value'];
      }
      if (name && value) {
         protocol.headers.push({ name: name, value: value });
      }
   }

   this._protocols.push(protocol);
};

// Parse HTTP headers
Protocol.prototype._parseHTTPHeaders = function (element) {
   var protocol = {};
   protocol.type = 'HTTP';
   protocol.headers = [];

   protocol.method = element['Method'];
   protocol.compression = element['Compression'];
   protocol.url = element['URL'];

   if (Object.prototype.hasOwnProperty.call(element, 'RequestURIParameter')) {
      var params = element['RequestURIParameter'];
      for (var i = 0; i < params.length; i++) {
         var param = params[i];
         var name = param['Name'];
         var value = param['Reference'];
         if (name && value) {
            var obj = {};
            obj[name] = value;
            protocol.headers.push(obj);
         }
      }
   }

   this._protocols.push(protocol);
};

// Parse profile
Protocol.prototype._parseProfile = function (profile) {
   if (Object.prototype.hasOwnProperty.call(profile, 'Protocol')) {
      if (profile['Protocol'] === 'RBUS_METHOD') {
         if (Object.prototype.hasOwnProperty.call(profile, 'RBUS_METHOD')) {
            var element = profile['RBUS_METHOD'];
            this._parseHeaders(element);
         } else {
            console.log('ERROR: Expected RBUS_METHOD');
         }
      } else if (profile['Protocol'] === 'HTTP') {
         if (Object.prototype.hasOwnProperty.call(profile, 'HTTP')) {
            var element = profile['HTTP'];
            this._parseHTTPHeaders(element);
         } else {
            console.log('ERROR: Expected HTTP');
         }
      } else {
         console.log('ERROR: Protocol ' + profile['Protocol'] + ' not handled');
      }
   }
};

// Send RBUS data (stub)
Protocol.prototype._sendDataRbus = function (protocol, ctx, body) {
   // console.log(protocol, body);
};

// Send HTTP data
Protocol.prototype._sendDataHttp = function (protocol, ctx, body) {
   var headers = {};
   for (var i = 0; i < protocol.headers.length; i++) {
      var header = protocol.headers[i];
      for (var key in header) {
         if (Object.prototype.hasOwnProperty.call(header, key)) {
            var value = header[key];
            if (Object.prototype.hasOwnProperty.call(ctx, value)) {
               headers[key] = ctx[value];
            } else {
               headers[key] = value;
            }
         }
      }
   }

   // Prepare request options
   var options = {
      url: protocol.url,
      method: protocol.method,
      headers: headers,
      body: body
   };

   console.log(options, body);

   // Use synchronous postSync
   try {
      var response = request.postSync(options.url, options);
      if (response.statusCode === 200) {
         console.log('HTTP Successful');
      } else {
         console.log('HTTP Failed: Status ' + response.statusCode);
      }
   } catch (err) {
      console.error('HTTP Error:', err.message);
   }
};

// Send data over all protocols
Protocol.prototype.sendData = function (ctx, body) {
   for (var i = 0; i < this._protocols.length; i++) {
      var protocol = this._protocols[i];
      if (protocol.type === 'RBUS_METHOD') {
         this._sendDataRbus(protocol, ctx, body);
      } else if (protocol.type === 'HTTP') {
         this._sendDataHttp(protocol, ctx, body);
      }
   }
};

// CommonJS export for Duktape
if (typeof module !== 'undefined' && module.exports) {
   module.exports = Protocol;
}