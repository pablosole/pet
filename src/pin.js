"use strict";

/************************************************************************/
/* Healper function                                                     */
/************************************************************************/

//return a JS String object from a c++ string pointer
function ReadStringPointer (addr) {
    return %_JSStringFromCString(addr);
}
global.readStringPointer = ReadStringPointer;

/* ExternalPointer support functions */
function PointerAssert() {
  if (!this.valid)
    throw "Invalid " + this.type;
}

function PointerDestroy() {
  this.assert();
  if (IS_FUNCTION(this._destructor))
    this._destructor(this.external);
  this.external={};
  this.valid=false;  
}

function InstallROAccessors(object, functions) {
  if (functions.length > 8)
    %OptimizeObjectForAddingMultipleProperties(object, functions.length >> 1);
  
  for (var i = 0; i < functions.length; i += 2)
    %DefineOrRedefineAccessorProperty(object, functions[i], functions[i+1], null, DONT_DELETE | READ_ONLY);
  
  %ToFastProperties(object);
}

/************************************************************************/
/* current object                                                       */
/************************************************************************/
global.current = new $Object();
var $current = global.current;
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

function SetupCurrent() {
  InstallROAccessors($current, $Array(
      'pid', PIN_GetPid,
      'tid', PIN_GetTid,
      'threadId', PIN_ThreadId,
      'threadUid', PIN_ThreadUid
  ));
}


/************************************************************************/
/* process object                                                       */
/************************************************************************/
global.process = new $Object();
var $process = global.process;
function PIN_IsProcessExiting() {
    return %_PIN_IsProcessExiting();
}

function PIN_ExitProcess(s) {
    %_PIN_ExitProcess(s);
}

function PIN_ExitApplication(s) {
    %_PIN_ExitApplication(s);
}

function SetupProcess() {
  InstallFunctions($process, DONT_ENUM, $Array(
    "exit", PIN_ExitApplication,
    "forcedExit", PIN_ExitProcess
  ));
  InstallROAccessors($process, $Array(
      'pid', PIN_GetPid, 
      'exiting', PIN_IsProcessExiting, 
      'imageHead', APP_ImgHead, 
      'imageTail', APP_ImgTail
  ));
}


/************************************************************************/
/* control object                                                       */
/************************************************************************/
global.control = new $Object();
var $control = global.control;
function PIN_Yield() {
    %_PIN_Yield();
}

function PIN_Sleep(t) {
    %_PIN_Sleep(t);
}

function SetupControl() {
  InstallFunctions($control, DONT_ENUM, $Array(
    "yield", PIN_Yield,
    "sleep", PIN_Sleep
  ));
}


/************************************************************************/
/* routines object                                                       */
/************************************************************************/
global.routines = new $Object();
var $routines = global.routines;

function RTN_FindNameByAddress(addr) {
    var tmp = new global.OwnString("");
    %_RTN_FindNameByAddress(addr, tmp);
    return %_JSStringFromCString(%_UnwrapPointer(tmp));
}

function RTN_FindByAddress(addr) {
    var external = new global.ExternalPointer();
    %_RTN_FindByAddress(addr, external);
    
    //no section info for this routine
    return new $Routine(external, {});
}

function RTN_CreateAt(addr, name) {
    this.close();
    var tmp = new global.OwnString(name);
    var external = new global.ExternalPointer();
    %_RTN_CreateAt(addr, tmp, external);
    
    //no section info for this routine
    return new $Routine(external, {});
}

function SetupRoutines() {
  InstallFunctions($routines, DONT_ENUM, $Array(
    "findNameByAddress", RTN_FindNameByAddress,
    "findByAddress", RTN_FindByAddress,
    "close", RTN_Close,
    "create", RTN_CreateAt
  ));
}


