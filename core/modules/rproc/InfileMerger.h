// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2014 LSST Corporation.
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
#ifndef LSST_QSERV_RPROC_INFILEMERGER_H
#define LSST_QSERV_RPROC_INFILEMERGER_H
/// InfileMerger.h declares:
///
/// struct InfileMergerError
/// class InfileMergerConfig
/// class InfileMerger
/// (see individual class documentation for more information)

// System headers
#include <string>

// Third-party headers
#include <boost/thread.hpp> // for mutex.
#include <boost/shared_ptr.hpp>

// Local headers
#include "rproc/mergeTypes.h"

// Forward declarations
namespace lsst {
namespace qserv {
namespace mysql {
    class MySqlConfig;
}
namespace proto {
    class ProtoHeader;
    class Result;
}
namespace qdisp {
    class MessageStore;
}
namespace query {
    class SelectStmt;
}
namespace rproc {
    class SqlInsertIter;
}
namespace sql {
    class SqlConnection;
}
namespace util {
    class PacketBuffer;
}
namespace xrdc {
    class PacketIter;
}}} // End of forward declarations


namespace lsst {
namespace qserv {
namespace rproc {

/// struct InfileMergerError - value class for InfileMerger error code.
struct InfileMergerError {
public:
    enum {NONE=0, HEADER_IMPORT, HEADER_OVERFLOW,
          RESULT_IMPORT, RESULT_MD5, MYSQLOPEN, MERGEWRITE, TERMINATE,
          CREATE_TABLE,
          MYSQLCONNECT, MYSQLEXEC} status;
    InfileMergerError() {}
    InfileMergerError(int code) : errorCode(code) {}
    InfileMergerError(int code, char const* desc)
        : errorCode(code), description(desc) {}
    int errorCode;
    std::string description;
    bool resultTooBig() const;
};

/// class InfileMergerConfig - value class for configuring a InfileMerger
class InfileMergerConfig {
public:
    InfileMergerConfig() {}
    InfileMergerConfig(boost::shared_ptr<qdisp::MessageStore> messageStore_,
                       std::string const& targetDb_,
                       std::string const& targetTable_,
                       boost::shared_ptr<query::SelectStmt> mergeStmt_,
                       std::string const& user_, std::string const& socket_)
        :  messageStore(messageStore_),
           targetDb(targetDb_),  targetTable(targetTable_),
           mergeStmt(mergeStmt_), user(user_), socket(socket_)
    {
    }

    boost::shared_ptr<qdisp::MessageStore> messageStore;
    std::string targetDb; // for final result, and imported result
    std::string targetTable;
    boost::shared_ptr<query::SelectStmt> mergeStmt;
    std::string user;
    std::string socket;
};

/// InfileMerger is a row-based merger that imports rows from a result messages
/// and inserts them into a MySQL table, as specified during construction by
/// InfileMergerConfig.
/// To use, construct a configured instance, then call merge() to kick off the
/// merging process, and finalize() waits for outstanding merging processes and
/// performs the appropriate post-processing before returning.
/// merge() right now expects an entire message buffer, where a message buffer
/// consists of:
/// Byte 0: unsigned char size of ProtoHeader message
/// Bytes 1 - size_ph : ProtoHeader message (containing size of result message)
/// Bytes size_ph - size_ph + size_rm : Result message
/// At present, Result messages are not chained.
class InfileMerger {
public:
    typedef boost::shared_ptr<util::PacketBuffer> PacketBufferPtr;
    explicit InfileMerger(InfileMergerConfig const& c);
    ~InfileMerger();

    /// Merge a message buffer, which contains:
    /// Size of ProtoHeader message
    /// ProtoHeader message
    /// Result message
    /// @return count of bytes imported.
    off_t merge(char const* dumpBuffer, int dumpLength);

    /// @return error details if finalize() returns false
    InfileMergerError const& getError() const { return _error; }
    /// @return final target table name  storing results after post processing
    std::string getTargetTable() const {return _config.targetTable; }
    /// Finalize a "merge" and perform postprocessing
    bool finalize();
    /// Check if the object has completed all processing.
    bool isFinished() const;

private:
    struct Msgs;
    int _readHeader(proto::ProtoHeader& header, char const* buffer, int length);
    int _readResult(proto::Result& result, char const* buffer, int length);
    bool _verifySession(int sessionId);
    bool _verifyMd5(std::string const& expected, std::string const& actual);
    int _importBuffer(char const* buffer, int length, bool setupTable);
    bool _setupTable(Msgs const& msgs);
    void _setupRow();
    bool _applySql(std::string const& sql);
    bool _applySqlLocal(std::string const& sql);
    void _fixupTargetName();

    InfileMergerConfig _config; //< Configuration
    boost::shared_ptr<mysql::MySqlConfig> _sqlConfig; //< SQL connection config
    boost::shared_ptr<sql::SqlConnection> _sqlConn; //< SQL connection

    std::string _mergeTable; //< Table for result loading
    InfileMergerError _error; //< Error state

    bool _isFinished; //< Completed?
    boost::mutex _createTableMutex; //< protection from creating tables
    boost::mutex _sqlMutex; //< Protection for SQL connection

    class Mgr;
    std::auto_ptr<Mgr> _mgr; //< Delegate merging action object

    bool _needCreateTable; //< Does the target table need creating?
};

}}} // namespace lsst::qserv::rproc

// Local Variables:
// mode:c++
// comment-column:0
// End:

#endif // LSST_QSERV_RPROC_INFILEMERGER_H
