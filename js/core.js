test = function() {
    return {
       assert: function(val) {
          assert(val);
       }
    }
};

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
   print(arguments.length)
   if (!arr) return null;
   if (!Array.isArray(arr)) return null;

   const originalLength = arr.length;
   const numElements = (arguments.length-1);

   print(originalLength, numElements);

   for (var i = 0; i < numElements; i++) {
      arr.push(0);
   }

   for (var i = originalLength - 1; i >= 0; i--) {
      arr[i + numElements] = arr[i];
   }

   for (var i = 0; i < numElements; i++) {
      arr[i] = arguments[i];
   }

   arr.length = originalLength + numElements;
   return arr.length;
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
