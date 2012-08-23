var stdout = require('system').stdout;

exports.sprintf = function() {
    if (arguments.length < 2) return;
    var args = Array.prototype.slice.call(arguments);
    
    var format = args[1];
    var matchIndex = 2;
    format = format.replace(/%s/, function(match) {
        return (matchIndex < args.length)? args[matchIndex++]: match;
    });
    var leftOverArgs = [];
    if (matchIndex < args.length) {
        leftOverArgs = args.slice(matchIndex);
    }
    leftOverArgs.unshift(format);
    args[0].writeLine(leftOverArgs.join(' '), true);
}

exports.printf = function() {
    if (arguments.length == 0) return;
    var args = Array.prototype.slice.call(arguments);
    args.unshift(stdout);
    exports.sprintf.apply(this,args);
}