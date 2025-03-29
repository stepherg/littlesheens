/*
print();
print("assert: ");
assert(true);

test().assert(true);

print();
print("type: ");
f = function() { 
};

print("function: ", type(f));
print("object: ", type({}));
print("array: ", type([1,2,3]));
print("string: ", type("string"));
print("int: ", type(1));
print("float: ", type(3.14));
print("bool: ", type(true));
   
print();
print("shift: ");
var x = [ 1, 2, 3 ];
print(shift(x));        // 1
print(x, "\n");  // [ 2, 3 ]
print(shift(null));

print();
print("substr: ");
var s = "The black cat climbed the green tree";
print(substr(s, 4, 5));      // black
print(substr(s, 4, -11));    // black cat climbed the
print(substr(s, 14));        // climbed the green tree
print(substr(s, -4));        // tree
print(substr(s, -4, 2));     // tr


print(split("1,2,3", ","));
print(split("1,2,3", ",", 2));

print();
print("reverse: ");

// reverse
print(reverse([1, 2, 3]));   // [ 3, 2, 1 ]
print(reverse("Abc"));       // "cbA"

print();
print("index: ");
// index
print(index("Hello hello hello", "ll"))          // 2
print(index([ 1, 2, 3, 1, 2, 3, 1, 2, 3 ], 2))   // 1
print(index("foo", "bar"))                       // -1
print(index(["Red", "Blue", "Green"], "Brown"))  // -1
print(index(123, 2))                             // null

print();
print("rindex: ");
print(rindex("Hello hello hello", "ll"))          // 14
print(rindex([ 1, 2, 3, 1, 2, 3, 1, 2, 3 ], 2))   //  7
print(rindex("foo", "bar"))                       // -1
print(rindex(["Red", "Blue", "Green"], "Brown"))  // -1
print(rindex(123, 2))                             // null

print();
print("unshift: ");
var x = [ 3, 4, 5 ];
print(unshift(x, 1, 2));  // 2
print(x, "\n");    // [ 1, 2, 3, 4, 5 ]
*/


// w - want
// w - want
var tests = [
   {
      "title": "type(function)",
      "f": type,
      "i": type,
      "w": 'function',
      "doc": ""
   },
   {
      "title": "type(string)",
      "f": type,
      "i": "test",
      "w": 'string',
      "doc": ""
   },
   {
      "title": "type(array)",
      "f": type,
      "i": [1,2,3],
      "w": 'array',
      "doc": ""
   },
   {
      "title": "type(boolean)",
      "f": type,
      "i": true,
      "w": 'boolean',
      "doc": ""
   },
   {
      "title": "type(int)",
      "f": type,
      "i": 1,
      "w": 'int',
      "doc": ""
   },
   {
      "title": "type(double)",
      "f": type,
      "i": 1.1,
      "w": 'double',
      "doc": ""
   },
   {
      "title": "shift([1,2,3])",
      "f": shift,
      "i": [1,2,3],
      "w": [1],
      "doc": ""
   },
   {
      "title": "shift(null)",
      "f": shift,
      "i": null,
      "w": null,
      "doc": ""
   },
   {
      "title": "substr('The black cat climbed the green tree, 4, 5')",
      "f": substr,
      "args": ["The black cat climbed the green tree", 4, 5],
      "w": "black",
      "doc": ""
   }
//print(substr(s, 4, 5));      // black
//var s = "The black cat climbed the green tree";
//print(substr(s, 4, 5));      // black
//print(substr(s, 4, -11));    // black cat climbed the
//print(substr(s, 14));        // climbed the green tree
//print(substr(s, -4));        // tree
//print(substr(s, -4, 2));     // tr
]

run_tests(tests);
