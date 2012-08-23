/*
 copyright 2012 sam l'ecuyer
*/

var RawStream = internals.RawStream;
var TextStream = internals.TextStream;
var b = require('binary');

RawStream.prototype.copy = function(target) {
    while (true) {
        var bs = this.read(null);
        if (bs.length == 0) break;
        target.write(bs);
        target.flush();
    }
}

TextStream.prototype.copy = function(target) {
    while (true) {
        try {
            var line = this.readLine();
            target.writeLine(line, true);
            target.raw.flush();
        } catch(e) {
            return;
        }
    }
}

TextStream.prototype.next = function() {
    var line = this.readLine();
    var len = line.length;
    if (len > 0) {
        if (line[len-1] == '\n') {
            line = line.substr(0, len-1);
        }
    }
    return line;
}

TextStream.prototype.readLines = function(target) {
    var lines = [];
    while (true) {
        var line = this.next();
        if (line.length > 0) {
            lines.push(line);
        } else {
            break;
        }
    }
    return lines;
}

exports.Stream = RawStream;
exports.TextStream = TextStream;
exports.stdin = internals.stdin;
exports.stdout = internals.stdout;
