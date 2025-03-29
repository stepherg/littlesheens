/*
print();
print("assert: ");
assert(true);

test().assert(true);

*/

// f - function
// i - inputs
// w - want
var tests = [

   {
      "title": "arrtoip([192, 168, 1, 1])",
      "f": arrtoip,
      "i": [[192, 168, 1, 1]],
      "w": '192.168.1.1',
      "doc": ""
   },
   {
      "title": "arrtoip([1, 2, 3])",
      "f": arrtoip,
      "i": [[ 1, 2, 3]],
      "w": null,
      "doc": ""
   },
   {
      "title": "arrtoip([192, '2', -5, 300])",
      "f": arrtoip,
      "i": [[ 1, "2", -5, 300]],
      "w": null,
      "doc": ""
   },
   {
      "title": "arrtoip([192, '2', 1, 1])",
      "f": arrtoip,
      "i": [[ 192, "2", 1, 1]],
      "w": null,
      "doc": ""
   },
   {
      "title": "arrtoip([192, 168, -5, 1])",
      "f": arrtoip,
      "i": [[ 192, "2", 1, 1]],
      "w": null,
      "doc": ""
   },
   {
      "title": "arrtoip([192, 168, 1, 300])",
      "f": arrtoip,
      "i": [[ 192, "2", 1, 1]],
      "w": null,
      "doc": ""
   },
   {
      "title": "arrtoip('123')",
      "f": arrtoip,
      "i": ["123"],
      "w": null,
      "doc": ""
   },
   {
      "title": "arrtoip([ 254, 128, 0, 0, 0, 0, 0, 0, 252, 84, 0, 255, 254, 130, 171, 189 ])",
      "f": arrtoip,
      "i": [[ 254, 128, 0, 0, 0, 0, 0, 0, 252, 84, 0, 255, 254, 130, 171, 189 ]],
      "w": "fe80::fc54:ff:fe82:abbd",
      "doc": ""
   },
   {
      "title": "type(function)",
      "f": type,
      "i": [type],
      "w": 'function',
      "doc": ""
   },
   {
      "title": "type(string)",
      "f": type,
      "i": ["test"],
      "w": 'string',
      "doc": ""
   },
   {
      "title": "type(array)",
      "f": type,
      "i": [[1,2,3]],
      "w": 'array',
      "doc": ""
   },
   {
      "title": "type(boolean)",
      "f": type,
      "i": [true],
      "w": 'boolean',
      "doc": ""
   },
   {
      "title": "type(int)",
      "f": type,
      "i": [1],
      "w": 'int',
      "doc": ""
   },
   {
      "title": "type(double)",
      "f": type,
      "i": [1.1],
      "w": 'double',
      "doc": ""
   },
   {
      "title": "shift([1,2,3])",
      "f": shift,
      "i": [[1,2,3]],
      "w": [1],
      "doc": ""
   },
   {
      "title": "shift(null)",
      "f": shift,
      "i": [null],
      "w": null,
      "doc": ""
   },
   {
      "title": "substr('The black cat climbed the green tree, 4, 5')",
      "f": substr,
      "i": ["The black cat climbed the green tree", 4, 5],
      "w": "black",
      "doc": "Test indexing string from the beginning with limited characters"
   },
   {
      "title": "substr('The black cat climbed the green tree, 4, -11')",
      "f": substr,
      "i": ["The black cat climbed the green tree", 4, -11],
      "w": "black cat climbed the",
      "doc": "Test indexing string from the beginning and end"
   },
   {
      "title": "substr('The black cat climbed the green tree, 14')",
      "f": substr,
      "i": ["The black cat climbed the green tree", 14],
      "w": "climbed the green tree",
      "doc": "Test grabbing limited characters from the beginning of the string"
   },
   {
      "title": "substr('The black cat climbed the green tree, 4, 5')",
      "f": substr,
      "i": ["The black cat climbed the green tree", -4],
      "w": "tree",
      "doc": "Test indexing string from the end"
   },
   {
      "title": "substr('The black cat climbed the green tree, 4, 5')",
      "f": substr,
      "i": ["The black cat climbed the green tree", -4, 2],
      "w": "tr",
      "doc": "Test indexing string from the end with limited characters"
   },
   {
      "title": "split('1,2,3',',')",
      "f": split,
      "i": ["1,2,3",","],
      "w": ["1", "2", "3"],
      "doc": ""
   },
   {
      "title": "split('1,2,3',',',2)",
      "f": split,
      "i": ["1,2,3",",",2],
      "w": ["1", "2"],
      "doc": ""
   },
   {
      "title": "reverse([1,2,3])",
      "f": reverse,
      "i": [[1, 2, 3]],
      "w": [3, 2, 1],
      "doc": ""
   },
   {
      "title": "reverse([1,2,3])",
      "f": reverse,
      "i": ["Abc"],
      "w": "cbA",
      "doc": ""
   },
   {
      "title": "index('Hello hello hello', 'll')",
      "f": index,
      "i": ["Hello hello hello", "ll"],
      "w": 2,
      "doc": ""
   },
   {
      "title": "index('[ 1, 2, 3, 1, 2, 3, 1, 2, 3 ], 2)",
      "f": index,
      "i": [[ 1, 2, 3, 1, 2, 3, 1, 2, 3 ], 2],
      "w": 1,
      "doc": ""
   },
   {
      "title": "index('foo', 'bar')",
      "f": index,
      "i": ["foo", "bar"],
      "w": -1,
      "doc": ""
   },
   {
      "title": "index([['Red', 'Blue', 'Green'], 'Brown')",
      "f": index,
      "i": [["Red", "Blue", "Green"], "Brown"],
      "w": -1,
      "doc": ""
   },
   {
      "title": "index(123)",
      "f": index,
      "i": [123,2],
      "w": null,
      "doc": ""
   },
   {
      "title": "rindex('Hello hello hello', 'll')",
      "f": rindex,
      "i": ["Hello hello hello", "ll"],
      "w": 14,
      "doc": ""
   },
   {
      "title": "rindex('[ 1, 2, 3, 1, 2, 3, 1, 2, 3 ], 2)",
      "f": rindex,
      "i": [[ 1, 2, 3, 1, 2, 3, 1, 2, 3], 2],
      "w": 7,
      "doc": ""
   },
   {
      "title": "rindex('foo', 'bar')",
      "f": rindex,
      "i": ["foo", "bar"],
      "w": -1,
      "doc": ""
   },
   {
      "title": "rindex('foo', 'bar')",
      "f": rindex,
      "i": [["Red", "Blue", "Green"], "Brown"],
      "w": -1,
      "doc": ""
   },
   {
      "title": "rindex(123)",
      "f": rindex,
      "i": [123,2],
      "w": null,
      "doc": ""
   },
   {
      "title": "unshift([3,4,5], 1, 2)",
      "f": unshift,
      "i": [[ 3, 4, 5], 1, 2],
      "w": 2,
      "doc": ""
   }
]

print(run_tests(tests));
