// Extend String.slice
if (!String.prototype.slice) {
   String.prototype.slice = function(start, end) {
      const length = this.length;
      var startIndex = start || 0;

      if (end === undefined) {
         var endIndex = length; 
      } else {
         var endIndex = end;
      }
      var endIndex = end === undefined ? length : end;

      if (startIndex < -length) {
         startIndex = 0;
      } else if (startIndex < 0) {
         startIndex = length + startIndex;
      } else if (startIndex > length) {
         startIndex = length;
      }

      if (endIndex < -length) {
         endIndex = 0;
      } else if (endIndex < 0)
         endIndex = length + endIndex;
      else if (endIndex > length)
         endIndex = length;

      const result = '';
      for (var i = startIndex; i < endIndex; i++) {
         result += this[i];
      }
      return result;
   };
}

// Extend String.padStart
if (!String.prototype.padStart) {
   String.prototype.padStart = function padStart(targetLength, padString) {
      targetLength = targetLength >> 0; // Truncate to integer, convert to number or 0
      padString = String(padString || ' ');
      if (this.length > targetLength) {
         return String(this);
      } else {
         targetLength = targetLength - this.length;
         if (targetLength > padString.length) {
            padString += padString.repeat(targetLength / padString.length); // Append to original to ensure target length
         }
         return padString.slice(0, targetLength) + String(this);
      }
   };
}

findConsecutiveRepeated = function(arr) {
  const result = [];
  var currentSequence = [];

  for (var i = 0; i < arr.length; i++) {
    if (i > 0 && arr[i] === arr[i - 1]) {
       currentSequence.push(arr[i]);
    } else {
      if (currentSequence.length > 0) {
        result.push(currentSequence);
      }
      currentSequence = [arr[i]];
    }
  }

  if (currentSequence.length > 1) {
    result.push(currentSequence);
  }

  return result;
}

findUniqueSeq = function(arr) {
   var result = [];
   var currentSequence = [];

   for (var i = 0; i < arr.length; i++) {
      if ((i == 0) || (arr[i] === arr[(i - 1)])) {
         continue;
      } else if (arr[i] !== arr[(i - 1)]) {
         currentSequence.push(arr[(i-1)]);

         if (i == (arr.length - 1)) {
            currentSequence.push(arr[i]);
         }

         var temp = result;
         result.push.apply(temp, currentSequence);
         currentSequence = Array();
      } 
   }

   // final array
   if (currentSequence.length > 1) {
      result.push(currentSequence);
   }

   return result;
}

// arrtoip(arr) → {string}nullable
// Convert the given input array of byte values to an IP address string.
// 
// Input arrays of length 4 are converted to IPv4 addresses, arrays of length 16 to IPv6 ones. All other lengths are rejected. If any array element is not an integer or exceeds the range 0..255 (inclusive), the array is rejected.
// 
// Returns a string containing the formatted IP address. Returns null if the input array was invalid.
/*
arrtoip = function(arr) {
   if (!Array.isArray(arr)) return null;

   if (arr.length == 4) {
      var str = "";
      for(var i = 0; i < arr.length; i++){
         if (!Number.isInteger(arr[i]) || arr[i]< 0 || arr[i]> 255) return null;
         if (i == 0) {
            str = str + arr[i];
         } else {
            str = str + "." + arr[i];
         }
      }
      return str;
   } else if (arr.length == 16) {
      const hexParts = [];
      for(var i = 0; i < arr.length; i++){
         if (!Number.isInteger(arr[i]) || arr[i]< 0 || arr[i]> 255) return null;
         var hex = arr[i].toString(16).padStart(2, '0');
         hexParts.push(hex);
      }

      const ipv6Parts = [];
      for (var i = 0; i < 16; i += 2) {
         var digit = hexParts[i] + hexParts[i + 1];

         var idx = rindex(digit,"0");
         if (idx < 0) {
            ipv6Parts.push(digit);
         } else if (idx == (digit.length - 1)){
            if (index(digit,"0") == idx) {
               ipv6Parts.push(digit);
            } else { 
               ipv6Parts.push("");
            }
         } else {
            ipv6Parts.push(substr(digit, idx+1));
         }
      }

      return findUniqueSeq(ipv6Parts).join(':');
   }
}
*/

// assert(cond, messageopt)
// Raise an exception with the given message parameter when the value in cond is not truish.
// 
// When message is omitted, the default value is Assertion failed.
// 
// Parameters:
// Name	Type	Description
// cond	*
// The value to check for truthiness.
// message	string	(optional)
// The message to include in the exception.
assert = function(condition, message) {
  if (!condition) {
    throw new Error(message || "Assertion failed");
  }
}

