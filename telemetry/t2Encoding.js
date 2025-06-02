// Constructor function
function Encoding(profile) {
   this._encoding_type = null;
   this._reportFormat = null;
   this._reportTimestamp = false;
   this._parseProfile(profile);
}

// Parse profile to configure encoding
Encoding.prototype._parseProfile = function (profile) {
   // Ensure console is defined
   var console = console || { log: function () { } };

   if (Object.prototype.hasOwnProperty.call(profile, 'EncodingType')) {
      this._encoding_type = profile['EncodingType'];

      if (profile['EncodingType'] === 'JSON') {
         if (Object.prototype.hasOwnProperty.call(profile, 'JSONEncoding')) {
            var element = profile['JSONEncoding'];
            this._reportFormat = element['ReportFormat'];
            this._reportTimestamp = element['ReportTimestamp'];
         } else {
            console.log("ERROR: EncodingType is JSON; missing JSONEncoding field");
         }
      }
   }
};

// Encode data based on report format
Encoding.prototype.encode = function (body) {
   if (this._reportFormat === 'NameValuePair') {
      return body;
   } else {
      return '';
   }
};

// CommonJS export for Duktape
if (typeof module !== 'undefined' && module.exports) {
   module.exports = Encoding;
}