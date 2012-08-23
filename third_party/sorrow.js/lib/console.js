/*
 copyright 2012 sam l'ecuyer
*/
var io = require('io');
exports.log = function() {
    var args = Array.prototype.slice.call(arguments);
    io.stdout.writeLine(args.join());
}