/************************************************************************/
/* semaphore object                                                     */
/************************************************************************/
global.Semaphore = function() {
  if (!%_IsConstructCall()) {
    return new $Semaphore();
  }

  this.external = new global.OwnPointer(8);  //size of a PIN Semaphore
  this.valid = %_PIN_SemaphoreInit(this.external);
  if (this.valid)
    %_PIN_SemaphoreClear(this.external);     //initialize semaphores on clear state
  this.type = "Semaphore";
  this._destructor = PIN_SemaphoreFini;
}
var $Semaphore = global.Semaphore;

function PIN_SemaphoreFini(sem) {
    %_PIN_SemaphoreFini(sem);
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
  var argc = %_ArgumentsLength();
  if (argc > 0 && timeout > 0) {
    return %_PIN_SemaphoreTimedWait(this.external, timeout);
  } else {
    %_PIN_SemaphoreWait(this.external);
    return true;
  }
}

function SetupSemaphore() {
  %FunctionSetInstanceClassName($Semaphore, 'Semaphore');
  %SetProperty($Semaphore.prototype, "constructor", $Semaphore, DONT_ENUM);
  InstallFunctions($Semaphore.prototype, DONT_ENUM, $Array(
    "assert", PointerAssert,
    "destroy", PointerDestroy,
    "set", SemaphoreSet,
    "isSet", SemaphoreIsSet,
    "clear", SemaphoreClear,
    "wait", SemaphoreWait
  ));
}

/************************************************************************/
/* image object                                                         */
/************************************************************************/
var $image_opened = false;
var $ImageInvalidAddr = %_IMG_Invalid();

global.Image = function(external) {
    if (!%_IsConstructCall()) {
        return new $Image(external);
    }
    
    if (%_UnwrapPointer(external) == $ImageInvalidAddr) {
        this.external = {};
        this.valid = false;
    } else {
        this.external = external;
        this.valid = true;
    }
  this.type = "Image";
}
var $Image = global.Image;

function APP_ImgHead() {
    var external = new global.ExternalPointer();
    %_APP_ImgHead(external);
    return new $Image(external);
}

function APP_ImgTail() {
    var external = new global.ExternalPointer();
    %_APP_ImgTail(external);
    return new $Image(external);
}

function IMG_Next() {
    this.assert();
    var external = new global.ExternalPointer();
    %_IMG_Next(this.external, external);
    return new $Image(external);
}

function IMG_Prev() {
    this.assert();
    var external = new global.ExternalPointer();
    %_IMG_Prev(this.external, external);
    return new $Image(external);
}

function IMG_Name() {
    this.assert();
    return %_JSStringFromCString(%_IMG_Name(this.external));
}

function IMG_Entry() {
    this.assert();
    return %_IMG_Entry(this.external);
}

function IMG_LoadOffset() {
    this.assert();
    return %_IMG_LoadOffset(this.external);
}

function IMG_SecHead(external) {
    this.assert();
    var sec = new global.ExternalPointer();
    %_IMG_SecHead(this.external, sec);
    return new $Section(sec, this);
}

function IMG_SecTail() {
    this.assert();
    var sec = new global.ExternalPointer();
    %_IMG_SecTail(this.external, sec);
    return new $Section(sec, this);
}

function IMG_RegsymHead(external) {
    this.assert();
    var sym = new global.ExternalPointer();
    %_IMG_RegsymHead(this.external, sym);
    return new $Symbol(sym, this);
}

function IMG_Close() {
    this.assert();
    %_IMG_Close(this.external);
    $image_opened = false;
}

function IMG_Open(name) {
    if ($image_opened)
        throw "It can be only one statically opened image";
    
    $image_opened = true;
    var external = new global.ExternalPointer();
    var jsname = new global.OwnString(name);
    %_IMG_Open(jsname, external);
    var img = new $Image(external);
    if (!img.valid)
        $image_opened = false;
    else
        img._destructor = IMG_Close;
    
    return img;
}

function ImageListConstructor() {
    var arr = new $Array();
    var head = APP_ImgHead();
    while (head.valid) {
        arr.push(head);
        head = head.next;
    }
    
    InstallFunctions(arr, DONT_ENUM, $Array(
      "findByAddress", IMG_FindByAddress,
      "findById", IMG_FindImgById,
      "open", IMG_Open
    ));
    
    return arr;
}

