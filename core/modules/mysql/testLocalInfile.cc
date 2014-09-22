#include <cassert>
#include <iostream>
#include <iterator>

#include "mysql/LocalInfile.h"
#include "mysql/SchemaFactory.h"
#include "sql/Schema.h"
typedef unsigned long long uint64_t;

using lsst::qserv::sql::ColSchema;
using lsst::qserv::sql::Schema;
using lsst::qserv::sql::ColumnsIter;
using lsst::qserv::mysql::LocalInfile;
using lsst::qserv::mysql::SchemaFactory;

/// Test code for exercising LocalInfile by implementing CREATE TABLE
/// xxxx SELECT * FROM yyyyy . Depends on having the right chunk
/// table(s) in a local mysqld, and so does not contain enough
/// information to be run as a unit test. This code is mostly useful
/// for developing or exploring the mysql_set_local_infile_handler
/// interface.

class Api {
public:
    Api() {
        mysql_init(&cursor);
    }
    ~Api() {
        mysql_close(&cursor);
    }

    void connect() {
        MYSQL* cur = mysql_real_connect(&cursor,
                                        "localhost", // host
                                        "danielw", // user
                                        "", // pw
                                        "", // db
                                        0, // port
                                        "/u1/local/mysql.sock", // socket
                                        0); // client flag
        if(!cur) {
            std::cout << "Failed to connect to MySQL: Error: "
                      << mysql_error(cur) << std::endl;
            assert(cur);
        }
    }
    MYSQL* getMysql() { return &cursor; }

    bool _sendQuery(std::string const& query) {
//   mysql_real_query(MYSQL*, char* query, int qlen) -> 0 success, nonzero error via mysql_error(mysql*)
        int result = mysql_real_query(&cursor, query.c_str(), query.size());
        if(result != 0) {
            std::cout << "error executing " << query << "  " << mysql_error(&cursor)
                 << std::endl;
            return false;
        } else {
            return true;
        }
    }

    bool exec(std::string query) {
        bool success = _sendQuery(query);
        if(success) {
            getResultUnbuf();
        }
        return success;
    }

    MYSQL_RES* execStart(std::string query) {
        bool success = _sendQuery(query);
        if(!success) {
            return 0;
        }
        MYSQL_RES* result = mysql_use_result(&cursor);
        return result;
    }
    bool createTable(std::string table, Schema const& s) {
        std::string formedCreate = formCreateStatement(table, s);
        std::cout << "Formed create: " << formedCreate << "\n";
        //return false;
        return exec(formedCreate);
    }
    Schema getSchema(MYSQL_RES* result) {
        return SchemaFactory::newFromResult(result);
    }
    std::string formCreateStatement(std::string const& table, Schema const& s) {
        std::ostringstream os;
        os << "CREATE TABLE " << table << " (";
        ColumnsIter b, i, e;
        for(i=b=s.columns.begin(), e=s.columns.end(); i != e; ++i) {
            if(i != b) {
                os << ",\n";
            }
            os << *i;
        }
        os << ")";
        return os.str();
    }
    std::string formInfileStatement(std::string const& table,
                                    std::string const& virtFile) {
        std::ostringstream os;
        os << "LOAD DATA LOCAL INFILE '" << virtFile << "' INTO TABLE "
           << table;
        return os.str();
    }

    bool loadDataInfile(std::string const& table, std::string const& virtFile) {
        std::string infileStatement = formInfileStatement(table, virtFile);
        std::cout << "Formed infile: " << infileStatement << "\n";
        //return false;
        return exec(infileStatement);
    }

    void getResult() {
        MYSQL_ROW row;

        MYSQL_RES* result = mysql_store_result(&cursor);
        // call after mysql_store_result
        uint64_t rowcount = mysql_affected_rows(&cursor);
        std::cout << rowcount
             << " records found.\n";

        if(result) { // rows?

            int num_fields = mysql_num_fields(result);
            std::cout << num_fields << " fields per row\n";
            while ((row = mysql_fetch_row(result))) {
                std::cout << "row: ";
                std::copy(row, row+num_fields,
                          std::ostream_iterator<char*>(std::cout, ","));
                std::cout << "\n";

            }
            mysql_free_result(result);
        } else  { // mysql_store_result() returned nothing
            if(mysql_field_count(&cursor) > 0) {
                // mysql_store_result() should have returned data
                std::cout <<  "Error getting records: "
                     << mysql_error(&cursor) << std::endl;
            }
        }
    }

