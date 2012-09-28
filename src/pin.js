"use strict";

/************************************************************************/
/* Healper function                                                     */
/************************************************************************/

//return a JS String object from a c++ string pointer
global.readCPPString = function(addr) {
    return %_JSStringFromCString(addr);
}

global.readAsciiString = function(addr, size) {
    if (IS_NULL_OR_UNDEFINED(size))
        size = 0;
    return %_ReadAsciiString(addr, size);
}

global.readTwoByteString = function(addr, size) {
    if (IS_NULL_OR_UNDEFINED(size))
        size = 0;
    return %_ReadTwoByteString(addr, size);
}

global.readDword = function(addr) {
    return %_ReadPointer(addr);
}

global.readWord = function(addr) {
    return %_ReadWord(addr);
}

global.readByte = function(addr) {
    return %_ReadByte(addr);
}

global.readDouble = function(addr) {
    return %_ReadDouble(addr);
}

global.writeDword = function(addr, val) {
    return %_WritePointer(addr, val);
}

global.writeWord = function(addr, val) {
    return %_WriteWord(addr, val);
}

global.writeByte = function(addr, val) {
    return %_WriteByte(addr, val);
}

global.writeDouble = function(addr, val) {
    return %_WriteDouble(addr, val);
}

/* ExternalPointer support functions */
function ExternalAssert() {
  if (!this.valid)
    throw "Invalid " + this.type;
}

function ExternalDestroy() {
  this.assert();
  if (IS_FUNCTION(this._destructor))
    this._destructor(this.external);
  this.external=0;
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
$current.event = {};
$current.thread = {};

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
    global.PIN_Sleep(t);
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
    //no section info for this routine
    return new $Routine(%_RTN_FindByAddress(addr), {});
}

function RTN_CreateAt(addr, name) {
    this.close();
    var tmp = new global.OwnString(name);
    
    //no section info for this routine
    return new $Routine(%_RTN_CreateAt(addr, tmp), {});
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
  return %_PIN_SemaphoreIsSet(this.external);
}

function SemaphoreSet() {
  %_PIN_SemaphoreSet(this.external);
}

function SemaphoreClear() {
  %_PIN_SemaphoreClear(this.external);
}

function SemaphoreWait(timeout) {
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
    "assert", ExternalAssert,
    "destroy", ExternalDestroy,
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
    
    this.external = external;
    this.valid = external != $ImageInvalidAddr;
    this.type = "Image";
}
var $Image = global.Image;

function APP_ImgHead() {
    return new $Image(%_APP_ImgHead());
}

function APP_ImgTail() {
    return new $Image(%_APP_ImgTail());
}

function IMG_Next() {
    return new $Image(%_IMG_Next(this.external));
}

function IMG_Prev() {
    return new $Image(%_IMG_Prev(this.external));
}

function IMG_Name() {
    return %_JSStringFromCString(%_IMG_Name(this.external));
}

function IMG_Entry() {
    return %_IMG_Entry(this.external);
}

function IMG_LoadOffset() {
    return %_IMG_LoadOffset(this.external);
}

function IMG_SecHead(external) {
    return new $Section(%_IMG_SecHead(this.external), this);
}

function IMG_SecTail() {
    return new $Section(%_IMG_SecTail(this.external), this);
}

function IMG_RegsymHead(external) {
    return new $Symbol(%_IMG_RegsymHead(this.external), this);
}

function IMG_Close() {
    %_IMG_Close(this.external);
    $image_opened = false;
}

function IMG_Open(name) {
    if ($image_opened)
        throw "It can be only one statically opened image";
    
    $image_opened = true;
    var jsname = new global.OwnString(name);
    var img = new $Image(%_IMG_Open(jsname));
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
    return new $Image(%_IMG_FindByAddress(addr));
}

function IMG_FindImgById(imgid) {
    return new $Image(%_IMG_FindImgById(imgid));
}

