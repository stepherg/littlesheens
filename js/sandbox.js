/* Copyright 2018 Comcast Cable Communications Management, LLC
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// sandboxedAction wishes to be a function that can evaluate
// ECMAScript source in a fresh, pristine, sandboxed environment.
//
// Returns {bs: BS, emitted: MESSAGES}.
function sandboxedAction(ctx, bs, src) {
   // This function calls a (presumably primitive) 'sandbox' function
   // to do the actual work.  That function is probably in
   // 'machines.c'.

   // ToDo: Different env for guards: no emitting.
   Times.tick("sandbox");

   if (!bs) {
      bs = {};
   }

   var bs_js = JSON.stringify(bs);

   var code = "\n" +
      "var emitting = [];\n" +
      "var debug_logging = [];\n" +
      "var env = {\n" +
      "  bindings: " + bs_js + ",\n" +  // Maybe JSON.parse.
      "  genRandomId: function(length) { return Math.random().toString(36).substring(2, length + 2); },\n" +
      "  generateRandomInt: function(min,max) {\n" +
      "      min = Math.ceil(min);\n" +
      "      max = Math.floor(max);\n" +
      "      return Math.floor(Math.random() * (max - min + 1)) + min;\n" +
      "  },\n" +
      "  generateRandomString: function(length) {\n" +
      "     var result = '';\n" +
      "     var characters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';\n" +
      "     var charactersLength = characters.length;\n" +
      "     for (var i = 0; i < length; i++) {\n" +
      "        result += characters.charAt(Math.floor(Math.random() * charactersLength));\n" +
      "     }\n" +
      "     return result;\n" +
      "     },\n" +
      "  log: function () {var args = Array.prototype.slice.call(arguments); debug_logging.push.apply(debug_logging, args);},\n" +
      "  rbus_getValue: function(path) { return env.generateRandomString(10); },\n" +
      "  out: function(x) { emitting.push(x); }\n" +
      "}\n" +
      "\n" +
      "var bs = (function(_) {\n" + src + "\n})(env);\n";

   // The following conditional checks for 'safeEval', might have
   // been defined by https://www.npmjs.com/package/safe-eval.  
   if (typeof safeEval === 'undefined') { // Just for ../nodemodify.sh
      //  Duktape-based sandbox just takes a code block.
      code += "JSON.stringify({bs: bs, debug_logging: debug_logging, emitted: emitting});\n";
   } else {
      // 'safeEval' wants an expression.  Build a called function
      code = "function() {\n" + code + "\n" +
         "return JSON.stringify({bs: bs, debug_logging: debug_logging, emitted: emitting});\n" +
         "}();\n";
   }

   try {
      var result_js = sandbox(code);
      try {
         var result = JSON.parse(result_js);
         for (var i = 0; i < result.debug_logging.length; i++) {
            print("**SANDBOX**: " + result.debug_logging[i]);
         }
         return result;
      } catch (e) {
         throw e + " on result parsing of '" + result_js + "'";
      }
   } catch (e) {
      print("walk action sandbox error", e);
      // Make a binding for the error so that branches could deal
      // with the error.
      //
      // ToDo: Do not overwrite?
      //
      // ToDo: Implement the spec switch that enabled
      // branching-based action error-handling.
      bs.error = e;
      return {bs: bs, error: e};
   } finally {
      Times.tock("sandbox");
   }
}

function sandboxedStatement(ctx, bs, src) {
   Times.tick("sandboxStatement");

   try {
      if (typeof safeEval === 'undefined') { // Just for ../nodemodify.sh
         // DUKTAPE
         var bs_js = JSON.stringify(bs);
         var code_block = "var result= (function(_) {\nreturn _." + src + "\n})(" + bs_js + ");\n";
         code_block += "JSON.stringify(result);\n";
         var result_js = sandbox(code_block);
         return JSON.parse(result_js);
      } else {
         // NODE SAFEEVAL
         return sandbox(src, bs);
      }

   } catch (e) {
      print("sandbox statement error", e);
   } finally {
      Times.tock("sandboxStatement");
   }
}
