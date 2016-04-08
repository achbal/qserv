// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2008-2015 LSST Corporation.
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

#ifndef LSST_QSERV_WCONFIG_CONFIG_H
#define LSST_QSERV_WCONFIG_CONFIG_H

// System headers
#include <map>
#include <memory>
#include <string>


#include "boost/property_tree/ini_parser.hpp"
#include "boost/property_tree/ptree.hpp"

// Qserv headers
#include "global/stringTypes.h"

namespace lsst {
namespace qserv {

namespace mysql {
    // Forward
    class MySqlConfig;
}

namespace wconfig {

/// The Config object provides a thin abstraction layer to shield code from
/// the details of how the qserv worker is configured.  It currently
/// reads configuration from environment variables, but could later use
/// its own configuration file.
class Config {
public:
    Config();
    int getInt(std::string const& key, int defVal=-1) const;
    std::string const& getString(std::string const& key) const;
    bool getIsValid() const { return _isValid; }
    std::string const& getError() const { return _error; }
    mysql::MySqlConfig const& getSqlConfig() const;

private:
    char const* _getEnvDefault(char const* varName, char const* defVal);
    void _load();
    void _validate();

    StringMap _map;
    bool _isValid {false};
    std::string _error;
    std::shared_ptr<mysql::MySqlConfig> _sqlConfig;
};

Config& getConfig();

}}} // namespace qserv::core::wconfig

#endif // LSST_QSERV_WCONFIG_CONFIG_H
