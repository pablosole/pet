/*
 copyright 2012 sam l'ecuyer
*/

var io = require('io');
var b  = require('binary');
var fs = internals.fs;

function open(path, mode) {
    var stream = new io.Stream(path, mode);
    if (mode.match(/b/)) return stream;
    return new io.TextStream(stream, {});
}

function copy(source, target) {
    var targetIo = open(target, "wb");
    var sourceIo = open(source, "rb");
    sourceIo.copy(targetIo);
    sourceIo.close();
    targetIo.close();
}

/* Path methods */

function workingDirectoryPath() {
    return absolute(fs.workingDirectory());
}

/* Paths as text */

function join() {
     var args = Array.prototype.slice.call(arguments);
     return args.join('/');
}

function split(path) {
    var parts = path.split('/');
    return parts;
}

function normal(path) {
    var parts = split(path);
    var newParts = parts.filter(function(e) {
        return e !== '.';
    });
    return join.apply(this,newParts);
}

function absolute(path) {
    return normal(join(fs.workingDirectory(), path));
}

function directory(path) {
    var parts = split(path);
    return join.apply(this, parts.slice(0, parts.length-1));
}

function base(path, ext) {
    var filename = split(path).pop();
    return filename.replace(new RegExp('.'+ext+'$'), '');
}

function extension(path) {
    var filename = split(path).pop();
    var exts = filename.match(/\.[^\.]+?$/);
    return exts? exts.pop().replace(/^[.]*/, ''): '';
}

/* Path Type */

function Path() {
    this._path = join.apply(this,arguments);
}
Path.prototype.toString = function() {
    return this._path;
}

exports.Path = Path;

/* A/0 methods */
exports.changeWorkingDirectory = fs.changeWorkingDirectory;
exports.workingDirectory = fs.workingDirectory;
exports.move = fs.move;
exports.remove = fs.remove;
exports.touch = fs.touch;
exports.makeDirectory = fs.mkdir;
exports.removeDirectory = fs.rmdir;
exports.canonical = fs.canonical;
exports.owner = fs.owner;
exports.changeOwner = fs.chown;
exports.group = fs.group;
exports.changeGroup = fs.chgroup;
exports.permissions = fs.perm;
exports.changePermissions = fs.chperm;
exports.exists = fs.exists;
exports.isFile = fs.isfile;
exports.isDirectory = fs.isdir;
exports.isLink = fs.islink;
exports.isReadable = fs.isreadable;
exports.isWriteable = fs.iswriteable;
exports.same = fs.same;
exports.sameFilesystem = fs.samefs;
exports.size = fs.size;
exports.lastModified = fs.lastmod;
exports.list = fs.list;
exports.symbolicLink = fs.slink;
exports.hardLink = fs.hlink;
exports.readLink = fs.rlink;

exports.open = open;
exports.copy = copy;
exports.split = split;
exports.join  = join;
exports.absolute = absolute;
exports.directory = directory;
exports.base = base;
exports.extension = extension;
