
var Plugin = require('plugins').Plugin;
var fs   = require('fs');
var dir  = fs.directory(module.uri);

function resolve(path) {
    return fs.join(dir, path);
}

var plugin;

function getPlugin() {
    if (!plugin) {
        var path = resolve('plugin.dylib');
        plugin = new Plugin(path);
    }
    return plugin;
}

var parts = getPlugin().load();

exports.MySql = parts.MySql;