    void getResultUnbuf() {
        MYSQL_ROW row;

        MYSQL_RES* result = mysql_use_result(&cursor);
        // call after mysql_store_result
        //uint64_t rowcount = mysql_affected_rows(&cursor);
        if(result) { // rows?
            Schema s = SchemaFactory::newFromResult(result);
            std::cout << "Schema is "
                      << formCreateStatement("hello", s) << "\n";

            std::cout << "will stream results.\n";
            int num_fields = mysql_num_fields(result);
            std::cout << num_fields << " fields per row\n";
            // createTable(s);
            while ((row = mysql_fetch_row(result))) {
                std::cout << "row: ";
                std::copy(row, row+num_fields,
                          std::ostream_iterator<char*>(std::cout, ","));
                std::cout << "\n";
                // Each element needs to be mysql-sanitized

            }
            mysql_free_result(result);
        } else  { // mysql_store_result() returned nothing
            if(mysql_field_count(&cursor) > 0) {
                // mysql_store_result() should have returned data
                std::cout <<  "Error getting records: "
                     << mysql_error(&cursor) << std::endl;
            }
        }
    }
    MYSQL cursor;
};

void play() {
    Api a;
    a.connect();
    //    a.exec("select count(*) from LSST.Object_3240");
    //    a.exec("select * from LSST.Object_3240 limit 1");
    a.exec("select * from test.deleteme limit 1");
}

void playDouble() {
    Api aSrc; // Source: will execute "select ..."
    aSrc.connect();
    Api aDest; // Dest: will execute "create table..." and "load data infile..."
    aDest.connect();
    MYSQL_RES* res = aSrc.execStart("SELECT * FROM LSST.Object_3240");
    LocalInfile::Mgr mgr;
    mgr.attach(aDest.getMysql());
    std::string virtFile = mgr.prepareSrc(res);
    std::string destTable = "qservResult." + virtFile;
    aDest.createTable(destTable, aSrc.getSchema(res));
    aDest.loadDataInfile(destTable, virtFile);
}
void playRead() {
    Api aSrc; // Source: will execute "select ..."
    aSrc.connect();
    Api aDest; // Dest: will execute "create table..." and "load data infile..."
    aDest.connect();
    MYSQL_RES* res = aSrc.execStart("SELECT * FROM LSST.Object_3240");
    LocalInfile::Mgr mgr;
    mgr.attach(aDest.getMysql());
    std::string virtFile = mgr.prepareSrc(res);
    void* infileptr;
    std::cout << "Init returned "
              << LocalInfile::Mgr::local_infile_init(&infileptr, virtFile.c_str(), &mgr)
              << std::endl;
    const int bufLen = 8192;
    char buf[bufLen];
    std::cout << "Read returned "
              << LocalInfile::Mgr::local_infile_read(infileptr, buf, bufLen)
              << std::endl;
}

void checkDoubleTable() {
    Api a; // Source: will execute "select ..."
    a.connect();

    if(!a.exec("create table test.twofloats (one float, two float);")) {
        std::cout << "error creating test.twofloats.\n";
        return;
    }

    MYSQL_RES* res = a.execStart("SELECT * FROM test.twofloats;");
    Schema schema = SchemaFactory::newFromResult(res);
    std::cout << "Two floats schema: ";
    std::copy(schema.columns.begin(), schema.columns.end(),
              std::ostream_iterator<ColSchema>(std::cout, ","));
    std::cout << std::endl;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        // Just drain the result
    }
    mysql_free_result(res);
    a.exec("drop table test.twofloats;");
}

int main(int,char**) {
    int blah = 3;
    switch(blah) {
    case 1:
        play();
        break;
    case 2:
        playDouble();
        break;
    case 3:
        checkDoubleTable();
        break;
    default:
        playRead();
        break;
    }
    std::cout << "done\n";
    return 0;
}
