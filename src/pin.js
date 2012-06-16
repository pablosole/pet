"use strict";

global.Pin = new $Object();

var $Pin = global.Pin;

function PIN_GetPid() {
    return %_PIN_GetPid();
}

function PIN_Test(arg) {
    return %PIN_Test(arg);
}

function SetupPIN() {
  %CheckIsBootstrapping();
  InstallFunctions($Pin, DONT_ENUM, $Array(
    "getPid", PIN_GetPid,
    "test", PIN_Test
  ));
}

SetupPIN();
