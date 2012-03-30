
#include "lsst/qserv/worker/Config.h"
#include "lsst/qserv/SqlConnection.hh"
#include "lsst/qserv/worker/Metadata.h"
#include "lsst/qserv/worker/QservPathStructure.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

using lsst::qserv::SqlConfig;
using std::cout;
using std::cerr;
using std::endl;
using std::ifstream;
using std::string;
using std::stringstream;
using std::vector;

const char* execName = "qsDbTool";

int
printHelp() {
    cout
     << "\nNAME:\n"
     << "  " << execName << "\n\n"
     << "DESCRIPTION:\n"
     << "  Manages qserv metadata\n\n"
     << "EXAMPLES:\n"
     << "  " << execName << " -r <mysqlAuth> <uniqueId> <dbName> [<table1>] "
                         << "[<table2>] ...\n"
     << "  " << execName << " -u <mysqlAuth> <uniqueId> <dbName>\n"
     << "  " << execName << " -s <mysqlAuth> <uniqueId>\n"
     << "  " << execName << " -e <mysqlAuth> <uniqueId> <baseDir> [<dbName>] "
                         << "[<dbName2>] ...\n"
     << "  " << execName << " -h\n\n"
     << "ARGUMENTS:\n"
     << "  -r, --register\n"
     << "         registers database in qserv metadata\n\n"
     << "  -u, --unregister\n"
     << "         unregisters database from qserv metadata\n\n"
     << "  -s, --show\n"
     << "         prints qserv metadata\n\n"
     << "  -e, --export\n"
     << "         generates export paths. If no dbName is given, it will\n"
     << "         run for all databases registered in qserv metadata.\n"
     << "\nABOUT <uniqueId>:\n"
     << "  The uniqueId was introduced to allow running multiple masters\n"
     << "  and/or workers on the same machine. It uniquely identifies\n"
     << "  a master / a worker.\n"
     << "\nABOUT <mysqlAuth>:\n"
     << "  <mysqlAuth> should point to a config file. Format of one line \n"
     << "  of config file: <token>:<value>. (Parsing is very basic,\n"
     << "  so no extra spaces please.) Supported tokens: \n"
     << "  host, port, user, pass, sock. Example contents:\n"
     << "      host:localhost\n"
     << "      port:3306\n"
     << "      user:theMySqlUser\n"
     << "      pass:thePassword\n"
     << "      sock:/the/mysql/socket/file.sock\n"
     << endl;
    return 0;
}

int
printHelpMsg(string const& msg) {
    cout << execName << ": " << msg << "\n" 
         << "Try `" << execName << " -h` for more information.\n";
    return 0;
}

SqlConfig
assembleSqlConfig(string const& authFile) {

    SqlConfig sc;
    //sc.socket = qWorker::getConfig().getString("mysqlSocket").c_str();

    ifstream f;
    f.open(authFile.c_str());
    if (!f) {
        cerr << "Failed to open '" << authFile << "'" << endl;
        assert(f);
    }
    string line;
    f >> line;
    while ( !f.eof() ) {
        int pos = line.find_first_of(':');
        if ( pos == -1 ) {
            cerr << "Invalid format, expecting <token>:<value>. "
                 << "File '" << authFile 
                 << "', line: '" << line << "'" << endl;
            assert(pos != -1);
        }
        string token = line.substr(0,pos);
        string value = line.substr(pos+1, line.size());
        if (token == "host") { 
            sc.hostname = value;
        } else if (token == "port") {
            sc.port = atoi(value.c_str());
            if ( sc.port <= 0 ) {
                cerr << "Invalid port number " << sc.port << ". "
                 << "File '" << authFile 
                 << "', line: '" << line << "'" << endl;
                assert (sc.port != 0);
            }        
        } else if (token == "user") {
            sc.username = value;
        } else if (token == "pass") {
            sc.password = value;
        } else if (token == "sock") {
            sc.socket = value;
        } else {
            cerr << "Unexpected token: '" << token << "'" 
                 << " (supported tokens are: host, port, user, pass, sock)" 
                 << endl;
            assert(0);
        }
        f >> line;
   }
   f.close();
   return sc;
}

bool
registerDb(SqlConfig& sc,
           string const& uniqueId,
           string const& dbName,
           string const& pTables) {
    lsst::qserv::SqlConnection sqlConn(sc);
    lsst::qserv::SqlErrorObject errObj;
    lsst::qserv::worker::Metadata m(uniqueId);
    if ( !m.registerQservedDb(dbName, pTables, sqlConn, errObj) ) {
        cerr << "Failed to register db. " << errObj.printErrMsg() << endl;
        return errObj.errNo();
    }
    cout << "Database " << dbName << " successfully registered." << endl;
    return 0;
}

bool
unregisterDb(SqlConfig& sc,
           string const& uniqueId,
           string const& dbName) {
    lsst::qserv::SqlConnection sqlConn(sc);
    lsst::qserv::SqlErrorObject errObj;
    lsst::qserv::worker::Metadata m(uniqueId);
    if ( !m.unregisterQservedDb(dbName, sqlConn, errObj) ) {
        cerr << "Failed to unregister db. " 
             << errObj.printErrMsg() << endl;
        return errObj.errNo();
    }
    cout << "Database " << dbName << " successfully unregistered." << endl;
    return 0;
}