function IMG_FindByAddress(addr) {
    var external = new global.ExternalPointer();
    %_IMG_FindByAddress(addr, external);
    return new $Image(external);
}

function IMG_FindImgById(imgid) {
    var external = new global.ExternalPointer();
    %_IMG_FindImgById(imgid, external);
    return new $Image(external);
}

function SetupImage() {
  %DefineOrRedefineAccessorProperty(global, 'images', ImageListConstructor, null, DONT_DELETE | READ_ONLY);
  
  %FunctionSetInstanceClassName($Image, 'Image');
  %SetProperty($Image.prototype, "constructor", $Image, DONT_ENUM);
  InstallFunctions($Image.prototype, DONT_ENUM, $Array(
    "assert", PointerAssert,
    "destroy", PointerDestroy
  ));
  
  InstallROAccessors($Image.prototype, $Array(
      'next', IMG_Next, 
      'prev', IMG_Prev, 
      'name', IMG_Name, 
      'entryPoint', IMG_Entry, 
      'loadOffset', IMG_LoadOffset, 
      'sectionHead', IMG_SecHead, 
      'sectionTail', IMG_SecTail, 
      'sections', SectionListConstructor, 
      'symbolHead', IMG_RegsymHead, 
      'symbols', SymbolListConstructor
  ));
}


/************************************************************************/
/* section object                                                       */
/************************************************************************/
var $SectionInvalidAddr = %_SEC_Invalid();
function SectionListConstructor() {
    this.assert();
    var arr = new $Array();
    var head = this.sectionHead;
    while (head.valid) {
        arr.push(head);
        head = head.next;
    }
    
    return arr;
}

global.Section = function (external, image) {
    if (!%_IsConstructCall()) {
        return new $Section(external, image);
    }
    
    if (%_UnwrapPointer(external) == $SectionInvalidAddr) {
        this.external = {};
        this.valid = false;
    } else {
        this.external = external;
        this.valid = true;
    }
  this.type = "Section";
  this.image = image;
}
var $Section = global.Section;

function SEC_Next() {
    this.assert();
    var external = new global.ExternalPointer();
    %_SEC_Next(this.external, external);
    return new $Section(external, this.image);
}

function SEC_Prev() {
    this.assert();
    var external = new global.ExternalPointer();
    %_SEC_Prev(this.external, external);
    return new $Section(external, this.image);
}

function SEC_Name() {
    this.assert();
    return %_JSStringFromCString(%_SEC_Name(this.external));
}

function SEC_RtnHead(external) {
    this.assert();
    var rtn = new global.ExternalPointer();
    %_SEC_RtnHead(this.external, rtn);
    return new $Routine(rtn, this);
}

function SEC_RtnTail() {
    this.assert();
    var rtn = new global.ExternalPointer();
    %_SEC_RtnTail(this.external, rtn);
    return new $Routine(rtn, this);
}

function SetupSection() {
  %FunctionSetInstanceClassName($Section, 'Section');
  %SetProperty($Section.prototype, "constructor", $Section, DONT_ENUM);
  InstallFunctions($Section.prototype, DONT_ENUM, $Array(
    "assert", PointerAssert,
    "destroy", PointerDestroy
  ));
  
  InstallROAccessors($Section.prototype, $Array(
      'next', SEC_Next, 
      'prev', SEC_Prev, 
      'name', SEC_Name, 
      'routineHead', SEC_RtnHead, 
      'routineTail', SEC_RtnTail, 
      'routines', RoutineListConstructor
  ));
}


/************************************************************************/
/* symbol object                                                        */
/************************************************************************/
var $SymbolInvalidAddr = %_SYM_Invalid();
function SymbolListConstructor() {
    this.assert();
    var arr = new $Array();
    var head = this.symbolHead;
    while (head.valid) {
        arr.push(head);
        head = head.next;
    }
    
    return arr;
}

