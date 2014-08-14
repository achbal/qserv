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
#include "ccontrol/UserQueryFactory.h"

// System headers
#include <stdlib.h>

// Qserv headers
#include "ccontrol/ConfigMap.h"
#include "ccontrol/UserQuery.h"
#include "ccontrol/userQueryProxy.h"
#include "css/Facade.h"
#include "qdisp/Executive.h"
#include "qproc/QuerySession.h"
#include "rproc/TableMerger.h"

namespace lsst {
namespace qserv {
namespace ccontrol {

class UserQueryFactory::Impl {
public:
    void readConfig(StringMap const& m);
    void initFacade(std::string const& cssTech, std::string const& cssConn);
    void initMergerTemplate();

    qdisp::Executive::Config::Ptr executiveConfig;
    boost::shared_ptr<css::Facade> facade;
    rproc::TableMergerConfig mergerConfigTemplate;
    std::string defaultDb;
};

////////////////////////////////////////////////////////////////////////
UserQueryFactory::UserQueryFactory(StringMap const& m) {
    ::putenv((char*)"XRDDEBUG=1");
    _impl.reset(new Impl);
    assert(_impl);
    _impl->readConfig(m);
}

int
UserQueryFactory::newUserQuery(std::string const& query,
                               std::string const& resultTable) {
    qproc::QuerySession::Ptr qs(new qproc::QuerySession(_impl->facade));
    qs->setResultTable(resultTable);
    qs->setDefaultDb(_impl->defaultDb);
    qs->setQuery(query);

    UserQuery* uq = new UserQuery(qs);
    int sessionId = UserQuery_takeOwnership(uq);
    uq->_sessionId = sessionId;
    uq->_executive.reset(new qdisp::Executive(
                             _impl->executiveConfig,
                             uq->_messageStore));
    rproc::TableMergerConfig* mct
        = new rproc::TableMergerConfig(_impl->mergerConfigTemplate);
    mct->targetTable = resultTable;
    uq->_mergerConfig.reset(mct);
    return sessionId;
}

void UserQueryFactory::Impl::readConfig(StringMap const& m) {
    ConfigMap cm(m);
    /// localhost:1094 is the most reasonable default, even though it is
    /// the wrong choice for all but small developer installations.
    std::string serviceUrl = cm.get(
        "frontend.xrootd", // czar.serviceUrl
        "WARNING! No xrootd spec. Using localhost:1094",
        "localhost:1094");
    executiveConfig.reset(new qdisp::Executive::Config(serviceUrl));
#if 0
    _scratchPath =  cm.get(
        "frontend.scratch_path",
        "Error, no scratch path found. Using /tmp.",
        "/tmp");
#endif
    // This should be overriden by the installer properly.
    mergerConfigTemplate.socket =  cm.get(
        "resultdb.unix_socket",
        "Error, resultdb.unix_socket not found. Using /u1/local/mysql.sock.",
        "/u1/local/mysql.sock");
    mergerConfigTemplate.user =  cm.get(
        "resultdb.user",
        "Error, resultdb.user not found. Using qsmaster.",
        "qsmaster");
    mergerConfigTemplate.targetDb =  cm.get(
        "resultdb.db",
        "Error, resultdb.db not found. Using qservResult.",
        "qservResult");
    std::string cssTech = cm.get(
        "css.technology",
        "Error, css.technology not found.",
        "invalid");
    std::string cssConn = cm.get(
        "css.connection",
        "Error, css.connection not found.",
        "");
    initFacade(cssTech, cssConn);
    defaultDb = cm.get(
        "table.defaultdb",
        "Empty table.defaultdb. Using LSST",
        "LSST");
#if 0 // TODO: Revisit during new result protocol/pipeline
    merger::TableMergerConfig cfg(_resultDbDb,     // cfg result db
                                  resultTable,     // cfg resultname
                                  m,               // merge fixup obj
                                  _resultDbUser,   // result db credentials
                                  _resultDbSocket, // result db credentials
                                  mysqlBin,        // Obsolete
                                  dropMem          // cfg
                                  );

#endif

}

void UserQueryFactory::Impl::initFacade(std::string const& cssTech,
                                        std::string const& cssConn) {
    if (cssTech == "zoo") {
        LOGGER_INF << "Initializing zookeeper-based css, with "
                   << cssConn << std::endl;
        facade = css::FacadeFactory::createZooFacade(cssConn);
//        _qSession.reset(new qproc::QuerySession(cssFPtr));
    } else if (cssTech == "mem") {
        LOGGER_INF << "Initializing memory-based css, with "
                   << cssConn << std::endl;
        facade = css::FacadeFactory::createMemFacade(cssConn);
//        _qSession.reset(new qproc::QuerySession(cssFPtr));
    } else {
        LOGGER_ERR << "Unable to determine css technology, check config file."
                   << std::endl;
//        throw ConfigError("Invalid css technology, check config file.");
// FIXME
    }
}

}}} // lsst::qserv::ccontrol
