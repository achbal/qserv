// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2014-2016 AURA/LSST.
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
/**
 * @file
 *
 * @ingroup util
 *
 * @brief Manage Qserv configuration
 *
 * Allow to read czar and xrdssi plugin configuration
 *
 * @author Fabrice Jammes, IN2P3/SLAC
 */

#ifndef LSST_QSERV_UTIL_CONFIG_H_
#define LSST_QSERV_UTIL_CONFIG_H_

// System headers
#include <string>

// Qserv headers
#include "global/stringTypes.h"

namespace lsst {
namespace qserv {
namespace util {

/** @brief Manage Qserv configuration
 *
 */
class Config {
public:

    /** Build an object from a configuration file
     *
     * @param configFilePath: path to Qserv configuration file
     */
    Config(std::string const& configFilePath);

    /** Overload output operator for current class
     *
     * @param out
     * @param config
     * @return an output stream
     */
    friend std::ostream& operator<<(std::ostream &out, Config const& config);

    /** Get value for a configuration key
     * @param key configuration key
     * @params defaultValue
     * @return the string value for a key, defaulting to defaultValue
     */

    std::string get(std::string const& key, std::string const& defaultValue);

    const lsst::qserv::StringMap& getConfigMap() const {
        return _configMap;
    }

private:

    StringMap _configMap;

};

}}} // namespace lsst::qserv::util

#endif /* LSST_QSERV_UTIL_CONFIG_H_ */
