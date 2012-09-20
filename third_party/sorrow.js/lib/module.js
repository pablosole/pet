/*
 copyright 2012 sam l'ecuyer
*/

var fs = require('fs');
var Lib = require('stdlib');

function Module(id, filename, parent, script) {
    this.id = id;
    this.filename = filename;
    this.parent = parent;
    this.exports = {};
    this.loaded = false;
    this.script = script;
}

Module._cache = {};
    
Module.prototype.cache = function() {
    Module._cache[this.filename] = this;
};
    
Module.getCached = function(filename) {
    return Module._cache[filename];
};
    
Module.wrap = function(script) {
    return '(function(exports, module, require) { '+script +'\n});';
};
    
Module.prototype.load = function() {
    var self = this;
    
    function require(path) {
        return self.require(path);
    }
    
    require.main = self.main;
    if (this.filename == "internal")
	var script = "(" + this.script + ")();";
    else
	var script = fs.open(this.filename,'rb').read().decodeToString('utf-8');

    script = script.replace(/^#!.*/, '');
    var wrappedScript = Module.wrap(script);
    var fn = internals.compile(wrappedScript, this.filename);
    fn.call(this, this.exports, {id: this.id, uri: this.filename}, require);
    this.loaded = true;
};

Module.runProg = function(script) {
    return Module._load(script, null, true);
}

Module.prototype.require = function(path) {
    return Module._load(path, this, false);
};

Module._resolvePath = function(script, parent) {
    var firstTerm = fs.split(script)[0];
    var isRelative = (firstTerm == '.' || firstTerm == '..');
    var parentPath = '';
    if (isRelative) {
        if (parent) {
            parentPath = fs.directory(parent.filename);
        } else { // this will only occur for the root module
            parentPath = fs.workingDirectory();
        }
    } else {
        if (Module.root) {
            parentPath = Module.root;
        } else { // this will only occur for the root module
            parentPath = fs.workingDirectory();
        }
    }
    if (script.indexOf('.js') == -1) {
        return fs.join(parentPath, script+'.js');
    } else {
        return fs.join(parentPath, script);
    }
}

Module._load = function(script, parent, isRoot) {
    if (Lib.exists(script)) {
        return Lib.require(script);
    }
    
    if (isRoot)
	var filename = script;
    else
        var filename = Module._resolvePath(script, parent);
    var canonical = fs.canonical(filename);
    var mod = Module.getCached(canonical);
    if (mod) {
        return mod.exports;
    }
    var modName = fs.base(script, 'js');
    mod = new Module(modName, canonical, parent);
    if (isRoot) {
        Module.root = fs.directory(filename);
        mod.main = modName;
    } else {
        mod.main = parent.main;
    }
    
    mod.cache();
    mod.load();
    mod.cache();
    return mod.exports;
}

exports.Module = Module;
