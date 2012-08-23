/*
 copyright 2012 sam l'ecuyer
*/

var assert = exports;
function AssertionError(err) {
  this.message  = err.message;
  this.actual   = err.actual;
  this.expected = err.expected;
  this.thrower = err.thrower || fail;
  Error.captureStackTrace(this, this.thrower);
};

AssertionError.prototype = new Error();
AssertionError.__proto__ = Error.prototype;
AssertionError.prototype.constructor = AssertionError;
AssertionError.prototype.toString = function() {
    return  'AssertionError: \n'+
            '\texpected: \t'+this.expected+' \n'+
            '\tactual: \t'+this.actual+' \n'+
            //'\tthrower: \t'+this.thrower+' \n'+
            '\tmessage: \t'+this.message+' \n';
};

assert.AssertionError = AssertionError;

assert.fail = function(message, expected, actual, thrower) {
    throw new AssertionError({ message: message, 
                               actual: actual, 
                               expected: expected,
                               thrower: thrower });
}

assert.ok = function(guard, message) {
    if (!!guard) return;
    exports.fail(message, true, !!guard, assert.ok);
}

assert.equal = function(actual, expected, message) {
    if (actual == expected) return;
    exports.fail(message, expected, actual, assert.equal);
}

assert.notEqual = function(actual, expected, message) {
    if (actual != expected) return;
    exports.fail(message, expected, actual, assert.notEqual);
}

assert.strictEqual = function(actual, expected, message) {
    if (actual === expected) return;
    exports.fail(message, expected, actual, assert.strictEqual);
}

assert.strictNotEqual = function(actual, expected, message) {
    if (actual !== expected) return;
    exports.fail(message, expected, actual, assert.strictNotEqual);
}

assert.throws = function(block, message) {
    var actual;
    try {
        block();
    } catch(e) {
        return;
    }
    assert.fail('', 'should have thrown an exception', 'didnt', assert.throws);
}
