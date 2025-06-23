const addon = require('./build/Release/rbus_binding');

// Initialize rbus handle
const handle = addon.rbusOpenHandle();
if (!handle) {
   throw new Error('Failed to open rbus');
}
addon.rbusHandle = handle;

process.on('exit', () => {
   addon.rbusCloseHandle && addon.rbusCloseHandle();
});

module.exports = addon;