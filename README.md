# A minimalist C library for working with Sheens

## Summary

A minimalist [Sheens](https://github.com/Comcast/sheens)
implementation in ECMAScript that's executed by
[Duktape](http://duktape.org/) and wrapped in a C library.  Yes, that
does sound a little odd.

The stripped 32-bit demo executable is 360KB.  The core ECMAScript
code (un-minified) is less than 40KB (11KB minified).  An experimental
[nodejs](https://nodejs.org/en/) module is available (via `make
nodejs`).

See that [Sheens repo](https://github.com/Comcast/sheens) for more
documentation about these machines.

## License

This repo is licensed under [Apache License 2.0](LICENSE).

## API

See `machines.h`.

## A demo

For now, running the demo requires two tools written in
[Go](https://golang.org/), so you need Go installed to build this
demo.  (We'll remove this unncessary dependency soon.)

```Shell
go install github.com/bronze1man/yaml2json@latest
go install github.com/tdewolff/minify/cmd/minify@latest
```

If Go isn't already in your default path, you can do the following to add it to your current shell

```Shell
export PATH=$PATH:$(go env GOPATH)/bin
```

To add it to your user's default path:

```Shell
echo PATH=$PATH:$(go env GOPATH)/bin >> <location of .profile or .zprofile for MAC>

# Usually .profile on Linux
echo 'PATH=$PATH:'$(go env GOPATH)/bin >> ~/.profile

# Usually .zprofile on a Mac
echo 'PATH=$PATH':$(go env GOPATH)/bin >> ~/.zprofile

```

Building on Linux

```Shell
make
./demo
```

Building on a MAC 

```Shell
make -f Makefile.osx
./demo
```

## Another demo

This demo is a simple process that reads messages from `stdin` and
writes output to `stdout`.

```Shell
make specs/turnstile.js specs/double.js sheensio

# Define a "crew" of two machines.
cat<<EOF > crew.json
{"id":"simpsons",
 "machines":{
   "doubler":{"spec":"specs/double.js","node":"listen","bs":{"count":0}},
   "turnstile":{"spec":"specs/turnstile.js","node":"locked","bs":{}}}}
EOF

# Send messages to that crew.
cat<<EOF | ./sheensio -d
{"double":1}
{"double":10}
{"double":100}
{"input":"push"}
{"input":"coin"}
{"input":"coin"}
{"input":"push"}
EOF
```

The above is in `demo.sh`.


## Yet another demo

This demo shows how to do some primitive Sheens work from Javascript
(`demo.js`).

```Shel
make demo
./demo driver.js demo.js
```

## Utilities

The `driver` executable will execute (`mach_eval`) code in files given
on the command line.  The environment includes what's in the directory
`js` and in the file `driver.js`, so you can experiment directly with
those functions.  For example, if the file `check.js` contains

```Javascript
JSON.stringify(match(null, {"likes":"?x"}, {"likes":"tacos"}, {}));
```

Then

```Shell
./driver check.js
```

should write

```Javascript
[{"?x":"tacos"}]
```

Per the documentation for `mach_eval`, the code that's executed should
return a string.


## Discussion

### Routing

`driver.js` implements optional routing that can present a message
only to machines specified in that message.  If a message has a `"to"`
property, the value should be a string or an array of strings.  The
message is then only presented to the machine or machines with those
ids.

For example, `{"to":"doubler","double":1}` will only be sent to a
machine with id `doubler` (if it exists).  The message
`{"to":["this","that"],...}` will only be presented to the machines
with ids `this` and `that` (if they exist).  Note that the entire
message (including the `"to"` property) is still present to the target
machines.

### Nodejs

Little Sheens has some crude support for [Node.js](https://nodejs.org/en/).

```Shell
make nodejs
```

That command might make the Node module `node-littlesheens`.  [This
example](sheens.ipynb) [Jupyter](http://jupyter.org/) notebook
demonstrates a little of the available functionality.

### "Lua programs can also be little"

A [start](misc/match.lua) at a [Lua](https://www.lua.org/)-based
Little Sheens?  (Should end up in another repo if worthy.)

## docker

Use the following to set up docker container for dev environment.

```
docker build -t gears/littlesheens:latest .
docker run -i -t -v $HOME/go:/go gears/littlesheens:latest /bin/bash
```

## Code of Conduct

We take our [code of conduct](CODE_OF_CONDUCT.md) seriously. Please
abide by it.


## Contributing

Please read our [contributing guide](CONTRIBUTING.md) for details on
how to contribute to our project.


## References

1. [Sheens](https://github.com/Comcast/sheens)
1. [Duktape](http://duktape.org/)