function SetupImage() {
  %DefineOrRedefineAccessorProperty(global, 'images', ImageListConstructor, null, DONT_DELETE | READ_ONLY);
  
  %FunctionSetInstanceClassName($Image, 'Image');
  %SetProperty($Image.prototype, "constructor", $Image, DONT_ENUM);
  InstallFunctions($Image.prototype, DONT_ENUM, $Array(
    "assert", ExternalAssert,
    "destroy", ExternalDestroy
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
    
    this.external = external;
    this.valid = external != $SectionInvalidAddr;
    this.type = "Section";
    this.image = image;
}
var $Section = global.Section;

function SEC_Next() {
    return new $Section(%_SEC_Next(this.external), this.image);
}

function SEC_Prev() {
    return new $Section(%_SEC_Prev(this.external), this.image);
}

function SEC_Name() {
    return %_JSStringFromCString(%_SEC_Name(this.external));
}

function SEC_RtnHead(external) {
    return new $Routine(%_SEC_RtnHead(this.external), this);
}

function SEC_RtnTail() {
    return new $Routine(%_SEC_RtnTail(this.external), this);
}

function SetupSection() {
  %FunctionSetInstanceClassName($Section, 'Section');
  %SetProperty($Section.prototype, "constructor", $Section, DONT_ENUM);
  InstallFunctions($Section.prototype, DONT_ENUM, $Array(
    "assert", ExternalAssert,
    "destroy", ExternalDestroy
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
    
    this.external = external;
    this.valid = external != $SymbolInvalidAddr;
    this.type = "Symbol";
    this.image = image;
}
var $Symbol = global.Symbol;

function SYM_Next() {
    return new $Symbol(%_SYM_Next(this.external), this.image);
}

function SYM_Prev() {
    return new $Symbol(%_SYM_Prev(this.external), this.image);
}

function SYM_Name() {
    return %_JSStringFromCString(%_SYM_Name(this.external));
}

function SetupSymbol() {
  %FunctionSetInstanceClassName($Symbol, 'Symbol');
  %SetProperty($Symbol.prototype, "constructor", $Symbol, DONT_ENUM);
  InstallFunctions($Symbol.prototype, DONT_ENUM, $Array(
    "assert", ExternalAssert,
    "destroy", ExternalDestroy
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
    
    this.external = external;
    this.valid = external != $RoutineInvalidAddr;
    this.type = "Routine";
    this.section = sec;
}
var $Routine = global.Routine;
var $routine_opened = null;

function IPointToNumber(point) {
    if (IS_NUMBER(point))
        return point;
    
    point = point.toLowerCase();
    switch (point) {
        case "before": return 1;  //IPOINT_BEFORE
        case "after": return 2;   //IPOINT_AFTER
        case "anywhere": return 3;//IPOINT_ANYWHERE
        case "taken_branch":      //IPOINT_TAKEN_BRANCH
        case "taken":
        case "branch": return 4;
    }
    
    throw "invalid IPOINT name";
}

function RTN_InsertCall(point, af) {
    this.open();
    global.InsertCall(this.external, IPointToNumber(point), af.external, 0);
    this.close();
}

function RTN_Close() {
    if (!IS_NULL($routine_opened)) {
        %_RTN_Close($routine_opened);
        $routine_opened = null;
    }
}

function RTN_Open() {
    if (!IS_NULL($routine_opened)) {
        //trying to open the same routine, maybe through a diff JS Object
        if (this.external == $routine_opened)
            return;

        RTN_Close();
    }
    
    $routine_opened = this.external;
    %_RTN_Open(this.external);
}

function RTN_IsOpen() {
    return (!IS_NULL($routine_opened) && this.external == $routine_opened);
}

function RTN_Next() {
    return new $Routine(%_RTN_Next(this.external), this.section);
}

function RTN_Prev() {
    return new $Routine(%_RTN_Prev(this.external), this.section);
}

function RTN_Name() {
    return %_JSStringFromCString(%_RTN_Name(this.external));
}

function RTN_InsHead(external) {
    this.open();
    return new $Instruction(%_RTN_InsHead(this.external));
}

function RTN_InsHeadOnly(external) {
    this.open();
    return new $Instruction(%_RTN_InsHeadOnly(this.external));
}

function RTN_InsTail(external) {
    this.open();
    return new $Instruction(%_RTN_InsTail(this.external));
}

function SetupRoutine() {
  %FunctionSetInstanceClassName($Routine, 'Routine');
  %SetProperty($Routine.prototype, "constructor", $Routine, DONT_ENUM);
  InstallFunctions($Routine.prototype, DONT_ENUM, $Array(
    "open", RTN_Open,
    "isOpen", RTN_IsOpen,  
    "close", RTN_Close, 
    "attach", RTN_InsertCall,
    "forEachInstruction", RoutineForEachInstruction,
    "assert", ExternalAssert,
    "destroy", ExternalDestroy
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
    
    this.external = external;
    this.valid = external != $InstructionInvalidAddr;
    this.type = "Instruction";
}
var $Instruction = global.Instruction;

function INS_Next() {
    return new $Instruction(%_INS_Next(this.external));
}

function INS_Prev() {
    return new $Instruction(%_INS_Prev(this.external));
}

function INS_Address() {
    return %_INS_Address(this.external);
}

function INS_Size() {
    return %_INS_Size(this.external);
}

function INS_Disassemble() {
    var tmp = new global.OwnString("");  //send an empty string to have an actual allocated string object
    %_INS_Disassemble(this.external, tmp);
    return %_JSStringFromCString(%_UnwrapPointer(tmp));
}

function INS_InsertCall(point, af) {
    global.InsertCall(this.external, IPointToNumber(point), af.external, 1);
}

function SetupInstruction() {
  %FunctionSetInstanceClassName($Instruction, 'Instruction');
  %SetProperty($Instruction.prototype, "constructor", $Instruction, DONT_ENUM);
  InstallFunctions($Instruction.prototype, DONT_ENUM, $Array(
    "assert", ExternalAssert,
    "destroy", ExternalDestroy,
    'toString', INS_Disassemble,
    "attach", INS_InsertCall
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
/* trace object                                                         */
/************************************************************************/
global.Trace = function(external) {
    if (!%_IsConstructCall()) {
        return new $Trace(external);
    }
    
    //this external is a class instance ptr, not a handle like INS,BBL,etc
    this.external = external;
    this.valid = true;
    this.type = "Trace";
}
var $Trace = global.Trace;

function TRACE_Rtn() {
    return new $Routine(%_TRACE_Rtn(this.external));
}

function TRACE_Address() {
    return %_TRACE_Address(this.external);
}

function TRACE_Size() {
    return %_TRACE_Size(this.external);
}

function TRACE_InsertCall(point, af) {
    global.InsertCall(this.external, IPointToNumber(point), af.external, 3);
}

function TRACE_BblHead(external) {
    return new $BBL(%_TRACE_BblHead(this.external), this);
}

function TRACE_BblTail() {
    return new $BBL(%_TRACE_BblTail(this.external), this);
}

function SetupTrace() {
  %FunctionSetInstanceClassName($Trace, 'Trace');
  %SetProperty($Trace.prototype, "constructor", $Trace, DONT_ENUM);
  InstallFunctions($Trace.prototype, DONT_ENUM, $Array(
    "assert", ExternalAssert,
    "destroy", ExternalDestroy,
    "attach", TRACE_InsertCall
  ));
  
  InstallROAccessors($Trace.prototype, $Array(
      'routine', TRACE_Rtn,
      'address', TRACE_Address,
      'size', TRACE_Size,
      'bblHead', TRACE_BblHead,
      'bblTail', TRACE_BblTail,
      'basicblocks', BBLListConstructor
  ));
}


/************************************************************************/
/* basicblock object                                                    */
/************************************************************************/
function BBLListConstructor() {
    var arr = new $Array();
    var head = this.bblHead;
    while (head.valid) {
        arr.push(head);
        head = head.next;
    }
    
    return arr;
}
global.BBL = function(external, trace) {
    if (!%_IsConstructCall()) {
        return new $Trace(external);
    }
    
    this.trace = trace;
    this.external = external;
    this.type = "BBL";
}
var $BBL = global.BBL;

function BBL_Valid() {
    return %_BBL_Valid(this.external);
}

function BBL_InsHead() {
    return new $Instruction(%_BBL_InsHead(this.external));
}

function BBL_InsTail() {
    return new $Instruction(%_BBL_InsTail(this.external));
}

function BBL_Next() {
    return new $BBL(%_BBL_Next(this.external), this.trace);
}

function BBL_Prev() {
    return new $BBL(%_BBL_Prev(this.external), this.trace);
}

function BBL_Address() {
    return %_BBL_Address(this.external);
}

function BBL_Size() {
    return %_BBL_Size(this.external);
}

function BBL_InsertCall(point, af) {
    global.InsertCall(this.external, IPointToNumber(point), af.external, 2);
}

function BBLInstructionListConstructor() {
    var arr = new $Array();
    var head = this.instructionHead;
    while (head.valid) {
        arr.push(head);
        head = head.next;
    }
    
    return arr;
}

function SetupBBL() {
  %FunctionSetInstanceClassName($BBL, 'BBL');
  %SetProperty($BBL.prototype, "constructor", $BBL, DONT_ENUM);
  InstallFunctions($BBL.prototype, DONT_ENUM, $Array(
    "assert", ExternalAssert,
    "destroy", ExternalDestroy,
    "attach", BBL_InsertCall
  ));
  
  InstallROAccessors($BBL.prototype, $Array(
      'valid', BBL_Valid,
      'instructionHead', BBL_InsHead,
      'instructionTail', BBL_InsTail,
      'instructions', BBLInstructionListConstructor,
      'next', BBL_Next, 
      'prev', BBL_Prev, 
      'address', BBL_Address,
      'size', BBL_Size
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
var $fastEvents_1_bool = null;
var $fastEvents_2_bool = null;
var $fastEvents_0 = new $Array();
var $fastEvents_1 = new $Array();
var $fastEvents_2 = new $Array();

//pre-initialize the fast event arguments and fill the external field on each dispatch
var $fastEvents_0_arg = new $Routine($RoutineInvalidAddr+1);
var $fastEvents_1_arg = new $Trace(0);
var $fastEvents_2_arg = new $Instruction($InstructionInvalidAddr+1);

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
        if (arr.length > 0) {
            %_WritePointer(%_UnwrapPointer(bool), 1); 
        } else {
            %_WritePointer(%_UnwrapPointer(bool), 0); 
        }
    }
    
    this._enabled = tmp;
}

global.setupEventBooleans = function(b1,b2,b3) {
    $fastEvents_0_bool = b1;
    $fastEvents_1_bool = b2;
    $fastEvents_2_bool = b3;
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
        
        //announce the first time we use an event type
        global.addEventProxy(evt.idx);
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

global.fastDispatcher_0 = function(external) {
    $fastEvents_0_arg.external = external;
    for (var idx in $fastEvents_0)
        $fastEvents_0[idx]($fastEvents_0_arg);
}

global.fastDispatcher_1 = function(external) {
    $fastEvents_1_arg.external = external;
    for (var idx in $fastEvents_1)
        $fastEvents_1[idx]($fastEvents_1_arg);
}

global.fastDispatcher_2 = function(external) {
    $fastEvents_2_arg.external = external;
    for (var idx in $fastEvents_2)
        $fastEvents_2[idx]($fastEvents_2_arg);
}

global.startAppDispatcher = function() {
    var name = "startapp";
    if (!IS_NULL_OR_UNDEFINED($EventList[name])) {
        for (var idx in $EventList[name]) {
            var evt = $EventList[name][idx];
            if (evt._enabled) {
                $current.event = evt;
                evt.callback();
            }
        }
        $current.event = {};
    }
}

global.finiAppDispatcher = function() {
    var name = "finiapp";
    if (!IS_NULL_OR_UNDEFINED($EventList[name])) {
        for (var idx in $EventList[name]) {
            var evt = $EventList[name][idx];
            if (evt._enabled) {
                $current.event = evt;
                evt.callback();
            }
        }
        $current.event = {};
    }
}

global.detachDispatcher = function() {
    var name = "detach";
    if (!IS_NULL_OR_UNDEFINED($EventList[name])) {
        for (var idx in $EventList[name]) {
            var evt = $EventList[name][idx];
            if (evt._enabled) {
                $current.event = evt;
                evt.callback();
            }
        }
        $current.event = {};
    }
}

global.startThreadDispatcher = function(tid, external, flags) {
    var name = "startthread";
    var ctx = new $Context(external);
    if (!IS_NULL_OR_UNDEFINED($EventList[name])) {
        $current.thread = global.threads[tid];
        for (var idx in $EventList[name]) {
            var evt = $EventList[name][idx];
            if (evt._enabled) {
                $current.event = evt;
                evt.callback(tid, ctx, flags);
            }
        }
        $current.event = {};
        $current.thread = {};
    }
}

global.finiThreadDispatcher = function(tid, external, code) {
    var name = "finithread";
    var ctx = new $Context(external);
    ctx.fixed = true;
    if (!IS_NULL_OR_UNDEFINED($EventList[name])) {
        $current.thread = global.threads[tid];
        for (var idx in $EventList[name]) {
            var evt = $EventList[name][idx];
            if (evt._enabled) {
                $current.event = evt;
                evt.callback(tid, ctx, code);
            }
        }
        $current.event = {};
        $current.thread = {};
    }
}

global.loadImageDispatcher = function(external) {
    var name = "loadimage";
    var img = new $Image(external);
    if (!IS_NULL_OR_UNDEFINED($EventList[name])) {
        for (var idx in $EventList[name]) {
            var evt = $EventList[name][idx];
            if (evt._enabled) {
                evt.callback(img);
                $current.event = evt;
            }
        }
        $current.event = {};
    }
}

global.unloadImageDispatcher = function(external) {
    var name = "unloadimage";
    var img = new $Image(external);
    if (!IS_NULL_OR_UNDEFINED($EventList[name])) {
        for (var idx in $EventList[name]) {
            var evt = $EventList[name][idx];
            if (evt._enabled) {
                evt.callback(img);
                $current.event = evt;
            }
        }
        $current.event = {};
    }
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


/************************************************************************/
/* context object                                                       */
/************************************************************************/
global.Context = function(external) {
    if (!%_IsConstructCall()) {
        return new $Context(external);
    }
    
    if (IS_UNDEFINED(external)) {
        this.external = {};
        this.valid = false;
        this.fixed = true;
    } else {
        if (!IS_UNDEFINED(external.type) && external.type == "Context") {
            //We're constructing a Context from another context, use PIN_SaveContext.
            var tmp = new global.OwnPointer(0x500);
            %_PIN_SaveContext(external.external, tmp);
            external = tmp;
        }
        this.external = external;
        this.valid = true;
        this.fixed = false;
    }
    this.type = "Context";
}
var $Context = global.Context;

function PIN_GetContextReg(reg) {
    var tmp = reg.external;
    return %_PIN_GetContextReg(this.external, tmp);
}

function PIN_SetContextReg(reg, val) {
    if (this.fixed)
        throw "Cannot set a register on a read-only Context";
    
    var tmp = reg.external;
    %_PIN_SetContextReg(this.external, tmp, val);
}

function PIN_ContextContainsState(state) {
    var tmp;
    switch (state.toLowerCase()) {
        case "x87":
            tmp = 0;
            break;
        case "xmm":
            tmp = 1;
            break;
        case "ymm":
            tmp = 2;
            break;
        default:
            throw "Invalid context state type";
    }
    
    return %_PIN_ContextContainsState(this.external, tmp);
}

//XXX: TODO
// solve ambiguity on PIN_GetContextFPState and PIN_SetContextFPState

function PIN_ExecuteAt() {
    //XXX: TODO
    //synchronize with proxy functions, it should switch after getting out of the Locker
    //only valid on AF and replacements.
}

function SetupContext() {
  %FunctionSetInstanceClassName($Context, 'Context');
  %SetProperty($Context.prototype, "constructor", $Context, DONT_ENUM);
  InstallFunctions($Context.prototype, DONT_ENUM, $Array(
    "assert", ExternalAssert,
    "destroy", ExternalDestroy,
    "execute", PIN_ExecuteAt,
    "get", PIN_GetContextReg,
    "set", PIN_SetContextReg,
    "hasState", PIN_ContextContainsState
  ));
}


/************************************************************************/
/* register object                                                      */
/************************************************************************/
global.Register = function(external) {
    if (!%_IsConstructCall()) {
        return new $Register(external);
    }
    
    if (IS_UNDEFINED(external)) {
        //claim a tool register
        external = %_PIN_ClaimToolRegister();
    }
    
    this.external = external;
    this.valid = external != 0;  //_REG_INVALID = 0
    this.type = "Register";
}
var $Register = global.Register;
global.registers = new $Object();
var $registers = global.registers;

function REG_StringShort() {
    var tmp = new global.OwnString("");  //send an empty string to have an actual allocated string object
    %_REG_StringShort(this.external, tmp);
    return %_JSStringFromCString(%_UnwrapPointer(tmp));
}

function REG_FullRegName() {
    return new $Register(%_REG_FullRegName(this.external));
}

function SetupRegister() {
    %FunctionSetInstanceClassName($Register, 'Register');
    %SetProperty($Register.prototype, "constructor", $Register, DONT_ENUM);
    InstallFunctions($Register.prototype, DONT_ENUM, $Array(
        "assert", ExternalAssert,
        "destroy", ExternalDestroy,
        "toString", REG_StringShort
    ));

    InstallROAccessors($Register.prototype, $Array(
        'name', REG_StringShort,
        'full', REG_FullRegName
    ));
  
    global.REG_NONE=new $Register(1);
    global.REG_FIRST=new $Register(2);
    global.REG_IMM8=new $Register(2);
    global.REG_IMM_BASE=new $Register(2);
    global.REG_IMM=new $Register(3);
    global.REG_IMM32=new $Register(4);
    global.REG_IMM_LAST=new $Register(4);
    global.REG_MEM=new $Register(5);
    global.REG_MEM_BASE=new $Register(5);
    global.REG_MEM_OFF8=new $Register(6);
    global.REG_MEM_OFF32=new $Register(7);
    global.REG_MEM_LAST=new $Register(7);
    global.REG_OFF8=new $Register(8);
    global.REG_OFF_BASE=new $Register(8);
    global.REG_OFF=new $Register(9);
    global.REG_OFF32=new $Register(10);
    global.REG_OFF_LAST=new $Register(10);
    global.REG_MODX=new $Register(11);
    global.REG_RBASE=new $Register(12);
    global.REG_MACHINE_BASE=new $Register(12);
    global.REG_APPLICATION_BASE=new $Register(12);
    global.REG_PHYSICAL_CONTEXT_BEGIN=new $Register(12);
    global.REG_GR_BASE=new $Register(12);
    global.REG_EDI=new $Register(12);
    global.REG_GDI=new $Register(12);
    global.REG_ESI=new $Register(13);
    global.REG_GSI=new $Register(13);
    global.REG_EBP=new $Register(14);
    global.REG_GBP=new $Register(14);
    global.REG_ESP=new $Register(15);
    global.REG_STACK_PTR=new $Register(15);
    global.REG_EBX=new $Register(16);
    global.REG_GBX=new $Register(16);
    global.REG_EDX=new $Register(17);
    global.REG_GDX=new $Register(17);
    global.REG_ECX=new $Register(18);
    global.REG_GCX=new $Register(18);
    global.REG_EAX=new $Register(19);
    global.REG_GAX=new $Register(19);
    global.REG_GR_LAST=new $Register(19);
    global.REG_SEG_BASE=new $Register(20);
    global.REG_SEG_CS=new $Register(20);
    global.REG_SEG_SS=new $Register(21);
    global.REG_SEG_DS=new $Register(22);
    global.REG_SEG_ES=new $Register(23);
    global.REG_SEG_FS=new $Register(24);
    global.REG_SEG_GS=new $Register(25);
    global.REG_SEG_LAST=new $Register(25);
    global.REG_EFLAGS=new $Register(26);
    global.REG_GFLAGS=new $Register(26);
    global.REG_EIP=new $Register(27);
    global.REG_INST_PTR=new $Register(27);
    global.REG_PHYSICAL_CONTEXT_END=new $Register(27);
    global.REG_AL=new $Register(28);
    global.REG_AH=new $Register(29);
    global.REG_AX=new $Register(30);
    global.REG_CL=new $Register(31);
    global.REG_CH=new $Register(32);
    global.REG_CX=new $Register(33);
    global.REG_DL=new $Register(34);
    global.REG_DH=new $Register(35);
    global.REG_DX=new $Register(36);
    global.REG_BL=new $Register(37);
    global.REG_BH=new $Register(38);
    global.REG_BX=new $Register(39);
    global.REG_BP=new $Register(40);
    global.REG_SI=new $Register(41);
    global.REG_DI=new $Register(42);
    global.REG_SP=new $Register(43);
    global.REG_FLAGS=new $Register(44);
    global.REG_IP=new $Register(45);
    global.REG_MM_BASE=new $Register(46);
    global.REG_MM0=new $Register(46);
    global.REG_MM1=new $Register(47);
    global.REG_MM2=new $Register(48);
    global.REG_MM3=new $Register(49);
    global.REG_MM4=new $Register(50);
    global.REG_MM5=new $Register(51);
    global.REG_MM6=new $Register(52);
    global.REG_MM7=new $Register(53);
    global.REG_MM_LAST=new $Register(53);
    global.REG_EMM_BASE=new $Register(54);
    global.REG_EMM0=new $Register(54);
    global.REG_EMM1=new $Register(55);
    global.REG_EMM2=new $Register(56);
    global.REG_EMM3=new $Register(57);
    global.REG_EMM4=new $Register(58);
    global.REG_EMM5=new $Register(59);
    global.REG_EMM6=new $Register(60);
    global.REG_EMM7=new $Register(61);
    global.REG_EMM_LAST=new $Register(61);
    global.REG_MXT=new $Register(62);
    global.REG_X87=new $Register(63);
    global.REG_XMM_BASE=new $Register(64);
    global.REG_FIRST_FP_REG=new $Register(64);
    global.REG_XMM0=new $Register(64);
    global.REG_XMM1=new $Register(65);
    global.REG_XMM2=new $Register(66);
    global.REG_XMM3=new $Register(67);
    global.REG_XMM4=new $Register(68);
    global.REG_XMM5=new $Register(69);
    global.REG_XMM6=new $Register(70);
    global.REG_XMM7=new $Register(71);
    global.REG_XMM_LAST=new $Register(71);
    global.REG_YMM_BASE=new $Register(72);
    global.REG_YMM0=new $Register(72);
    global.REG_YMM1=new $Register(73);
    global.REG_YMM2=new $Register(74);
    global.REG_YMM3=new $Register(75);
    global.REG_YMM4=new $Register(76);
    global.REG_YMM5=new $Register(77);
    global.REG_YMM6=new $Register(78);
    global.REG_YMM7=new $Register(79);
    global.REG_YMM_LAST=new $Register(79);
    global.REG_MXCSR=new $Register(80);
    global.REG_MXCSRMASK=new $Register(81);
    global.REG_ORIG_EAX=new $Register(82);
    global.REG_ORIG_GAX=new $Register(82);
    global.REG_DR_BASE=new $Register(83);
    global.REG_DR0=new $Register(83);
    global.REG_DR1=new $Register(84);
    global.REG_DR2=new $Register(85);
    global.REG_DR3=new $Register(86);
    global.REG_DR4=new $Register(87);
    global.REG_DR5=new $Register(88);
    global.REG_DR6=new $Register(89);
    global.REG_DR7=new $Register(90);
    global.REG_DR_LAST=new $Register(90);
    global.REG_CR_BASE=new $Register(91);
    global.REG_CR0=new $Register(91);
    global.REG_CR1=new $Register(92);
    global.REG_CR2=new $Register(93);
    global.REG_CR3=new $Register(94);
    global.REG_CR4=new $Register(95);
    global.REG_CR_LAST=new $Register(95);
    global.REG_TSSR=new $Register(96);
    global.REG_LDTR=new $Register(97);
    global.REG_TR_BASE=new $Register(98);
    global.REG_TR=new $Register(98);
    global.REG_TR3=new $Register(99);
    global.REG_TR4=new $Register(100);
    global.REG_TR5=new $Register(101);
    global.REG_TR6=new $Register(102);
    global.REG_TR7=new $Register(103);
    global.REG_TR_LAST=new $Register(103);
    global.REG_FPST_BASE=new $Register(104);
    global.REG_FPSTATUS_BASE=new $Register(104);
    global.REG_FPCW=new $Register(104);
    global.REG_FPSW=new $Register(105);
    global.REG_FPTAG=new $Register(106);
    global.REG_FPIP_OFF=new $Register(107);
    global.REG_FPIP_SEL=new $Register(108);
    global.REG_FPOPCODE=new $Register(109);
    global.REG_FPDP_OFF=new $Register(110);
    global.REG_FPDP_SEL=new $Register(111);
    global.REG_FPSTATUS_LAST=new $Register(111);
    global.REG_FPTAG_FULL=new $Register(112);
    global.REG_ST_BASE=new $Register(113);
    global.REG_ST0=new $Register(113);
    global.REG_ST1=new $Register(114);
    global.REG_ST2=new $Register(115);
    global.REG_ST3=new $Register(116);
    global.REG_ST4=new $Register(117);
    global.REG_ST5=new $Register(118);
    global.REG_ST6=new $Register(119);
    global.REG_ST7=new $Register(120);
    global.REG_ST_LAST=new $Register(120);
    global.REG_FPST_LAST=new $Register(120);
    global.REG_MACHINE_LAST=new $Register(120);
    global.REG_STATUS_FLAGS=new $Register(121);
    global.REG_DF_FLAG=new $Register(122);
    global.REG_APPLICATION_LAST=new $Register(122);
    global.REG_PIN_BASE=new $Register(123);
    global.REG_PIN_GR_BASE=new $Register(123);
    global.REG_PIN_EDI=new $Register(123);
    global.REG_PIN_GDI=new $Register(123);
    global.REG_PIN_ESI=new $Register(124);
    global.REG_PIN_EBP=new $Register(125);
    global.REG_PIN_ESP=new $Register(126);
    global.REG_PIN_STACK_PTR=new $Register(126);
    global.REG_PIN_EBX=new $Register(127);
    global.REG_PIN_EDX=new $Register(128);
    global.REG_PIN_GDX=new $Register(128);
    global.REG_PIN_ECX=new $Register(129);
    global.REG_PIN_GCX=new $Register(129);
    global.REG_PIN_EAX=new $Register(130);
    global.REG_PIN_GAX=new $Register(130);
    global.REG_PIN_AL=new $Register(131);
    global.REG_PIN_AH=new $Register(132);
    global.REG_PIN_AX=new $Register(133);
    global.REG_PIN_CL=new $Register(134);
    global.REG_PIN_CH=new $Register(135);
    global.REG_PIN_CX=new $Register(136);
    global.REG_PIN_DL=new $Register(137);
    global.REG_PIN_DH=new $Register(138);
    global.REG_PIN_DX=new $Register(139);
    global.REG_PIN_BL=new $Register(140);
    global.REG_PIN_BH=new $Register(141);
    global.REG_PIN_BX=new $Register(142);
    global.REG_PIN_BP=new $Register(143);
    global.REG_PIN_SI=new $Register(144);
    global.REG_PIN_DI=new $Register(145);
    global.REG_PIN_SP=new $Register(146);
    global.REG_PIN_X87=new $Register(147);
    global.REG_PIN_MXCSR=new $Register(148);
    global.REG_THREAD_ID=new $Register(149);
    global.REG_SEG_GS_VAL=new $Register(150);
    global.REG_SEG_FS_VAL=new $Register(151);
    global.REG_PIN_INDIRREG=new $Register(152);
    global.REG_PIN_IPRELADDR=new $Register(153);
    global.REG_PIN_SYSENTER_RESUMEADDR=new $Register(154);
    global.REG_PIN_VMENTER=new $Register(155);
    global.REG_PIN_T_BASE=new $Register(156);
    global.REG_PIN_T0=new $Register(156);
    global.REG_PIN_T1=new $Register(157);
    global.REG_PIN_T2=new $Register(158);
    global.REG_PIN_T3=new $Register(159);
    global.REG_PIN_T0L=new $Register(160);
    global.REG_PIN_T1L=new $Register(161);
    global.REG_PIN_T2L=new $Register(162);
    global.REG_PIN_T3L=new $Register(163);
    global.REG_PIN_T0W=new $Register(164);
    global.REG_PIN_T1W=new $Register(165);
    global.REG_PIN_T2W=new $Register(166);
    global.REG_PIN_T3W=new $Register(167);
    global.REG_PIN_T0D=new $Register(168);
    global.REG_PIN_T1D=new $Register(169);
    global.REG_PIN_T2D=new $Register(170);
    global.REG_PIN_T3D=new $Register(171);
    global.REG_PIN_T_LAST=new $Register(171);
    global.REG_SEG_GS_BASE=new $Register(172);
    global.REG_SEG_FS_BASE=new $Register(173);
    global.REG_INST_BASE=new $Register(174);
    global.REG_INST_SCRATCH_BASE=new $Register(174);
    global.REG_INST_G0=new $Register(174);
    global.REG_INST_TOOL_FIRST=new $Register(174);
    global.REG_INST_G1=new $Register(175);
    global.REG_INST_G2=new $Register(176);
    global.REG_INST_G3=new $Register(177);
    global.REG_INST_G4=new $Register(178);
    global.REG_INST_G5=new $Register(179);
    global.REG_INST_G6=new $Register(180);
    global.REG_INST_G7=new $Register(181);
    global.REG_INST_G8=new $Register(182);
    global.REG_INST_G9=new $Register(183);
    global.REG_INST_G10=new $Register(184);
    global.REG_INST_G11=new $Register(185);
    global.REG_INST_G12=new $Register(186);
    global.REG_INST_G13=new $Register(187);
    global.REG_INST_G14=new $Register(188);
    global.REG_INST_G15=new $Register(189);
    global.REG_INST_G16=new $Register(190);
    global.REG_INST_G17=new $Register(191);
    global.REG_INST_G18=new $Register(192);
    global.REG_INST_G19=new $Register(193);
    global.REG_INST_TOOL_LAST=new $Register(193);
    global.REG_BUF_BASE0=new $Register(194);
    global.REG_BUF_BASE1=new $Register(195);
    global.REG_BUF_BASE2=new $Register(196);
    global.REG_BUF_BASE3=new $Register(197);
    global.REG_BUF_BASE4=new $Register(198);
    global.REG_BUF_BASE5=new $Register(199);
    global.REG_BUF_BASE6=new $Register(200);
    global.REG_BUF_BASE7=new $Register(201);
    global.REG_BUF_BASE8=new $Register(202);
    global.REG_BUF_BASE9=new $Register(203);
    global.REG_BUF_LAST=new $Register(203);
    global.REG_BUF_END0=new $Register(204);
    global.REG_BUF_END1=new $Register(205);
    global.REG_BUF_END2=new $Register(206);
    global.REG_BUF_END3=new $Register(207);
    global.REG_BUF_END4=new $Register(208);
    global.REG_BUF_END5=new $Register(209);
    global.REG_BUF_END6=new $Register(210);
    global.REG_BUF_END7=new $Register(211);
    global.REG_BUF_END8=new $Register(212);
    global.REG_BUF_END9=new $Register(213);
    global.REG_BUF_ENDLAST=new $Register(213);
    global.REG_INST_SCRATCH_LAST=new $Register(213);
    global.REG_INST_COND=new $Register(214);
    global.REG_INST_LAST=new $Register(214);
    global.REG_INST_T0=new $Register(215);
    global.REG_INST_T0L=new $Register(216);
    global.REG_INST_T0W=new $Register(217);
    global.REG_INST_T0D=new $Register(218);
    global.REG_INST_T1=new $Register(219);
    global.REG_INST_T1L=new $Register(220);
    global.REG_INST_T1W=new $Register(221);
    global.REG_INST_T1D=new $Register(222);
    global.REG_INST_T2=new $Register(223);
    global.REG_INST_T2L=new $Register(224);
    global.REG_INST_T2W=new $Register(225);
    global.REG_INST_T2D=new $Register(226);
    global.REG_INST_T3=new $Register(227);
    global.REG_INST_T3L=new $Register(228);
    global.REG_INST_T3W=new $Register(229);
    global.REG_INST_T3D=new $Register(230);
    global.REG_INST_PRESERVED_PREDICATE=new $Register(231);
    global.REG_FLAGS_BEFORE_AC_CLEARING=new $Register(232);
    global.REG_PIN_BRIDGE_ORIG_SP=new $Register(233);
    global.REG_PIN_BRIDGE_APP_IP=new $Register(234);
    global.REG_PIN_BRIDGE_SP_BEFORE_ALIGN=new $Register(235);
    global.REG_PIN_BRIDGE_SP_BEFORE_CALL=new $Register(236);
    global.REG_PIN_BRIDGE_MARSHALLING_FRAME=new $Register(237);
    global.REG_PIN_BRIDGE_ON_STACK_CONTEXT_FRAME=new $Register(238);
    global.REG_PIN_BRIDGE_ON_STACK_CONTEXT_SP=new $Register(239);
    global.REG_PIN_BRIDGE_MULTI_MEMORYACCESS_FRAME=new $Register(240);
    global.REG_PIN_BRIDGE_MULTI_MEMORYACCESS_SP=new $Register(241);
    global.REG_PIN_BRIDGE_TRANS_MEMORY_CALLBACK_FRAME=new $Register(242);
    global.REG_PIN_BRIDGE_TRANS_MEMORY_CALLBACK_SP=new $Register(243);
    global.REG_PIN_TRANS_MEMORY_CALLBACK_READ_ADDR=new $Register(244);
    global.REG_PIN_TRANS_MEMORY_CALLBACK_READ2_ADDR=new $Register(245);
    global.REG_PIN_TRANS_MEMORY_CALLBACK_WRITE_ADDR=new $Register(246);
    global.REG_PIN_BRIDGE_SPILL_AREA_CONTEXT_FRAME=new $Register(247);
    global.REG_PIN_BRIDGE_SPILL_AREA_CONTEXT_SP=new $Register(248);
    global.REG_PIN_SPILLPTR=new $Register(249);
    global.REG_PIN_GR_LAST=new $Register(249);
    global.REG_PIN_STATUS_FLAGS=new $Register(250);
    global.REG_PIN_DF_FLAG=new $Register(251);
    global.REG_PIN_FLAGS=new $Register(252);
    global.REG_PIN_XMM_BASE=new $Register(253);
    global.REG_PIN_XMM0=new $Register(253);
    global.REG_PIN_XMM1=new $Register(254);
    global.REG_PIN_XMM2=new $Register(255);
    global.REG_PIN_XMM3=new $Register(256);
    global.REG_PIN_XMM4=new $Register(257);
    global.REG_PIN_XMM5=new $Register(258);
    global.REG_PIN_XMM6=new $Register(259);
    global.REG_PIN_XMM7=new $Register(260);
    global.REG_PIN_XMM8=new $Register(261);
    global.REG_PIN_XMM9=new $Register(262);
    global.REG_PIN_XMM10=new $Register(263);
    global.REG_PIN_XMM11=new $Register(264);
    global.REG_PIN_XMM12=new $Register(265);
    global.REG_PIN_XMM13=new $Register(266);
    global.REG_PIN_XMM14=new $Register(267);
    global.REG_PIN_XMM15=new $Register(268);
    global.REG_PIN_YMM_BASE=new $Register(269);
    global.REG_PIN_YMM0=new $Register(269);
    global.REG_PIN_YMM1=new $Register(270);
    global.REG_PIN_YMM2=new $Register(271);
    global.REG_PIN_YMM3=new $Register(272);
    global.REG_PIN_YMM4=new $Register(273);
    global.REG_PIN_YMM5=new $Register(274);
    global.REG_PIN_YMM6=new $Register(275);
    global.REG_PIN_YMM7=new $Register(276);
    global.REG_PIN_YMM_LAST=new $Register(276);
    global.REG_PIN_YMM8=new $Register(277);
    global.REG_PIN_YMM9=new $Register(278);
    global.REG_PIN_YMM10=new $Register(279);
    global.REG_PIN_YMM11=new $Register(280);
    global.REG_PIN_YMM12=new $Register(281);
    global.REG_PIN_YMM13=new $Register(282);
    global.REG_PIN_YMM14=new $Register(283);
    global.REG_PIN_YMM15=new $Register(284);
    global.REG_PIN_LAST=new $Register(284);
    global.REG_LAST=new $Register(285);
}

/************************************************************************/
/* thread object                                                        */
/************************************************************************/
global.Thread = function(id,tid,tuid,isapp) {
    if (!%_IsConstructCall()) {
        return new $Thread(id,tid,tuid,isapp);
    }
    
    //we're allocating a new internal thread
    if (IS_FUNCTION(id)) {
        this.callback = id;
        this.args = tid;
        //tid is an array of arguments to pass to the function
        var data = global.SpawnThread(this);
        if (IS_NULL_OR_UNDEFINED(data))
            throw "Spawn new thread failed";
        
        this.threadId = data[0];
        this.threadUid = data[1];
        this.tid = -1;
        this.appThread = false;
        this.spawnedThread = true;
    } else {
        this.callback = {};
        this.args = {};
        this.threadId = tid;
        this.threadUid = tuid;
        this.tid = id;
        this.appThread = isapp;
        this.spawnedThread = false;
    }
    
    this.type = "Thread";
}
var $Thread = global.Thread;
global.threads = new $Object();
var $threads_spawned = new $Object();
var $threads_spawned_finished = new $Object();

global.spawnedThreadDispatcher = function(threadobj) {
    //add the internally spawned thread to the list of threads
    global.threads[threadobj.threadId] = threadobj;
    $threads_spawned[threadobj.threadId]=true;;
    
    $current.thread = threadobj;
    threadobj.callback.apply(null, threadobj.args);
    $current.thread = {};
    
    $threads_spawned_finished[threadobj.threadId]=true;;
    global.removeThread(threadobj.threadId);
}

global.addThread = function(id,tid,tuid,isapp) {
    var t = new $Thread(id,tid,tuid,isapp);
    global.threads[tid] = t;
}

global.removeThread = function(tid) {
    delete global.threads[tid];
}

function ThreadWaitToFinish(timeout) {
    if (!this.spawnedThread)
        throw "We can only wait for our own internally spawned threads";
    
    //the thread has not started yet
    if (!(this.threadId in $threads_spawned))
        return false;
    
    if (IS_NULL_OR_UNDEFINED(timeout))
        timeout = 0;

    var current = 0;
    while (!(this.threadId in $threads_spawned_finished)) {
        global.PIN_Sleep(200);
        current += 200;
        if (timeout && current >= timeout)
            return false;
    }
    
    return true;
}

function ThreadWaitToStart(timeout) {
    if (!this.spawnedThread)
        throw "We can only wait for our own internally spawned threads";
    
    if (IS_NULL_OR_UNDEFINED(timeout))
        timeout = 0;

    var current = 0;
    while (!(this.threadId in $threads_spawned)) {
        global.PIN_Sleep(200);
        current += 200;
        if (timeout && current >= timeout)
            return false;
    }
    
    return true;
}

function ThreadToString() {
    return "[Thread (os=" + this.tid + ", threadId=" + this.threadId + ")]";
}

function ThreadListToString() {
    var ret=new $Array();
    for (var x in global.threads)
        ret.push(global.threads[x].toString());
    return ret.toString();
}

function SetupThread() {
    %FunctionSetInstanceClassName($Thread, 'Thread');
    %SetProperty($Thread.prototype, "constructor", $Thread, DONT_ENUM);
    InstallFunctions($Thread.prototype, DONT_ENUM, $Array(
        "wait", ThreadWaitToFinish,
        "waitStart", ThreadWaitToStart,
        "toString", ThreadToString
    ));
    
    InstallFunctions(global.threads, DONT_ENUM, $Array(
        "toString", ThreadListToString
    ));
}


/************************************************************************/
/* AnalysisFunction object                                              */
/************************************************************************/
global.AnalysisFunction = function(fun,init,dtor,args) {
    if (!%_IsConstructCall()) {
        return new $AnalysisFunction(fun,init,dtor,args);
    }
    
    if (IS_FUNCTION(fun))
        fun = fun.toString();
    if (!IS_STRING(fun))
        throw "function callback is not a function source string";
    
    if (IS_NULL_OR_UNDEFINED(args))
        args = new $ArgsArray();

    if (IS_NULL_OR_UNDEFINED(init))
        init = "function() {}";
    else if (IS_FUNCTION(init))
        init = init.toString();
    if (!IS_STRING(init))
        throw "initialization callback is not a function source string";
        
    if (IS_NULL_OR_UNDEFINED(dtor))
        dtor = "function() {}";
    else if (IS_FUNCTION(dtor))
        dtor = dtor.toString();
    if (!IS_STRING(dtor))
        throw "destructor callback is not a function source string";
        
    this.external = global.createAF(fun, init, dtor, args.external);
    this.callback = fun;
    this.init = init;
    this.dtor = dtor;
    this.args = args;
    
    this.type = "AnalysisFunction";
}
var $AnalysisFunction = global.AnalysisFunction;

function AnalysisFunctionEnabledGetter() {
    return global.getEnabledAF(this.external);
}

function AnalysisFunctionEnabledSetter(val) {
    var tmp = ToBoolean(val);
    return global.setEnabledAF(this.external, tmp);
}

var $afs = new $Object();
global.AfSetup = function(buffer, require) {
	var passbuffer_offset = 11 * 4;
	var afs = $afs;
	var arg0 = 0;
	var arg1 = 0;
	var arg2 = 0;
	var arg3 = 0;
	var arg4 = 0;
	var arg5 = 0;
	var arg6 = 0;
	var arg7 = 0;
	var arg8 = 0;
	var arg9 = 0;
	
	while (true) {
		%_ReturnContext(buffer);
		
		var action = %_ReadPointer(buffer + passbuffer_offset);
		var afid = %_ReadPointer(buffer + passbuffer_offset + 4);
	
		if (action == 0) {
			//prepare AF
			var cback_ptr = %_ReadPointer(buffer + passbuffer_offset + 8);
			var ctor_ptr = %_ReadPointer(buffer + passbuffer_offset + 12);
			var dtor_ptr = %_ReadPointer(buffer + passbuffer_offset + 16);
			var cback = %_ReadAsciiString(cback_ptr, 0);
			var ctor = %_ReadAsciiString(ctor_ptr, 0);
			var dtor = %_ReadAsciiString(dtor_ptr, 0);
			
			afs[afid] = new $Array();
			afs[afid][0] = GlobalEval("(" + cback + ")");
			afs[afid][1] = GlobalEval("(" + ctor + ")");
			afs[afid][2] = GlobalEval("(" + dtor + ")");
			
			//call ctor
			afs[afid][1](require);
		} else if (action == 1) {
			//call AF callback
			var numargs = %_ReadPointer(buffer + passbuffer_offset + 8);
			
			//I'm not being lazy, this is supposed to be faster than constructing an array of arguments
			switch (numargs) {
				default:
				case 10: arg9 = %_ReadPointer(buffer + passbuffer_offset + 12 + 9*4);
				case 9:  arg8 = %_ReadPointer(buffer + passbuffer_offset + 12 + 8*4);
				case 8:  arg7 = %_ReadPointer(buffer + passbuffer_offset + 12 + 7*4);
				case 7:  arg6 = %_ReadPointer(buffer + passbuffer_offset + 12 + 6*4);
				case 6:  arg5 = %_ReadPointer(buffer + passbuffer_offset + 12 + 5*4);
				case 5:  arg4 = %_ReadPointer(buffer + passbuffer_offset + 12 + 4*4);
				case 4:  arg3 = %_ReadPointer(buffer + passbuffer_offset + 12 + 3*4);
				case 3:  arg2 = %_ReadPointer(buffer + passbuffer_offset + 12 + 2*4);
				case 2:  arg1 = %_ReadPointer(buffer + passbuffer_offset + 12 + 1*4);
				case 1:  arg0 = %_ReadPointer(buffer + passbuffer_offset + 12 + 0*4);
				case 0:	 break;				
			}
			
			switch (numargs) {
				case 0: afs[afid][0](); break;
				case 1: afs[afid][0](arg0); break;
				case 2: afs[afid][0](arg0,arg1); break;
				case 3: afs[afid][0](arg0,arg1,arg2); break;
				case 4: afs[afid][0](arg0,arg1,arg2,arg3); break;
				case 5: afs[afid][0](arg0,arg1,arg2,arg3,arg4); break;
				case 6: afs[afid][0](arg0,arg1,arg2,arg3,arg4,arg5); break;
				case 7: afs[afid][0](arg0,arg1,arg2,arg3,arg4,arg5,arg6); break;
				case 8: afs[afid][0](arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7); break;
				case 9: afs[afid][0](arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8); break;
				default:
				case 10:afs[afid][0](arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9); break;
			}
		} else if (action == 2) {
			//call all destructor functions
			for (var idx in afs)
				afs[idx][2](require);
			
			//and exit the loop
			return;
		}
	}	
}

function SetupAnalysisFunction() {
    %FunctionSetInstanceClassName($AnalysisFunction, 'AnalysisFunction');
    %SetProperty($AnalysisFunction.prototype, "constructor", $AnalysisFunction, DONT_ENUM);
    
    %DefineOrRedefineAccessorProperty($AnalysisFunction.prototype, "enabled", AnalysisFunctionEnabledGetter, AnalysisFunctionEnabledSetter, DONT_DELETE);
}


/************************************************************************/
/* ArgsArray object                                                     */
/************************************************************************/
global.ArgsArray = function() {
    if (!%_IsConstructCall()) {
        throw "ArgsArray is a constructor function";
    }
    
    var len = %_ArgumentsLength();
    for (var x=0; x < len; x++)
		if (IS_ARRAY(arguments[x]) && IS_OBJECT(arguments[x][1]))
			arguments[x][1] = arguments[x][1].external;
    this.external = new global.IntArgsArray(arguments, len);
    
    this.type = "ArgsArray";
    this.args = arguments;
}
var $ArgsArray = global.ArgsArray;

function ArgsArrayToString() {
    var arr=new $Array();
    for (var x=0;x < this.args.length;x++) {
        var item = this.args[x];
        if (IS_ARRAY(item))
            arr.push("["+global.ArgTypes[item[0]] + ":0x" + item[1].toString(16)+"]");
        else
            arr.push("[0x"+item.toString(16)+"]");
    }
    
    return arr.toString();
}

function SetupArgsArray() {
    %FunctionSetInstanceClassName($ArgsArray, 'ArgsArray');
    %SetProperty($ArgsArray.prototype, "constructor", $ArgsArray, DONT_ENUM);
    
    InstallFunctions($ArgsArray.prototype, DONT_ENUM, $Array(
        "toString", ArgsArrayToString
    ));
}


function SetupHandyFunctions() {
    global.Number.prototype.hex = function() { return this.toString(16); }
    global.Number.prototype.bin = function() { return this.toString(2); }
    global.String.prototype.slice = function(start,stop) {
        if (!stop) stop = start;
        if (stop < 0) stop = this.length + stop;
        if (start < 0) start = this.length + start;
        return this.substr(start, stop-start+1);
    }
}

function SetupArgumentTypeConstants() {
    global.IARG_INVALID=0;
    global.IARG_ADDRINT=1;
    global.IARG_PTR=2;
    global.IARG_BOOL=3;
    global.IARG_UINT32=4;
    global.IARG_INST_PTR=5;
    global.IARG_REG_VALUE=6;
    global.IARG_REG_REFERENCE=7;
    global.IARG_REG_CONST_REFERENCE=8;
    global.IARG_MEMORYREAD_EA=9;
    global.IARG_MEMORYREAD2_EA=10;
    global.IARG_MEMORYWRITE_EA=11;
    global.IARG_MEMORYREAD_SIZE=12;
    global.IARG_MEMORYWRITE_SIZE=13;
    global.IARG_MEMORYREAD_PTR=14;
    global.IARG_MEMORYREAD2_PTR=15;
    global.IARG_MEMORYWRITE_PTR=16;
    global.IARG_MEMORYOP_PTR=17;
    global.IARG_MULTI_MEMORYACCESS_EA=18;
    global.IARG_BRANCH_TAKEN=19;
    global.IARG_BRANCH_TARGET_ADDR=20;
    global.IARG_FALLTHROUGH_ADDR=21;
    global.IARG_EXECUTING=22;
    global.IARG_FIRST_REP_ITERATION=23;
    global.IARG_PREDICATE=24;
    global.IARG_STACK_VALUE=25;
    global.IARG_STACK_REFERENCE=26;
    global.IARG_MEMORY_VALUE=27;
    global.IARG_MEMORY_REFERENCE=28;
    global.IARG_SYSCALL_NUMBER=29;
    global.IARG_SYSARG_REFERENCE=30;
    global.IARG_SYSARG_VALUE=31;
    global.IARG_SYSRET_VALUE=32;
    global.IARG_SYSRET_ERRNO=33;
    global.IARG_FUNCARG_CALLSITE_REFERENCE=34;
    global.IARG_FUNCARG_CALLSITE_VALUE=35;
    global.IARG_FUNCARG_ENTRYPOINT_REFERENCE=36;
    global.IARG_FUNCARG_ENTRYPOINT_VALUE=37;
    global.IARG_FUNCRET_EXITPOINT_REFERENCE=38;
    global.IARG_FUNCRET_EXITPOINT_VALUE=39;
    global.IARG_RETURN_IP=40;
    global.IARG_ORIG_FUNCPTR=41;
    global.IARG_PROTOTYPE=42;
    global.IARG_THREAD_ID=43;
    global.IARG_CONTEXT=44;
    global.IARG_CONST_CONTEXT=45;
    global.IARG_PARTIAL_CONTEXT=46;
    global.IARG_PRESERVE=47;
    global.IARG_RETURN_REGS=48;
    global.IARG_CALL_ORDER=49;
    global.IARG_REG_NAT_VALUE=50;
    global.IARG_REG_OUTPUT_FRAME_VALUE=51;
    global.IARG_REG_OUTPUT_FRAME_REFERENCE=52;
    global.IARG_IARGLIST=53;
    global.IARG_FAST_ANALYSIS_CALL=54;
    global.IARG_SYSCALL_ARG0=55;
    global.IARG_SYSCALL_ARGBASE=55;
    global.IARG_SYSCALL_ARG1=56;
    global.IARG_SYSCALL_ARG2=57;
    global.IARG_SYSCALL_ARG3=58;
    global.IARG_SYSCALL_ARG4=59;
    global.IARG_SYSCALL_ARG5=60;
    global.IARG_SYSCALL_ARGLAST=60;
    global.IARG_G_RESULT0=61;
    global.IARG_G_RETBASE=61;
    global.IARG_G_RESULTLAST=61;
    global.IARG_G_ARG0_CALLEE=62;
    global.IARG_G_ARGBASE_CALLEE=62;
    global.IARG_G_ARG1_CALLEE=63;
    global.IARG_G_ARG2_CALLEE=64;
    global.IARG_G_ARG3_CALLEE=65;
    global.IARG_G_ARG4_CALLEE=66;
    global.IARG_G_ARG5_CALLEE=67;
    global.IARG_G_ARGLAST_CALLEE=67;
    global.IARG_G_ARG0_CALLER=68;
    global.IARG_G_ARGBASE_CALLER=68;
    global.IARG_G_ARG1_CALLER=69;
    global.IARG_G_ARG2_CALLER=70;
    global.IARG_G_ARG3_CALLER=71;
    global.IARG_G_ARG4_CALLER=72;
    global.IARG_G_ARG5_CALLER=73;
    global.IARG_G_ARGLAST_CALLER=73;
    global.IARG_MEMORYOP_EA=74;
    global.IARG_MEMORYOP_MASKED_ON=75;
    global.IARG_TSC=76;
    global.IARG_FILE_NAME=77;
    global.IARG_LINE_NO=78;

    global.ArgTypes = new $Object();
    global.ArgTypes[0]="IARG_INVALID";
    global.ArgTypes[1]="IARG_ADDRINT";
    global.ArgTypes[2]="IARG_PTR";
    global.ArgTypes[3]="IARG_BOOL";
    global.ArgTypes[4]="IARG_UINT32";
    global.ArgTypes[5]="IARG_INST_PTR";
    global.ArgTypes[6]="IARG_REG_VALUE";
    global.ArgTypes[7]="IARG_REG_REFERENCE";
    global.ArgTypes[8]="IARG_REG_CONST_REFERENCE";
    global.ArgTypes[9]="IARG_MEMORYREAD_EA";
    global.ArgTypes[10]="IARG_MEMORYREAD2_EA";
    global.ArgTypes[11]="IARG_MEMORYWRITE_EA";
    global.ArgTypes[12]="IARG_MEMORYREAD_SIZE";
    global.ArgTypes[13]="IARG_MEMORYWRITE_SIZE";
    global.ArgTypes[14]="IARG_MEMORYREAD_PTR";
    global.ArgTypes[15]="IARG_MEMORYREAD2_PTR";
    global.ArgTypes[16]="IARG_MEMORYWRITE_PTR";
    global.ArgTypes[17]="IARG_MEMORYOP_PTR";
    global.ArgTypes[18]="IARG_MULTI_MEMORYACCESS_EA";
    global.ArgTypes[19]="IARG_BRANCH_TAKEN";
    global.ArgTypes[20]="IARG_BRANCH_TARGET_ADDR";
    global.ArgTypes[21]="IARG_FALLTHROUGH_ADDR";
    global.ArgTypes[22]="IARG_EXECUTING";
    global.ArgTypes[23]="IARG_FIRST_REP_ITERATION";
    global.ArgTypes[24]="IARG_PREDICATE";
    global.ArgTypes[25]="IARG_STACK_VALUE";
    global.ArgTypes[26]="IARG_STACK_REFERENCE";
    global.ArgTypes[27]="IARG_MEMORY_VALUE";
    global.ArgTypes[28]="IARG_MEMORY_REFERENCE";
    global.ArgTypes[29]="IARG_SYSCALL_NUMBER";
    global.ArgTypes[30]="IARG_SYSARG_REFERENCE";
    global.ArgTypes[31]="IARG_SYSARG_VALUE";
    global.ArgTypes[32]="IARG_SYSRET_VALUE";
    global.ArgTypes[33]="IARG_SYSRET_ERRNO";
    global.ArgTypes[34]="IARG_FUNCARG_CALLSITE_REFERENCE";
    global.ArgTypes[35]="IARG_FUNCARG_CALLSITE_VALUE";
    global.ArgTypes[36]="IARG_FUNCARG_ENTRYPOINT_REFERENCE";
    global.ArgTypes[37]="IARG_FUNCARG_ENTRYPOINT_VALUE";
    global.ArgTypes[38]="IARG_FUNCRET_EXITPOINT_REFERENCE";
    global.ArgTypes[39]="IARG_FUNCRET_EXITPOINT_VALUE";
    global.ArgTypes[40]="IARG_RETURN_IP";
    global.ArgTypes[41]="IARG_ORIG_FUNCPTR";
    global.ArgTypes[42]="IARG_PROTOTYPE";
    global.ArgTypes[43]="IARG_THREAD_ID";
    global.ArgTypes[44]="IARG_CONTEXT";
    global.ArgTypes[45]="IARG_CONST_CONTEXT";
    global.ArgTypes[46]="IARG_PARTIAL_CONTEXT";
    global.ArgTypes[47]="IARG_PRESERVE";
    global.ArgTypes[48]="IARG_RETURN_REGS";
    global.ArgTypes[49]="IARG_CALL_ORDER";
    global.ArgTypes[50]="IARG_REG_NAT_VALUE";
    global.ArgTypes[51]="IARG_REG_OUTPUT_FRAME_VALUE";
    global.ArgTypes[52]="IARG_REG_OUTPUT_FRAME_REFERENCE";
    global.ArgTypes[53]="IARG_IARGLIST";
    global.ArgTypes[54]="IARG_FAST_ANALYSIS_CALL";
    global.ArgTypes[55]="IARG_SYSCALL_ARG0";
    global.ArgTypes[55]="IARG_SYSCALL_ARGBASE";
    global.ArgTypes[56]="IARG_SYSCALL_ARG1";
    global.ArgTypes[57]="IARG_SYSCALL_ARG2";
    global.ArgTypes[58]="IARG_SYSCALL_ARG3";
    global.ArgTypes[59]="IARG_SYSCALL_ARG4";
    global.ArgTypes[60]="IARG_SYSCALL_ARG5";
    global.ArgTypes[60]="IARG_SYSCALL_ARGLAST";
    global.ArgTypes[61]="IARG_G_RESULT0";
    global.ArgTypes[61]="IARG_G_RETBASE";
    global.ArgTypes[61]="IARG_G_RESULTLAST";
    global.ArgTypes[62]="IARG_G_ARG0_CALLEE";
    global.ArgTypes[62]="IARG_G_ARGBASE_CALLEE";
    global.ArgTypes[63]="IARG_G_ARG1_CALLEE";
    global.ArgTypes[64]="IARG_G_ARG2_CALLEE";
    global.ArgTypes[65]="IARG_G_ARG3_CALLEE";
    global.ArgTypes[66]="IARG_G_ARG4_CALLEE";
    global.ArgTypes[67]="IARG_G_ARG5_CALLEE";
    global.ArgTypes[67]="IARG_G_ARGLAST_CALLEE";
    global.ArgTypes[68]="IARG_G_ARG0_CALLER";
    global.ArgTypes[68]="IARG_G_ARGBASE_CALLER";
    global.ArgTypes[69]="IARG_G_ARG1_CALLER";
    global.ArgTypes[70]="IARG_G_ARG2_CALLER";
    global.ArgTypes[71]="IARG_G_ARG3_CALLER";
    global.ArgTypes[72]="IARG_G_ARG4_CALLER";
    global.ArgTypes[73]="IARG_G_ARG5_CALLER";
    global.ArgTypes[73]="IARG_G_ARGLAST_CALLER";
    global.ArgTypes[74]="IARG_MEMORYOP_EA";
    global.ArgTypes[75]="IARG_MEMORYOP_MASKED_ON";
    global.ArgTypes[76]="IARG_TSC";
    global.ArgTypes[77]="IARG_FILE_NAME";
    global.ArgTypes[78]="IARG_LINE_NO";
}

function SetupPIN() {
  %CheckIsBootstrapping();
  
  SetupCurrent();
  SetupProcess();
  SetupControl();
  SetupRoutines();
  SetupSemaphore();
  SetupImage();
  SetupSection();
  SetupSymbol();
  SetupRoutine();
  SetupInstruction();
  SetupTrace();
  SetupBBL();
  SetupEvents();
  SetupContext();
  SetupRegister();
  SetupThread();
  SetupAnalysisFunction();
  SetupArgsArray();
  SetupHandyFunctions();
  SetupArgumentTypeConstants();
}

SetupPIN();
