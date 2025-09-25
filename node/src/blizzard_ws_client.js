const WebSocket = require('ws');
const {EventEmitter} = require('events');

function genRandomId() {
   return Math.random().toString(36).slice(2);
}

class BlizzardWsClient extends EventEmitter {
   constructor(url, opts = {}) {
      super();
      this.url = url;
      this.token = opts.token;
      this.ws = null;
      this.pending = new Map(); // jsonrpc id -> { resolve, reject }
      this.connected = false;
   }

   async connect() {
      if (this.connected && this.ws && this.ws.readyState === WebSocket.OPEN) return;
      const headers = {};
      if (this.token) headers['Authorization'] = `Bearer ${this.token}`;
      this.ws = new WebSocket(this.url, {headers});

      await new Promise((resolve, reject) => {
         const onOpen = () => {
            this.connected = true;
            this.emit('open');
            cleanup();
            resolve();
         };
         const onError = (err) => {
            this.emit('error', err);
            cleanup();
            reject(err);
         };
         const cleanup = () => {
            this.ws.off('open', onOpen);
            this.ws.off('error', onError);
         };
         this.ws.on('open', onOpen);
         this.ws.on('error', onError);
      });

      // Wire passthrough events + JSON-RPC routing
      this.ws.on('message', (data) => {
         let msg = null;
         try {
            msg = JSON.parse(data.toString());
         } catch (e) {
            this.emit('error', new Error(`Invalid JSON from server: ${e.message}`));
            return;
         }
         // Forward raw event for generic consumers (keeps compatibility with index.js)
         this.emit('message', data);
         // Resolve request/response promises
         if (msg && Object.prototype.hasOwnProperty.call(msg, 'id')) {
            const entry = this.pending.get(msg.id);
            if (entry) {
               this.pending.delete(msg.id);
               if (Object.prototype.hasOwnProperty.call(msg, 'error') && msg.error) {
                  entry.reject(msg.error);
               } else {
                  entry.resolve(msg.result);
               }
            }
         } else {
            // notification: surface as 'notification'
            this.emit('notification', msg);
         }
      });

      this.ws.on('close', () => {
         this.connected = false;
         this.emit('close');
      });
      this.ws.on('error', (err) => this.emit('error', err));
   }

   close() {
      if (this.ws) this.ws.close();
   }

   // Send raw JSON or object (maintains compatibility with existing harness)
   send(payload) {
      if (!this.ws || this.ws.readyState !== WebSocket.OPEN) throw new Error('WebSocket not open');
      const data = typeof payload === 'string' ? payload : JSON.stringify(payload);
      this.ws.send(data);
   }

   request(method, params = {}, id = undefined) {
      const jsonrpcId = id ?? genRandomId();
      const req = {jsonrpc: '2.0', method, params, id: jsonrpcId};
      return new Promise((resolve, reject) => {
         this.pending.set(jsonrpcId, {resolve, reject});
         this.send(req);
      });
   }

   // Convenience helpers
   subscribe({id, provider, event}) {
      return this.request('rpc.Subscribe', {id, provider, event});
   }

   unsubscribe({id, provider, event}) {
      return this.request('rpc.Unsubscribe', {id, provider, event});
   }
}

module.exports = {BlizzardWsClient};
