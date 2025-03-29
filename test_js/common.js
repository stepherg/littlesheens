function canonicalBs(bs) {
   var ps = [];
   for (var p in bs) {
      ps.push(p);
   }
   ps.sort();
   var acc = [];
   for (var i = 0; i < ps.length; i++) {
      var p = ps[i];
      var v = bs[p];
      acc.push([p,v]);
   }
   return acc;
}

function canonicalBss(bss) {
   if (!bss) {
      return "null";
   }
   var acc = [];
   for (var i = 0; i < bss.length; i++) {
      var bs = bss[i];
      acc.push(JSON.stringify(canonicalBs(bs)));
   }
   acc.sort();
   return JSON.stringify(acc);
}

function run_tests(tests) {
   var acc = [];

   for (var i = 0; i < tests.length; i++) {
      var test = tests[i];
      var result = {"n": i+1, "case": test};
      if (test.b === undefined) {
         test.b = {};
      }
      acc.push(result);

      try {
         try {
            // Benchmark
            var rounds = 1000;
            result.bench = {rounds: null, elapsed: null};
            var then = Date.now();
            for (var b = 0; b < rounds; b++) {
               // make a copy of input params in case the function manipulates
               var f = test.f;
               var args;
               if (test.args=== undefined) {
                  args = Object.assign({}, test.i);
                  test.f(args);
               } else {
                  args = test.args;
                  test.f.apply(null, args);
               }
            }
            result.bench.rounds = rounds;
            result.bench.elapsed = Date.now() - then;
         } catch (e) {
         }

         if (test.benchmarkOnly) {
            continue;
         }

         if (type(test.f) === 'function') {
            var f = test.f;
            var args;
            var bss;
            if (test.args === undefined) {
               bss = f(test.i);
            } else {
               bss = f.apply(null, test.args);
            }
            result.bss = bss;
            result.happy = canonicalBss(bss) == canonicalBss(test.w);
            result.got = canonicalBss(bss);
            result.wanted = canonicalBss(test.w);
            continue;
         }

         result.happy = false;
         result.err = "no function for test";

      } catch (e) {
         if (!test.err) {
            print("" + e);
            result.err = "" + e;
            result.happy = false;
         }
      }
   }

   return JSON.stringify(acc);
}
