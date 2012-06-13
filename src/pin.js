"use strict";

global.Pin = new $Object();

var $Pin = global.Pin;

$Pin.PIN_GetPid = function() {
    return %_PIN_GetPid();
}

$Pin.PIN_Test = function(arg) {
    return %PIN_Test(arg);
}
