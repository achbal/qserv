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
//#include "xrdsvc/SsiService.h"

// System headers
#include <iostream>
//#include <string>

// Third-party headers
//#include "XrdSsi/XrdSsiLogger.hh"

// Qserv headers
#include "xrdsvc/SsiService.h"

class XrdSsiLogger;
class XrdSsiCluster;

extern "C" {
XrdSsiService *XrdSsiGetServerService(XrdSsiLogger  *logP,
                                      XrdSsiCluster *clsP,
                                      const char    *cfgFn,
                                      const char    *parms)
{
    std::cerr << " Returning new Service " << std::endl;
    return new lsst::qserv::xrdsvc::SsiService(logP);
}
} // extern "C"
