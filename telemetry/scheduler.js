// Constructor function
function Scheduler() {
  this.tasks = [];
}

// Add a task with callback, delay, and optional repeat
Scheduler.prototype.addTask = function (callback, delay, repeat) {
  // Default repeat to false if undefined
  repeat = typeof repeat === 'undefined' ? false : repeat;

  // Create task object
  var task = {
    callback: callback,
    delay: delay,
    repeat: repeat,
    timerId: null,
    startTime: Date.now()
  };

  // Add to tasks array
  this.tasks.push(task);

  // Schedule task
  var self = this; // Capture this for timer callback
  if (repeat) {
    task.timerId = setInterval(function () {
      self.executeTask(task);
    }, delay);
  } else {
    task.timerId = setTimeout(function () {
      self.executeTask(task);
    }, delay);
  }

  return task;
};

// Execute a task
Scheduler.prototype.executeTask = function (task) {
  task.callback();
  if (!task.repeat) {
    this.removeTask(task);
  } else {
    task.startTime = Date.now(); // Update start time
  }
};

// Remove a task
Scheduler.prototype.removeTask = function (task) {
  if (task.repeat) {
    clearInterval(task.timerId);
  } else {
    clearTimeout(task.timerId);
  }

  // Remove task from array
  var newTasks = [];
  for (var i = 0; i < this.tasks.length; i++) {
    if (this.tasks[i] !== task) {
      newTasks.push(this.tasks[i]);
    }
  }
  this.tasks = newTasks;
};

// Clear all tasks
Scheduler.prototype.clearAllTasks = function () {
  for (var i = 0; i < this.tasks.length; i++) {
    this.removeTask(this.tasks[i]);
  }
  this.tasks = []; // Ensure array is empty
};

// CommonJS export for Duktape
if (typeof module !== 'undefined' && module.exports) {
  module.exports = Scheduler;
}