bool
showMetadata(SqlConfig& sc,
             string const& uniqueId) {
    lsst::qserv::SqlConnection sqlConn(sc);
    lsst::qserv::SqlErrorObject errObj;
    lsst::qserv::worker::Metadata m(uniqueId);
    if ( !m.showMetadata(sqlConn, errObj) ) {
        cerr << "Failed to print metadata. " << errObj.printErrMsg() << endl;
        return errObj.errNo();
    }
    return 0;
}

bool
generateExportPathsForDb(SqlConfig& sc,
                         string const& uniqueId,
                         string const& dbName, 
                         string const& baseDir) {
    lsst::qserv::SqlConnection sqlConn(sc);
    lsst::qserv::SqlErrorObject errObj;

    lsst::qserv::worker::Metadata m(uniqueId);
    
    vector<string> exportPaths;
    if ( !m.generateExportPathsForDb(baseDir, dbName,
                                     sqlConn, errObj, exportPaths) ) {
        cerr << "Failed to generate export directories. " 
             << errObj.printErrMsg() << endl;
        return errObj.errNo();
    }
    lsst::qserv::worker::QservPathStructure p;
    if ( !p.insert(exportPaths) ) {
        cerr << "Failed to insert export paths. "
             << errObj.printErrMsg() << endl;
        return errObj.errNo();
    }
    if ( !p.persist() ) {
        cerr << "Failed to persist export paths. " 
             << errObj.printErrMsg() << endl;
        return errObj.errNo();
    }
    cout << "Export paths successfully created for db " 
         << dbName << "." << endl;
    return 0;
}

bool
generateExportPaths(SqlConfig& sc,
                    string const& uniqueId,
                    string const& baseDir) {
    lsst::qserv::SqlConnection sqlConn(sc);
    lsst::qserv::SqlErrorObject errObj;

    lsst::qserv::worker::Metadata m(uniqueId);
    
    vector<string> exportPaths;
    if ( ! m.generateExportPaths(baseDir, sqlConn, errObj, exportPaths) ) {
        cerr << "Failed to generate export directories. " 
             << errObj.printErrMsg() << endl;
        return errObj.errNo();
    }
    lsst::qserv::worker::QservPathStructure p;
    if ( !p.insert(exportPaths) ) {
        cerr << "Failed to insert export paths. "
             << errObj.printErrMsg() << endl;
        return errObj.errNo();
    }
    if ( !p.persist() ) {
        cerr << "Failed to persist export paths. " 
             << errObj.printErrMsg() << endl;
        return errObj.errNo();
    }
    cout << "Export paths successfully created for all " 
         << "databases registered in qserv metadata." << endl;
    return 0;
}


int 
main(int argc, char* argv[]) {
    const char* execName = "qsDbTool";//argv[0];
    if ( argc < 2 ) {
        return printHelp();
    }
    string firstArg = argv[1];
    if ( firstArg == "-h" ) {
        return printHelp();
    } else if (firstArg != "-r" && firstArg != "--register" &&
               firstArg != "-u" && firstArg != "--unregister" &&
               firstArg != "-s" && firstArg != "--show" &&
               firstArg != "-e" && firstArg != "--export") {
        stringstream s;
        s << "unrecognized option '" << firstArg << "'";
        return printHelpMsg(s.str());
    } else if ( argc < 4 ) {
        return printHelpMsg("insufficient number of arguments.");
    }
    SqlConfig sc = assembleSqlConfig(argv[2]);
    string uniqueId = argv[3];

    if ( firstArg == "-r" || firstArg == "--register" ) {
        if ( argc < 5 ) {
            return printHelpMsg("insufficient number of arguments.");
        }
        string pTables;
        if ( argc > 5 ) {
            for (int i=5 ; i<argc ; i++) {
                pTables += argv[i];
                if (i != argc-1) {
                    pTables += ',';
                }
            }
        }
        return registerDb(sc, uniqueId, argv[4]/*dbName*/, pTables);        
    } else if ( firstArg == "-u" || firstArg == "--unregister" ) {
        if ( argc != 5 ) {
            return printHelpMsg("insufficient number of arguments.");
        }
        return unregisterDb(sc, uniqueId, argv[4]/*dbName*/);
    } else if ( firstArg == "-s" || firstArg == "--show" ) {
        if ( argc != 4 ) {
            return printHelpMsg("insufficient number of arguments.");
        }
        return showMetadata(sc, uniqueId);
    } else if ( firstArg == "-e" || firstArg == "--export" ) {
        if ( argc < 5 ) {
            return printHelpMsg("insufficient number of arguments.");
        } else if ( argc == 5 ) {
            return generateExportPaths(sc, uniqueId, argv[4]/*baseDir*/);
        } else {
            for (int i=5 ; i<argc ; i++) {
                generateExportPathsForDb(sc, uniqueId, 
                                         argv[5]/*dbName*/, argv[4]/*baseDir*/);
            }
            return 0;
        }
    } else {
        stringstream s;
        s << "unrecognized option '" << firstArg << "'";
        return printHelpMsg(s.str());
    }
    printHelp();
    return 0;
}
