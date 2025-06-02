// Constructor function
function Trigger() {
  this.triggers = [];
}

// Add a trigger with callback and conditions
Trigger.prototype.addTrigger = function (callback, conditions) {
  var trigger = {
    callback: callback,
    conditions: conditions,
    startTime: Date.now()
  };

  // Initialize each condition
  for (var i = 0; i < trigger.conditions.length; i++) {
    var condition = trigger.conditions[i];
    condition.timerId = null;
    condition.lastTriggerTime = 0;
    // Simulate trigger with setInterval
    var self = this; // Capture this for timer callback
    condition.timerId = setInterval(function () {
      self.executeTrigger(trigger);
    }, 5000);
  }

  this.triggers.push(trigger);

  return trigger;
};

// Execute a trigger
Trigger.prototype.executeTrigger = function (trigger) {
  // Process each condition (stop at first valid one)
  for (var i = 0; i < trigger.conditions.length; i++) {
    var condition = trigger.conditions[i];
    if (condition.minThresholdDuration) {
      if ((Date.now() - condition.lastTriggerTime) < condition.minThresholdDuration * 1000) {
        console.log('minThresholdDuration not met');
        return;
      }
    }
    condition.lastTriggerTime = Date.now(); // Update start time
    trigger.callback();
    return;
  }
};

// Remove a trigger
Trigger.prototype.removeTrigger = function (trigger) {
  // Clear all condition timers
  for (var i = 0; i < trigger.conditions.length; i++) {
    var condition = trigger.conditions[i];
    clearInterval(condition.timerId);
  }

  // Remove trigger from array
  var newTriggers = [];
  for (var i = 0; i < this.triggers.length; i++) {
    if (this.triggers[i] !== trigger) {
      newTriggers.push(this.triggers[i]);
    }
  }
  this.triggers = newTriggers;
};

// Clear all triggers
Trigger.prototype.clearAllTriggers = function () {
  for (var i = 0; i < this.triggers.length; i++) {
    this.removeTrigger(this.triggers[i]);
  }

  // Need to debug why all triggers aren't removed after the first loop
  if (this.triggers.length > 0) {
    for (var i = 0; i < this.triggers.length; i++) {
      this.removeTrigger(this.triggers[i]);
    }  
  }

  this.triggers = []; // Ensure array is empty
};

// CommonJS export for Duktape
if (typeof module !== 'undefined' && module.exports) {
  module.exports = Trigger;
}