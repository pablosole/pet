// build with:
//  g++ -dynamiclib -undefined suppress -flat_namespace \
    `mysql_config --cflags --libs` -I../../deps/v8/include/ \
    main.cpp -o plugin.dylib

#include "../sorrow_ext.cpp"
#include <mysql.h>

namespace sorrow_mysql {
	using namespace v8;
    
    Persistent<Function> mysqlResult;
    Persistent<FunctionTemplate> mysqlResult_t;
    
    void ExternalWeakMysqlCallback(Persistent<Value> object, void* conn) {
        mysql_close((MYSQL*)conn);
		object.Dispose();
	}
    
    void ExternalWeakResultCallback(Persistent<Value> object, void* res) {
        mysql_free_result((MYSQL_RES*)res);
		object.Dispose();
	}
    
    /*
        MySql connection functions
     */
    V8_FUNCTN(MySQL) {
        HandleScope scope;
		Handle<Object> connection = args.This();
        
        MYSQL *conn;
        conn = mysql_init(NULL);
        if (conn == NULL) {
            return THROW(ERR(V8_STR("Could not init")))
        }
        
		Persistent<Object> persistent_conn = Persistent<Object>::New(connection);
		persistent_conn.MakeWeak(conn, ExternalWeakMysqlCallback);
		persistent_conn.MarkIndependent();
		
		connection->SetPointerInInternalField(0, conn);
		return connection;
	}
    
    V8_FUNCTN(RealConnect) {
        HandleScope scope;
		Handle<Object> connection = args.This();
        MYSQL *conn = (MYSQL*)connection->GetPointerFromInternalField(0);
        char *host = NULL, *user = NULL, *passwd = NULL, *db = NULL;
        int port = 0;

        String::Utf8Value hostname(args[0]->ToString());
        String::Utf8Value username(args[1]->ToString());
        String::Utf8Value pswd(args[2]->ToString());
        String::Utf8Value dbname(args[3]->ToString());
        port = args[4]->IntegerValue();
        
        if (args.Length() > 0) host = *hostname;
        if (args.Length() > 1) user = *username;
        if (args.Length() > 2) passwd = *pswd;
        if (args.Length() > 3) db = *dbname;
        
        if (mysql_real_connect(conn, host, user, passwd, db, port, NULL, 0) == NULL) {
            return THROW(ERR(V8_STR(mysql_error(conn))))
        }
        
		return connection;
	}
    
    V8_FUNCTN(RealQuery) {
        HandleScope scope;
		Handle<Object> connection = args.This();
        MYSQL *conn = (MYSQL*)connection->GetPointerFromInternalField(0);
        
        String::Utf8Value q(args[0]->ToString());
        
        if (mysql_real_query(conn, *q, q.length()) != 0) {
            return THROW(ERR(V8_STR(mysql_error(conn))))
        }
        
        MYSQL_RES *res = mysql_store_result(conn);
		Local<Value> resultArgs[1] = { External::New((void*)res) };
		Local<Value> result = mysqlResult->NewInstance(1, resultArgs);
        return scope.Close(result);
	}

    /*
        ResultSet functions
     */
    V8_FUNCTN(Result) {
        HandleScope scope;
		Handle<Object> result = args.This();
        
        MYSQL_RES *res = reinterpret_cast<MYSQL_RES*>(External::Unwrap(args[0]));
        
		Persistent<Object> persistent_res = Persistent<Object>::New(result);
		persistent_res.MakeWeak(res, ExternalWeakResultCallback);
		persistent_res.MarkIndependent();
		
		result->SetPointerInInternalField(0, res);
		return result;
	}
    
    V8_FUNCTN(NumRows) {
        HandleScope scope;
		Handle<Object> result = args.This();
        MYSQL_RES *res = (MYSQL_RES*)result->GetPointerFromInternalField(0);
        int num = mysql_num_rows(res);
        return Integer::New(num);
	}
    
    V8_FUNCTN(FetchRow) {
        HandleScope scope;
		Handle<Object> result = args.This();
        MYSQL_RES *res = (MYSQL_RES*)result->GetPointerFromInternalField(0);
        
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row == NULL) {
            return Null();
        }
        unsigned int num_fields = mysql_num_fields(res);
        unsigned long *lengths = mysql_fetch_lengths(res);
        MYSQL_FIELD *fields = mysql_fetch_fields(res);
        Local<Object> rowObj = Object::New();
        for(uint32_t i = 0; i < num_fields; i++) {
            rowObj->Set(String::New(fields[i].name, fields[i].name_length),
                        String::New(row[i], lengths[i]));
        }
        return scope.Close(rowObj);
	}

	V8_FUNCTN(ClientVersion) {
		return V8_STR(mysql_get_client_info());
	}

	extern "C" Handle<Object> Initialize() {
		HandleScope scope;
		Local<Object> exports = Object::New();
		SET_METHOD(exports, "version", ClientVersion)
        
        Local<FunctionTemplate> mysql_t = FunctionTemplate::New(MySQL);
        Local<ObjectTemplate> mysql_ot = mysql_t->PrototypeTemplate();
        SET_METHOD(mysql_ot, "realConnect", RealConnect);
        SET_METHOD(mysql_ot, "realQuery",   RealQuery);
		exports->Set(V8_STR("MySql"), mysql_t->GetFunction());
        
        mysqlResult_t =  Persistent<FunctionTemplate>::New(FunctionTemplate::New(Result));
        mysqlResult = Persistent<Function>::New(mysqlResult_t->GetFunction());
        Local<ObjectTemplate> result_ot = mysqlResult_t->InstanceTemplate();
        SET_METHOD(result_ot, "numRows",   NumRows);
        SET_METHOD(result_ot, "fetchRow",   FetchRow);
		
		return exports;
	}

	extern "C" void Teardown(Handle<Object> o) {
		//nothing really to do here
	}

}