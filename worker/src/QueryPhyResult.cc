/* 
 * LSST Data Management System
 * Copyright 2012 LSST Corporation.
 * 
 * This product includes software developed by the
 * LSST Project (http://www.lsst.org/).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the LSST License Statement and 
 * the GNU General Public License along with this program.  If not, 
 * see <http://www.lsstcorp.org/LegalNotices/>.
 */
#include "lsst/qserv/worker/QueryPhyResult.h"

#include <fcntl.h>
#include <iterator>

#include "lsst/qserv/SqlErrorObject.hh"
#include "lsst/qserv/worker/Config.h"
#include "lsst/qserv/worker/Base.h"
#include "lsst/qserv/worker/MonetConnection.hh"
 
namespace qWorker = lsst::qserv::worker;
using lsst::qserv::MonetConfig;

////////////////////////////////////////////////////////////////////////
void qWorker::QueryPhyResult::addResultTable(std::string const& t) {
    _resultTables.insert(t);
}

bool qWorker::QueryPhyResult::hasResultTable(std::string const& t) const {
    return _resultTables.end() != _resultTables.find(t);
}

void qWorker::QueryPhyResult::reset() {
    _resultTables.clear();
    _outDb.assign(std::string());
}

std::string qWorker::QueryPhyResult::getCommaResultTables()  {
    std::stringstream ss;
    std::string s;
    std::copy(_resultTables.begin(), _resultTables.end(), 
              std::ostream_iterator<std::string const&>(ss, ","));
    s = ss.str();
    s.erase(s.end()-1, s.end()); // drop final comma
    return s;
}

std::string qWorker::QueryPhyResult::_getSpaceResultTables() const {
    std::stringstream ss;
    std::copy(_resultTables.begin(), _resultTables.end(), 
              std::ostream_iterator<std::string const&>(ss, " "));
    return ss.str();
}


bool qWorker::QueryPhyResult::performMysqldump(qWorker::Logger& log,
                                               std::string const& user,
                                               std::string const& dumpFile,
                                               SqlErrorObject& errObj) {
    // Dump a database to a dumpfile.
    
    // Make sure the path exists
    _mkdirP(dumpFile);

    std::string cmd = getConfig().getString("mysqlDump") + 
        (Pformat(
            " --compact --add-locks --create-options --skip-lock-tables"
	    " --socket=%1%"
            " -u %2%"
            " --result-file=%3% %4% %5%")
         % getConfig().getString("mysqlSocket") 
         % user
         % dumpFile % _outDb 
         % _getSpaceResultTables()).str();
    log((Pformat("dump cmdline: %1%") % cmd).str().c_str());

    log((Pformat("TIMING,000000QueryDumpStart,%1%")
            % ::time(NULL)).str().c_str());
    int cmdResult = system(cmd.c_str());

    log((Pformat("TIMING,000000QueryDumpFinish,%1%")
            % ::time(NULL)).str().c_str());

    if (cmdResult != 0) {
        errObj.setErrNo(errno);
        return errObj.addErrMsg("Unable to dump database " + _outDb
                                + " to " + dumpFile);
    }
    return true;
}

bool qWorker::QueryPhyResult::performMonetDump(qWorker::Logger& log,
                                               MonetConfig const& mc,
                                               std::string const& dumpFile,
                                               SqlErrorObject& errObj) {
    // Dump a database to a dumpfile.
    
    // Make sure the path exists
    _mkdirP(dumpFile);

#if 0
    /*
      msqldump [ options ] [ dbname ]

      Msqldump is the program to dump an MonetDB/SQL database. The
      dump can be used to populate a new MonetDB/SQL database. 
      OPTIONS
        −−database=database (−d database)
        −−host=hostname (−h hostname) (default: localhost).
        −−port=portnr (−p portnr) (default: 50000).       
        −−user=user (−u user)
        −−describe (−D)  Only dump the database schema.
        −−inserts (−N)
        When dumping the table data, use INSERT INTO statements,
      rather than COPY INTO + CSV values. INSERT INTO statements are
      better portable, and necessary when the load of the dump is
      processed by e.g. a JDBC application. 
      −−quiet (−q) Don’t print the welcome message.
    */
    // Hardcode msqldump path.
#if 0
    std::string dumpProg = "/scratch/danielw/MonetDB-Apr2012-SP1/bin/msqldump";
    std::string cmd = dumpProg + 
        (Pformat( " −−database=%1%"
                  " −−host=%2%" 
                  " −−port=%3%"
                  " −−user=%4%"
                  " −−inserts")).str();
#endif
    // Actually, use dump wrapper to do type conversion.
    std::string myDump = "monetdump.py";
    std::stringstream cs;
    cs << myDump << " " 
       << mc.hostname << " "
       << mc.port << " "
       << mc.user << " " 
       << mc.password << " "
       << mc.db << " " // or _outDb, which might be "schema" in MonetDB
       << table << " "
       << dumpFile;
    log((Pformat("dump cmdline: %1%") % ss.str()).str().c_str());

    log((Pformat("TIMING,000000QueryDumpStart,%1%")
            % ::time(NULL)).str().c_str());
    int cmdResult = system(cmd.c_str());

    log((Pformat("TIMING,000000QueryDumpFinish,%1%")
            % ::time(NULL)).str().c_str());

    if (cmdResult != 0) {
        errObj.setErrNo(errno);
        return errObj.addErrMsg("Unable to dump database " + _outDb
                                + " to " + dumpFile);
    }
    */
#endif
    return true;
}


void qWorker::QueryPhyResult::_mkdirP(std::string const& filePath) {
    // Quick and dirty mkdir -p functionality.  No error checking.
    std::string::size_type pos = 0;
    struct stat statbuf;
    while ((pos = filePath.find('/', pos + 1)) != std::string::npos) {
        std::string dir(filePath, 0, pos);
        if (::stat(dir.c_str(), &statbuf) == -1) {
            if (errno == ENOENT) {
                mkdir(dir.c_str(), 0777);
            }
        }
    }
}