global.Symbol = function(external, image) {
    if (!%_IsConstructCall()) {
        return new $Symbol(external, image);
    }
    
    if (%_UnwrapPointer(external) == $SymbolInvalidAddr) {
        this.external = {};
        this.valid = false;
    } else {
        this.external = external;
        this.valid = true;
    }
  this.type = "Symbol";
  this.image = image;
}
var $Symbol = global.Symbol;

function SYM_Next() {
    this.assert();
    var external = new global.ExternalPointer();
    %_SYM_Next(this.external, external);
    return new $Symbol(external, this.image);
}

function SYM_Prev() {
    this.assert();
    var external = new global.ExternalPointer();
    %_SYM_Prev(this.external, external);
    return new $Symbol(external, this.image);
}

function SYM_Name() {
    this.assert();
    return %_JSStringFromCString(%_SYM_Name(this.external));
}

function SetupSymbol() {
  %FunctionSetInstanceClassName($Symbol, 'Symbol');
  %SetProperty($Symbol.prototype, "constructor", $Symbol, DONT_ENUM);
  InstallFunctions($Symbol.prototype, DONT_ENUM, $Array(
    "assert", PointerAssert,
    "destroy", PointerDestroy
  ));
  
  InstallROAccessors($Symbol.prototype, $Array(
      'next', SYM_Next,
      'prev', SYM_Prev,
      'name', SYM_Name
  ));
}


/************************************************************************/
/* routine object                                                       */
/************************************************************************/
var $RoutineInvalidAddr = %_RTN_Invalid();
function RoutineListConstructor() {
    this.assert();
    var arr = new $Array();
    var head = this.routineHead;
    while (head.valid) {
        arr.push(head);
        head = head.next;
    }
    
    return arr;
}

global.Routine = function(external, sec) {
    if (!%_IsConstructCall()) {
        return new $Routine(external, sec);
    }
    
    if (%_UnwrapPointer(external) == $RoutineInvalidAddr) {
        this.external = {};
        this.valid = false;
    } else {
        this.external = external;
        this.valid = true;
    }
    
    this.type = "Routine";
    this.section = sec;
}
var $Routine = global.Routine;
var $routine_opened = null;

function RTN_Close() {
    if (!IS_NULL($routine_opened)) {
        %_RTN_Close($routine_opened);
        $routine_opened = null;
    }
}

function RTN_Open() {
    this.assert();
    
    if (!IS_NULL($routine_opened)) {
        //trying to open the same routine, maybe through a diff JS Object
        if (%_UnwrapPointer(this.external) == %_UnwrapPointer($routine_opened))
            return;

        RTN_Close();
    }
    
    $routine_opened = this.external;
    %_RTN_Open(this.external);
}

function RTN_IsOpen() {
    this.assert();
    return (!IS_NULL($routine_opened) && %_UnwrapPointer(this.external) == %_UnwrapPointer($routine_opened));
}

function RTN_Next() {
    this.assert();
    var external = new global.ExternalPointer();
    %_RTN_Next(this.external, external);
    return new $Routine(external, this.section);
}

function RTN_Prev() {
    this.assert();
    var external = new global.ExternalPointer();
    %_RTN_Prev(this.external, external);
    return new $Routine(external, this.section);
}

function RTN_Name() {
    this.assert();
    return %_JSStringFromCString(%_RTN_Name(this.external));
}

function RTN_InsHead(external) {
    this.assert();
    this.open();
    var ins = new global.ExternalPointer();
    %_RTN_InsHead(this.external, ins);
    return new $Instruction(ins);
}

function RTN_InsHeadOnly(external) {
    this.assert();
    this.open();
    var ins = new global.ExternalPointer();
    %_RTN_InsHeadOnly(this.external, ins);
    return new $Instruction(ins);
}

function RTN_InsTail(external) {
    this.assert();
    var ins = new global.ExternalPointer();
    %_RTN_InsTail(this.external, ins);
    return new $Instruction(ins);
}

