"use strict";

global.current = new $Object();
global.process = new $Object();
global.images = new $Object();
global.threads = new $Object();
global.events = new $Object();
global.routines = new $Object();
global.control = new $Object();
global.utils = new $Object();

global.Semaphore = new $Function();
global.Trace = new $Function();
global.Image = new $Function();

var $current = global.current;
var $process = global.process;
var $images = global.images;
var $threads = global.threads;
var $events = global.events;
var $routines = global.routines;
var $control = global.control;
var $utils = global.utils;

var $Semaphore = global.Semaphore;
var $Trace = global.Trace;
var $Image = global.Image;

/* current object support */
function PIN_GetPid() {
    return %_PIN_GetPid();
}

function PIN_GetTid() {
    return %_PIN_GetTid();
}

function PIN_ThreadId() {
    return %_PIN_ThreadId();
}

function PIN_ThreadUid() {
    return %_PIN_ThreadUid();
}

/* process object support */
function PIN_IsProcessExiting() {
    return %_PIN_IsProcessExiting();
}

function PIN_ExitProcess(s) {
    %_PIN_ExitProcess(s);
}

function PIN_ExitApplication(s) {
    %_PIN_ExitApplication(s);
}

/* control object support */
function PIN_Yield() {
    %_PIN_Yield();
}

function PIN_Sleep(t) {
    %_PIN_Sleep(t);
}

/* ExternalPointer support functions */
function ExternalPointerAssert() {
  if (!this.valid)
    throw "Invalid " + this.type;
}

function ExternalPointerDestroy() {
  this.assert();
  if (IS_FUNCTION(this._destructor))
    this._destructor(this.external);
  this.external=null;
  this.valid=false;  
}

/* Image object */
function IMG_FindByAddress(addr) {
    return new $Image(%_IMG_FindByAddress(addr));
}

function IMG_FindImgById(id) {
    return new $Image(%_IMG_FindImgById(id));
}

var $ImageList = new $Function();
var $image_opened = false;

function ImageStaticOpen(name) {
    if ($image_opened)
        throw "It can be only one statically opened image";
    
    $image_opened = true;
    var img = new $Image(%_IMG_Open(name));
    if (!img.valid)
        $image_opened = false;
    else
        img._destructor = function () { $image_opened = false; }
    
    return img;
}

var $ImageInvalidAddr = %_IMG_Invalid();
function ImageConstructor(addr) {
    if (!%_IsConstructCall()) {
        return new $Image(addr);
    }
    
    if (addr == $ImageInvalidAddr) {
        this.external = null;
        this.valid = false;
    } else {
        this.external = new global.ExternalPointer(addr);
        this.valid = true;
    }
}

function IMG_Next() {
    this.assert();
    return new $Image(%_IMG_Next(this.external));
}

function IMG_Prev() {
    this.assert();
    return new $Image(%_IMG_Prev(this.external));
}

function IMG_SecHead() {
    this.assert();
    return new $Section(%_IMG_SecHead(this.external));
}

function IMG_SecTail() {
    this.assert();
    return new $Section(%_IMG_SecTail(this.external));
}


/* Trace object */
function TraceConstructor() {
  if (!%_IsConstructCall()) {
    return new $Trace();
  }

  this.external = {};
  this.valid = false;
  this.type = "Trace";
}

function TRACE_Original() {
    this.assert();
    return %_TRACE_Original(this.external);
}

/* Semaphore object */
function PIN_SemaphoreFini(sem) {
    if (!IS_SPEC_OBJECT(sem))
      throw MakeTypeError('incompatible_method_receiver',
                          ['PIN_SemaphoreInit', this]);
    
    %_PIN_SemaphoreFini(sem);
}

function SemaphoreConstructor() {
  if (!%_IsConstructCall()) {
    return new $Semaphore();
  }

  this.external = new global.OwnPointer(8);
  this.valid = %_PIN_SemaphoreInit(this.external);
  if (this.valid)
    %_PIN_SemaphoreClear(this.external);
  this.type = "Semaphore";
  this._destructor = PIN_SemaphoreFini;
}

function SemaphoreIsSet() {
  this.assert();
  return %_PIN_SemaphoreIsSet(this.external);
}

function SemaphoreSet() {
  this.assert();
  %_PIN_SemaphoreSet(this.external);
}

function SemaphoreClear() {
  this.assert();
  %_PIN_SemaphoreClear(this.external);
}

function SemaphoreWait(timeout) {
  this.assert();
  if (!IS_UNDEFINED(timeout) && timeout > 0) {
    return %_PIN_SemaphoreTimedWait(this.external, timeout);
  } else {
    %_PIN_SemaphoreWait(this.external);
    return true;
  }
}

function NoOpSetter(ignored) {}

function SetupTrace() {
  //Trace
  %FunctionSetInstanceClassName($Trace, 'Trace');
  %SetCode($Trace, TraceConstructor);
  %SetProperty($Trace.prototype, "constructor", $Trace, DONT_ENUM);
  InstallFunctions($Trace.prototype, DONT_ENUM, $Array(
    "assert", ExternalPointerAssert
  ));
  %OptimizeObjectForAddingMultipleProperties($Trace.prototype, 9);
  %DefineOrRedefineAccessorProperty($Trace.prototype, 'isOriginal', TRACE_Original, NoOpSetter, DONT_DELETE);
  %ToFastProperties($Trace.prototype);
}

function SetupSemaphore() {
  //Semaphore
  %FunctionSetInstanceClassName($Semaphore, 'Semaphore');
  %SetCode($Semaphore, SemaphoreConstructor);
  %SetProperty($Semaphore.prototype, "constructor", $Semaphore, DONT_ENUM);
  InstallFunctions($Semaphore.prototype, DONT_ENUM, $Array(
    "assert", ExternalPointerAssert,
    "destroy", ExternalPointerDestroy,
    "set", SemaphoreSet,
    "isSet", SemaphoreIsSet,
    "clear", SemaphoreClear,
    "wait", SemaphoreWait
  ));
}

function SetupCurrent() {
  %OptimizeObjectForAddingMultipleProperties($current, 4);
  %DefineOrRedefineAccessorProperty($current, 'pid', PIN_GetPid, NoOpSetter, DONT_DELETE);
  %DefineOrRedefineAccessorProperty($current, 'tid', PIN_GetTid, NoOpSetter, DONT_DELETE);
  %DefineOrRedefineAccessorProperty($current, 'threadId', PIN_ThreadId, NoOpSetter, DONT_DELETE);
  %DefineOrRedefineAccessorProperty($current, 'threadUid', PIN_ThreadUid, NoOpSetter, DONT_DELETE);
  %ToFastProperties($current);
}

function SetupControl() {
  InstallFunctions($control, DONT_ENUM, $Array(
    "yield", PIN_Yield,
    "sleep", PIN_Sleep
  ));
}

function SetupProcess() {
  InstallFunctions($process, DONT_ENUM, $Array(
    "exit", PIN_ExitApplication,
    "forcedExit", PIN_ExitProcess
  ));
  %OptimizeObjectForAddingMultipleProperties($process, 2);
  %DefineOrRedefineAccessorProperty($process, 'pid', PIN_GetPid, NoOpSetter, DONT_DELETE);
  %DefineOrRedefineAccessorProperty($process, 'exiting', PIN_IsProcessExiting, NoOpSetter, DONT_DELETE);
  %ToFastProperties($process);
}

function SetupPIN() {
  %CheckIsBootstrapping();
  
  SetupControl();
  
  SetupProcess();

  SetupCurrent();

  SetupTrace();  

  SetupSemaphore();
}

SetupPIN();
