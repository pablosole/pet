/*
 copyright 2012 sam l'ecuyer
*/

var log = require('console').log;
exports.run = function(exports) {
    var pass = 0,
        fail = 0;
    for (x in exports) {
        var mthd = exports[x];
        if (typeof mthd != 'function') continue;
        if (!x.match(/^test.+/i)) continue;
        try {
            mthd();
            pass++;
        } catch(e) {
            log('Failure: ' + x);
            log(e);
            fail++;
        }
    }
    log('Results: '+pass+' test pass, '+fail+' tests fail');
    return fail;
};