function SetupRoutine() {
  %FunctionSetInstanceClassName($Routine, 'Routine');
  %SetProperty($Routine.prototype, "constructor", $Routine, DONT_ENUM);
  InstallFunctions($Routine.prototype, DONT_ENUM, $Array(
    "open", RTN_Open,
    "isOpen", RTN_IsOpen,  
    "close", RTN_Close, 
    "forEachInstruction", RoutineForEachInstruction,
    "assert", PointerAssert,
    "destroy", PointerDestroy
  ));
  
  InstallROAccessors($Routine.prototype, $Array(
      'next', RTN_Next, 
      'prev', RTN_Prev, 
      'name', RTN_Name, 
      'instructionHead', RTN_InsHead, 
      'instructionHeadOnly', RTN_InsHeadOnly, 
      'instructionTail', RTN_InsTail
  ));
}


/************************************************************************/
/* instruction object                                                   */
/************************************************************************/
var $InstructionInvalidAddr = %_INS_Invalid();
function RoutineForEachInstruction(fun) {
    this.assert();
    
    //implicitly open the routine
    var head = this.instructionHead;
    while (head.valid) {
        fun(head);
        head = head.next;
    }
    
    //Close the Routine
    this.close();
}

global.Instruction = function(external) {
    if (!%_IsConstructCall()) {
        return new $Instruction(external);
    }
    
    if (%_UnwrapPointer(external) == $InstructionInvalidAddr) {
        this.external = {};
        this.valid = false;
    } else {
        this.external = external;
        this.valid = true;
    }
  this.type = "Instruction";
  this._disassemble = null;
}
var $Instruction = global.Instruction;

function INS_Next() {
    this.assert();
    var external = new global.ExternalPointer();
    %_INS_Next(this.external, external);
    return new $Instruction(external);
}

function INS_Prev() {
    this.assert();
    var external = new global.ExternalPointer();
    %_INS_Prev(this.external, external);
    return new $Instruction(external);
}

function INS_Address() {
    this.assert();
    return %_INS_Address(this.external);
}

function INS_Size() {
    this.assert();
    return %_INS_Size(this.external);
}

function INS_Disassemble() {
    this.assert();
    if (IS_NULL(this._disassemble)) {
        var tmp = new global.OwnString("");  //send an empty string to have an actual allocated string object
        %_INS_Disassemble(this.external, tmp);
        this._disassemble = %_JSStringFromCString(%_UnwrapPointer(tmp));
    }
    return this._disassemble;
}

function SetupInstruction() {
  %FunctionSetInstanceClassName($Instruction, 'Instruction');
  %SetProperty($Instruction.prototype, "constructor", $Instruction, DONT_ENUM);
  InstallFunctions($Instruction.prototype, DONT_ENUM, $Array(
    "assert", PointerAssert,
    "destroy", PointerDestroy,
    'toString', INS_Disassemble
  ));
  
  InstallROAccessors($Instruction.prototype, $Array(
      'next', INS_Next, 
      'prev', INS_Prev, 
      'disassemble', INS_Disassemble,
      'address', INS_Address,
      'size', INS_Size
  ));
}

/************************************************************************/
/* event object                                                         */
/************************************************************************/
var $EventNames = new $Array("newroutine", "newtrace", "newinstruction", 
    "startthread", "finithread", "loadimage", "unloadimage", "startapp", "finiapp", 
    "followchild", "detach", "fetch", "memoryaddresstranslation", "contextchange",
    "syscallenter", "syscallexit");
var $EventList = new $Object();
global.events = $EventList;

//fast event checking for highly recurrent events (newroutine, newtrace, newinstruction)
//$fastEvents_#_bool is actually an external pointer to a boolean on C++
var $fastEvents_0_bool = null;
var $fastEvents_0 = new $Array();
var $fastEvents_1_bool = null;
var $fastEvents_1 = new $Array();
var $fastEvents_2_bool = null;
var $fastEvents_2 = new $Array();
var $eventFlags = 0;

