/*
 copyright 2012 sam l'ecuyer
*/

(function(internals) {
    var global = this;
        
    function Lib(id) {
        this.id = id;
        this.filename = id + '.js';
        this.exports = {};
        this.loaded = false;
    }
    
    Lib._sources = internals.stdlib;
    Lib._cache = {};
    
    Lib.require = function(ident) {
        if (ident == '_impl') 
            return internals;
        if (ident == 'stdlib') 
            return Lib;
            
        var lib = Lib.getCached(ident);
        if (lib) {
            return lib.exports;
        }
        
        lib = new Lib(ident);
        lib.cache();
        lib.load();
        lib.cache();
        return lib.exports;
    }
    
    Lib.prototype.cache = function() {
        Lib._cache[this.id] = this;
    };
    
    Lib.getCached = function(id) {
        return Lib._cache[id];
    };
    
    Lib.exists = function(id) {
        return !!Lib._sources[id];
    };
    
    Lib.wrap = function(script) {
        return '(function(exports, module, require, internals) { '+script +'\n});';
    };
    
    Lib.prototype.load = function() {
        var script = Lib._sources[this.id];
        var wrappedScript = Lib.wrap(script);
        var fn = internals.compile(wrappedScript, this.filename);
        fn.call(this, this.exports, {id: this.id}, Lib.require, internals);
        this.loaded = true;
    };
    
    internals.fire = Lib.require('event').fire;
    
    internals.loadMain = function() {
    
      var Module = Lib.require('module').Module;
      var io     = Lib.require('io');
      var args   = Lib.require('_impl').args;
    
      if (args.length < 1) {
          io.stdout.writeLine('there was some input expected.  try it with a file');
          return;
      }
    
      return Module.runProg(args[0]);
    }
    
});
