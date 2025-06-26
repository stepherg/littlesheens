const WebSocket = require('ws');
const { JSONRPCServer } = require('json-rpc-2.0');
var rbus = null;
try {
   var rbus = require('./rbus');
} catch (e) {}

const wss = new WebSocket.Server({ port: 8080 });
const jsonRpcServer = new JSONRPCServer();

// Define JSON-RPC methods
jsonRpcServer.addMethod('echo', (params) => {
   return params;
});

jsonRpcServer.addMethod('add', (params) => {
   if (Array.isArray(params) && params.length === 2) {
      return params[0] + params[1];
   }
   throw new Error('Invalid parameters for add method. Expected two numbers.');
});

jsonRpcServer.addMethod('retrieve_t2_parameters', (params) => {
   var results = {}
   elements = params.Parameters;
   for (var i = 0; i < elements.length; i++) {
      var element = elements[i];
      if (element['type'] === 'dataModel') {
         console.log("Processing ", element);
         try {
            if (element.name)
               results[element.name] = rbus.getValue(element.reference);
            else
               results[element.reference] = rbus.getValue(element.reference);
         } catch (err) {
            console.error('getValue Error:', err.message);
            results[element.reference] = 'Unknown';
         }
      }
      if (element['type'] === 'event') {
         // TODO
      }
      if (element['type'] === 'grep') {
         // TODO
      }
   }
   return results;
   //throw new Error('Invalid parameters');
});

wss.on('connection', ws => {
   console.log('Client connected');

   ws.on('message', async message => {
      try {
         const jsonRpcRequest = JSON.parse(message.toString());
         const jsonRpcResponse = await jsonRpcServer.receive(jsonRpcRequest);
         if (jsonRpcResponse) {
            ws.send(JSON.stringify(jsonRpcResponse));
         }
      } catch (error) {
         console.error('Error processing message:', error);
         // Handle invalid JSON or other errors
         ws.send(JSON.stringify({
            jsonrpc: '2.0',
            error: { code: -32700, message: 'Parse error' },
            id: null // Or the id from the original request if possible
         }));
      }
   });

   ws.on('close', () => {
      console.log('Client disconnected');
   });

   ws.on('error', error => {
      console.error('WebSocket error:', error);
   });
});

console.log('JSON-RPC WebSocket server started on port 8080');

