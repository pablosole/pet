var MySql = require('./mysql').MySql;
var util = require('util');

var conn = new MySql();
conn.realConnect('localhost', 'grubhub', 'sywtbmc', 'grubhub');

var results = conn.realQuery('select uid from customer');

var row = results.fetchRow();
require('console').log(results.numRows());
while(row) {
    util.printf("%s\n",row.uid);
    row = results.fetchRow();
}