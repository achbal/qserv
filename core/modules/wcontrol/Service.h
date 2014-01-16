/* 
 * LSST Data Management System
 * Copyright 2012-2013 LSST Corporation.
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
#ifndef LSST_QSERV_WORKER_SERVICE_H
#define LSST_QSERV_WORKER_SERVICE_H
#include "wbase/Base.h"
namespace lsst {
namespace qserv {
namespace worker {

class Foreman; // Forward
class Logger;

class Service {
public:
    typedef boost::shared_ptr<Service> Ptr;

    explicit Service(boost::shared_ptr<Logger> log=boost::shared_ptr<Logger>());
    TaskAcceptor::Ptr getAcceptor();
    void squashByHash(std::string const& hash);

private:
    boost::shared_ptr<Foreman> _foreman;
};

}}} // lsst::qserv:worker
#endif // LSST_QSERV_WORKER_SERVICE_H