// type(x) → {string}nullable
// Query the type of the given value.
// 
// Returns the type of the given value as a string which might be one of "function", "object", "array", "double", "int", or "bool".
// 
// Returns null when no value or null is passed.
type = function(string) { 
   if (Array.isArray(string)) return "array"
   if (Number.isInteger(string)) return "int"
   if (Number(string) === string && string % 1 !== 0) return "double"
   return typeof string;     
}

// shift(arr) → {*}
//
// Pops the first item from the given array and returns it.
// 
// Returns null if the array was empty or if a non-array argument was passed.
//
shift = function(arr) { 
   if (!arr) return null;
   if (!Array.isArray(arr)) return null;

   return arr.splice(0, 1);     
}

//unshift(arr, …Values) → {*}
//Add the given values to the beginning of the array passed via first argument.

//Returns the last value added to the array.
unshift = function(arr) { 
   if (!arr) return null;
   if (!Array.isArray(arr)) return null;

   const originalLength = arr.length;
   const numElements = (arguments.length-1);

   // copy the original array
   var copy = Object.assign({}, arr);

   // expand the original array
   for (var i = 0; i < numElements; i++) {
      arr.push(0);
   }

   // shift originals in
   for (var i = originalLength - 1; i >= 0; i--) {
      arr[i + numElements] = copy[i];
   }

   // shift additionals in
   var last;
   for (var i = 0; i < numElements; i++) {
      arr[i] = arguments[i+1];
      last = arr[i];
   }

   return last;
}


// substr(str, off, lenopt) → {string}
// Extracts a substring out of str and returns it. First character is at offset zero.

// If off is negative, starts that far back from the end of the string.
// If len is omitted, returns everything through the end of the string.
// If len is negative, leaves that many characters off the string end.
// Returns the extracted substring.
substr = function(str, off, lenopt) { 
   var temp;

   if (lenopt < 0) {
      temp = str.substr(off, (str.length + lenopt - off));
   } else {
      temp = str.substr(off, lenopt);     
   }

   return temp;
}


// split(str, sep, limitopt) → {Array}
// Split the given string using the separator passed as the second argument and return an array containing the resulting pieces.
// If a limit argument is supplied, the resulting array contains no more than the given amount of entries, that means the string is split at most limit - 1 times total.
// The separator may either be a plain string or a regular expression.
// Returns a new array containing the resulting pieces.
split = function(str, sep, limitopt) { 
   return str.split(sep, limitopt);     
}


// reverse(arr_or_str) → {Array|string}nullable
// Reverse the order of the given input array or string.
// If an array is passed, returns the array in reverse order. If a string is passed, returns the string with the sequence of the characters reversed.
// Returns the reversed array or string. Returns null if neither an array nor a string were passed.
reverse = function (arr_or_str) {
   if (Array.isArray(arr_or_str))  {
      var result = new Array();
      for(var x = arr_or_str.length-1; x >= 0; x--) {
         result.push(arr_or_str[x]);
      }
      return result;
   } else {
      var result = '';
      for(var x = arr_or_str.length-1; x >= 0; x--) {
         result += arr_or_str[x];
      }
      return result;
   }
}

// index(arr_or_str, needle) → {number}nullable
// Finds the given value passed as the second argument within the array or string specified in the first argument.
// Returns the last matching array index or last matching string offset or -1 if the value was not found.
// Returns null if the first argument was neither an array nor a string.
index = function (arr_or_str, needle) {
   if (!Array.isArray(arr_or_str) && !(typeof arr_or_str === 'string')) return null;
   return arr_or_str.indexOf(needle);
}

// rindex(arr_or_str, needle) → {number}nullable
// Finds the given value passed as the second argument within the array or string specified in the first argument.
// Returns the last matching array index or last matching string offset or -1 if the value was not found.
// Returns null if the first argument was neither an array nor a string.
rindex = function (arr_or_str, needle) {

   if (Array.isArray(arr_or_str)) {
      for (var i = (arr_or_str.length - 1); i > 0; i--){
         if (arr_or_str[i] === needle) {
            return i;
         }
      }
   } else if (typeof arr_or_str === 'string') {
      for (var i = (arr_or_str.length - 1); i > 0; i--){
         if (substr(arr_or_str,i,needle.length) === needle) {
            return i;
         }
      }
   } else return null;

   return -1;
}
