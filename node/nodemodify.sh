#!/bin/bash

# This goofy script attempts to generate a Node module based on the
# Javascript sources in 'js/'.
#
# Uses https://www.npmjs.com/package/safe-eval.
#
# After running this script, try
#
#   sudo npm link node-littlesheens
#
# Then
#
#   const SHEENS = require('littlesheens');
#

set -e

TARGET=node-littlesheens
mkdir -p $TARGET

cat<<EOF > $TARGET/index.js
function print(...x) {
    console.log(...x);
}

var safeEval = require('safe-eval');

var sandbox = function(code) {
    var context = {
      print: function (...args) { console.log(...args); }
    }
    return safeEval(code);
}
EOF

for F in prof match sandbox step; do 
#   sed -e 's/print(/console.log(/g' ../js/$F.js >> $TARGET/index.js
   cat ../js/$F.js >> $TARGET/index.js
done

cat<<EOF >> $TARGET/index.js
exports.step = step;
exports.walk = walk;
exports.match = match;
exports.action = sandboxedAction;
exports.times = Times;
EOF

cat<<EOF > $TARGET/package.json
{
  "name": "littlesheens",
  "version": "1.0.0",
  "description": "Little Sheens",
  "main": "index.js",
  "scripts": {
    "test": "echo \"Everything is perfect\""
  },
  "author": "",
  "license": "ISC"
}
EOF

#echo "Wrote $TARGET"
#echo "Now run: 'npm link ./node-littlesheens'"


