/*
 copyright 2012 sam l'ecuyer
 */

var callbacks = {};

function on(e, callback) {
    if (!callbacks[e]) {
        callbacks[e] = [];
    }
    callbacks[e].push(callback);
}

function fire(ev) {
    if (callbacks[ev]) {
        callbacks[ev].forEach(function(cb) {
            try { cb(); } catch(e){}
        });
    }
}

exports.on = on;
exports.fire = fire;