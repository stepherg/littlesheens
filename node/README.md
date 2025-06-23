# littlesheens-rbus

A Node.js native addon for interacting with the RBus system, providing bindings to get/set properties and subscribe/unsubscribe to events. This project integrates with `node-littlesheens` and is built using `node-addon-api` for Node.js v18.19.1.

## Prerequisites

- **Node.js**: v18.19.1
- **RBus**: Ensure RBus libraries are installed (`librbus`, typically at `/usr/lib` or `/usr/local/lib`).
- **Build Tools**: `gcc`, `make`, `python3`, and `node-gyp`.
- **Dependencies**: `node-addon-api@5.0.0`, `safe-eval@0.4.1`, and `littlesheens` (local module).
- **Support Utilities**: To populate RBus, a utility like `rbus-tr181` can be used for development. https://github.com/stepherg/rbus-tr181

## Building the Project

To build the `littlesheens-rbus` project, follow these steps:

1. **Run Preinstall Script**:
   This generates the `node-littlesheens` module and prepares the build environment.
   ```bash
   npm run preinstall
   ```
   This executes `nodemodify.sh` and `build_specs.sh` to set up `node-littlesheens`.

2. **Install Dependencies and Build**:
   This installs dependencies (including `node-littlesheens`) and compiles the RBus native addon.
   ```bash
   npm install
   ```
   This runs `node-gyp rebuild` for the native addon and `npm install` in `node-littlesheens`.

## Usage

The RBus bindings provide four main functions: `rbusOpenHandle`, `rbusCloseHandle`, `getValue`, `setValue`, `subscribe`, and `unsubscribe`. Below are JavaScript examples demonstrating `getValue`, `setValue`, `subscribe`, and `unsubscribe`.

### JavaScript Examples

#### 1. `getValue`
Retrieve the value of an RBus property.

```javascript
const rbus = require('./rbus');

try {
   const value = rbus.getValue('Device.DeviceInfo.ModelName');
   console.log('ModelName:', value);
} catch (err) {
   console.error('getValue Error:', err.message);
}
```

#### 2. `setValue`
Set the value of an RBus property (string-based input).

```javascript
const rbus = require('./rbus');
try {
   rbus.setValue('Device.DeviceInfo.ModelName', 'TestModel');
   console.log('Set ModelName to TestModel');
} catch (err) {
   console.error('setValue Error:', err.message);
}
```

#### 3. `subscribe` (With `userData`)
Subscribe to an RBus event with optional `userData` and `timeout`.

```javascript
const rbus = require('./rbus');

try {
   const userData = {
      deviceId: 'device123',
      callbackCount: 0
   };

   rbus.setValue('Device.DeviceInfo.ModelName', 'TestModel');

   rbus.subscribe('Device.DeviceInfo.ModelName', (event, userData) => {
      userData.callbackCount++;
      console.log('Event received:', event);
      console.log('userData:', userData);
   }, userData, 0);
   console.log('Subscribed with userData');

   // Trigger event
   rbus.setValue('Device.DeviceInfo.ModelName', 'DebugModel');
} catch (err) {
   console.error('subscribe Error:', err.message);
}
```

#### 4. `subscribe` (Without `userData`)
Subscribe to an RBus event without `userData`.

```javascript
const rbus = require('./rbus');

try {
   rbus.setValue('Device.DeviceInfo.ModelName', 'TestModel');

   rbus.subscribe('Device.DeviceInfo.ModelName', (event) => {
      console.log('Event received:', event);
   });
   console.log('Subscribed without userData');

   // Trigger event
   rbus.setValue('Device.DeviceInfo.ModelName', 'DebugModel');
} catch (err) {
   console.error('subscribe Error:', err.message);
}
```

#### 5. `unsubscribe`
Unsubscribe from an RBus event.

```javascript
const rbus = require('./rbus');

try {
   rbus.unsubscribe('Device.DeviceInfo.ModelName');
   console.log('Unsubscribed from Device.DeviceInfo.ModelName');
} catch (err) {
   console.error('unsubscribe Error:', err.message);
}
```


## Troubleshooting

- **Build Errors**:
  - Ensure `node-addon-api@5.0.0`:
    ```bash
    npm list node-addon-api
    ```
  - Verify RBus libraries:
    ```bash
    ls /usr/lib | grep rbus
    ```
  - Check `binding.gyp` for correct paths.

- **Runtime Errors**:
  - Set `LD_LIBRARY_PATH`:
    ```bash
    export LD_LIBRARY_PATH=/usr/lib:/usr/local/lib:$LD_LIBRARY_PATH
    ```
  - Check logs for N-API or RBus errors:
    ```bash
    grep "N-API Error\|rbus error" stderr.log
    ```

- **Event Issues**:
  - Ensure the RBus property (e.g., `Device.DeviceInfo.ModelName`) exists:
    ```bash
    rbuscli get Device.DeviceInfo.ModelName
    ```
