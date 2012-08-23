/*
 copyright 2012 sam l'ecuyer
*/

var Binary     = internals.Binary;
var ByteArray  = internals.ByteArray;
var ByteString = internals.ByteString;

// TODO: organize this

ByteArray.prototype.toByteArray = function() {
    if (arguments.length == 0)
        return new ByteArray(this);
    return Binary.prototype.toByteArray.apply(this, arguments);
};

ByteArray.prototype.copy = function(target, start, stop, targetStart) {
    if(!start) start = 0;
    if(!stop)  stop = this.length;
    if(!targetStart) targetStart = 0;
    
    for (var i = start; i < stop; i++) {
        target[i+targetStart] = this[i];
    }
};

ByteArray.prototype.fill = function(value, start, stop) {
    if(!start) start = 0;
    if(!stop)  stop = this.length;
    var filler = (typeof value == 'number')? value :value[0];
    
    for (var i = start; i < stop; i++) {
        this[i] = filler;
    }
};


ByteArray.prototype.toString = function() {
    return '[ByteArray '+this.length+']';
};

ByteArray.prototype.toSource = function() {
    return 'ByteArray(['+this.toArray()+'])';
};

ByteArray.prototype.toByteString = function() {
    if (arguments.length == 0)
        return new ByteString(this);
    return Binary.prototype.toByteString.apply(this, arguments);
};

ByteString.prototype.toByteString = function() {
    if (arguments.length == 0)
        return this;
    return Binary.prototype.toByteString.apply(this, arguments);
};

ByteString.prototype.toString = function() {
    return '[ByteString '+this.length+']';
};


ByteString.prototype.copy = function(target, start, stop, targetStart) {
    if(!start) start = 0;
    if(!stop)  stop = this.length;
    if(!targetStart) targetStart = 0;
    
    for (var i = start; i < stop; i++) {
        target[i+targetStart] = this.codeAt(i);
    }
};

ByteString.join = function(array, delimeter) {
    var newArray = [array[0]];
    for (var i = 1; i < array.length; i++) {
        newArray.push(delimeter);
        newArray.push(array[i]);
    }
    return new ByteString(newArray);
};


Binary.prototype.get     = function(i) { return this[i]; };
ByteString.prototype.valueAt = function(i) { return this[i]; };
ByteString.prototype.byteAt  = function(i) { return this[i]; };

ByteArray.prototype.valueAt  = function(i) { 
    return new ByteString([this[i]]); 
};
ByteArray.prototype.byteAt  = function(i) { 
    return new ByteString([this[i]]); 
};


exports.Binary    = Binary;
exports.ByteArray = ByteArray;
exports.ByteString = ByteString;