global.Event = function(name, fun) {
    if (!IS_FUNCTION(fun)) {
        throw "Event Callback is not a function";
    }
    
    var idx = $EventNames.indexOf(name.toLowerCase());
    if (idx == -1) {
        throw "Event name not recognized: " + name;
    }
    
    if (idx < 3)
        this.fast = true;
    else
        this.fast = false;
    
    this.idx = idx;
    this.name = name.toLowerCase();
    this.type = "Event";
    this.callback = fun;
    this._enabled = true;
}
var $Event = global.Event;

function EventEnabledGetter() {
    return this._enabled;
}

function EventEnabledSetter(val) {
    var tmp = ToBoolean(val);
    
    //noop
    if (tmp == this._enabled)
        return;
    
    if (this.fast) {
        var arr;
        var bool;
        if (this.idx == 0) {
            arr = $fastEvents_0;
            bool = $fastEvents_0_bool;
        } else if (this.idx == 1) {
            arr = $fastEvents_1;
            bool = $fastEvents_1_bool;
        } else if (this.idx == 2) {
            arr = $fastEvents_2;
            bool = $fastEvents_2_bool;
        }
        
        if (tmp == true) {
            //push the event back into the array
            var idx = arr.indexOf(this.callback);
            if (idx == -1)
                arr.push(this.callback);
        } else {
            //remove from the event list
            var idx = arr.indexOf(this.callback);
            if (idx != -1)
                arr.splice(idx, 1);
        }
        if (arr.length() > 0) {
            %_WritePointer(%_UnwrapPointer(bool), 1); 
        } else {
            %_WritePointer(%_UnwrapPointer(bool), 0); 
        }
    }
    
    this._enabled = tmp;
}

global.setupEventBooleans = function(b1,b2,b3,flags) {
    $fastEvents_0_bool = b1;
    $fastEvents_1_bool = b2;
    $fastEvents_2_bool = b3;
    $eventFlags = flags;
}

function EventAttach(arg0, arg1) {
    var argc = %_ArgumentsLength();
    var evt;
    if (argc == 2) {
        evt = new $Event(arg0, arg1);
    } else {
        evt = arg0;
    }
    
    if (IS_NULL_OR_UNDEFINED($EventList[evt.name])) {
        $EventList[evt.name] = new $Array();
        
        //flag the first time we use an event type
        var cur = %_ReadPointer(%_UnwrapPointer($eventFlags));
        cur |= 1 << evt.idx;
        %_WritePointer(%_UnwrapPointer($eventFlags), cur);
    }
    
    //event already attached
    if ($EventList[evt.name].indexOf(evt) != -1)
        return false;
    
    $EventList[evt.name].push(evt);
    
    //hack for fast events
    if (evt.fast) {
        var enabled = evt._enabled;
        evt._enabled = false;
        
        //force adding the event to correspondent array
        evt.enabled = enabled;
    }
    
    return true;
}

function EventDetach(evt) {
    var idx = $EventList[evt.name].indexOf(evt);
    if (idx == -1)
        return false;

    $EventList[evt.name].splice(idx, 1);
    
    //hack for fast events
    if (evt.fast) {
        var enabled = evt._enabled;
        //force removing the event from correspondent array
        evt.enabled = false;
        evt._enabled = enabled;
    }
    
    return true;
}

function SetupEvents() {
  %FunctionSetInstanceClassName($Event, 'Event');
  %SetProperty($Event.prototype, "constructor", $Event, DONT_ENUM);
  InstallFunctions($EventList, DONT_ENUM, $Array(
    'attach', EventAttach,
    'detach', EventDetach
  ));
  
  %DefineOrRedefineAccessorProperty($Event.prototype, "enabled", EventEnabledGetter, EventEnabledSetter, DONT_DELETE);
}

function SetupPIN() {
  %CheckIsBootstrapping();
  
  SetupControl();
  
  SetupProcess();

  SetupCurrent();
  
  SetupRoutines();

  SetupSemaphore();

  SetupImage();
  
  SetupSection();
  
  SetupSymbol();
  
  SetupRoutine();
  
  SetupInstruction();
  
  SetupEvents();
}

SetupPIN();
