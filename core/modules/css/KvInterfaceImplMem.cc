// -*- LSST-C++ -*-

/*
 * LSST Data Management System
 * Copyright 2014-2015 AURA/LSST.
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
  * @brief Interface to the Common State System - zookeeper-based implementation.
  *
  * @Author Jacek Becla, SLAC
  */

/*
 * Based on:
 * http://zookeeper.apache.org/doc/r3.3.4/zookeeperProgrammers.html#ZooKeeper+C+client+API
 *
 * To do:
 *  - perhaps switch to async (seems to be recommended by zookeeper)
 */

// Class header
#include "css/KvInterfaceImplMem.h"

// System headers
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string.h> // for memset

// Third-party headers
#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/predicate.hpp"
#include "boost/make_shared.hpp"

// LSST headers
#include "lsst/log/Log.h"

// Qserv headers
#include "css/CssError.h"

using std::endl;
using std::map;
using std::ostringstream;
using std::string;
using std::vector;

namespace lsst {
namespace qserv {
namespace css {

/**
  * Initialize the interface.
  *
  * @param mapPath path to the map dumped using ./admin/bin/qserv-admin.py
  *
  * To generate the key/value map, follow this recipe:
  * 1) cleanup everything in zookeeper. careful, this will wipe out
  *    everyting in zookeeper!
  *    echo "drop everything;" | ./admin/bin/qserv-admin.py
  * 2) generate the clean set:
  *    ./admin/bin/qserv-admin.py <  <commands>
  *    (example commands can be found in admin/examples/testMap_generateMap)
  * 3) then copy the generate file to final destination:
  *    mv /tmp/testMap.kvmap <destination>
  */
KvInterfaceImplMem::KvInterfaceImplMem(std::istream& mapStream) {
    _init(mapStream);
}

KvInterfaceImplMem::KvInterfaceImplMem(std::string const& filename) {
    std::ifstream f(filename.c_str());
    _init(f);
}

KvInterfaceImplMem::KvInterfaceImplMem(std::map<std::string, std::string> &&keysAndVals)
    : _kvMap(keysAndVals)
{
}

KvInterfaceImplMem::~KvInterfaceImplMem() {
}

void
KvInterfaceImplMem::create(string const& key, string const& value) {
    LOGF_INFO("*** KvInterfaceImplMem::create(%1%, %2%)" % key % value);
    if (exists(key)) {
        throw KeyExistsError(key);
    }
    // create all parents
    string parent = key;
    for (string::size_type p = parent.rfind('/'); p != string::npos; p = parent.rfind('/')) {
        parent = parent.substr(0, p);
        if (parent.empty()) break;
        if (_kvMap.find(parent) != _kvMap.end()) break;
        _kvMap.insert(std::make_pair(parent, std::string()));
    }
    // store the key with value
    _kvMap[key] = value;
}

void
KvInterfaceImplMem::set(string const& key, string const& value) {
    // Should always succeed, as long as std::map works.
    LOGF_INFO("*** KvInterfaceImplMem::set(%1%, %2%)" % key % value);
    _kvMap[key] = value;
}

bool
KvInterfaceImplMem::exists(string const& key) {
    bool ret = _kvMap.find(key) != _kvMap.end();
    LOGF_INFO("*** KvInterfaceImplMem::exists(%1%): %2%"
              % key % (ret?"YES":"NO"));
    return ret;
}

string
KvInterfaceImplMem::_get(string const& key,
                         string const& defaultValue,
                         bool throwIfKeyNotFound) {
    LOGF_INFO("*** KvInterfaceImplMem::get(%1%)" % key);
    if ( !exists(key) ) {
        if (throwIfKeyNotFound) {
            throw NoSuchKey(key);
        }
        return defaultValue;
    }
    string s = _kvMap[key];
    LOGF_INFO("got: '%1%'" % s);
    return s;
}

vector<string>
KvInterfaceImplMem::getChildren(string const& key) {
    LOGF_INFO("*** KvInterfaceImplMem::getChildren(), key: %1%" % key);
    if ( ! exists(key) ) {
        throw NoSuchKey(key);
    }
    const std::string pfx(key == "/" ? key : key + "/");
    vector<string> retV;
    map<string, string>::const_iterator itrM;
    for (itrM=_kvMap.begin() ; itrM!=_kvMap.end() ; itrM++) {
        string fullKey = itrM->first;
        LOGF_INFO("fullKey: %1%" % fullKey);
        if (boost::starts_with(fullKey, pfx)) {
            string theChild = fullKey.substr(pfx.length());
            if (!theChild.empty() && (theChild.find("/") == string::npos)) {
                LOGF_INFO("child: %1%" % theChild);
                retV.push_back(theChild);
            }
        }
    }
    if (LOG_CHECK_INFO()) {
        vector<string>::const_iterator itrV;
        std::stringstream ss;
        for (itrV=retV.begin(); itrV!=retV.end() ; itrV++) {
            ss << *itrV;
        }
        LOGF_INFO("got: %1% children: %2%" % retV.size() % ss.str());
    }
    return retV;
}

void
KvInterfaceImplMem::deleteKey(string const& key) {
    LOGF_INFO("*** KvInterfaceImplMem::deleteKey(%1%)." % key);
    if ( ! exists(key) ) {
        throw NoSuchKey(key);
    }
    _kvMap.erase(key);
}

void KvInterfaceImplMem::_init(std::istream& mapStream) {
    if(mapStream.fail()) {
        throw ConnError();
    }
    string line;
    vector<string> strs;
    while ( std::getline(mapStream, line) ) {
        boost::split(strs, line, boost::is_any_of("\t"));
        string theKey = strs[0];
        string theVal = strs[1];
        if (theVal == "\\N") {
            theVal = "";
        }
        _kvMap[theKey] = theVal;
    }
    //map<string, string>::const_iterator itrM;
    //for (itrM=_kvMap.begin() ; itrM!=_kvMap.end() ; itrM++) {
    //    string val = "\\N";
    //    if (itrM->second != "") {
    //        val = itrM->second;
    //    }
    //    LOGF_INFO("%1%\t%2%" % itrM->first % val);
    //}
}

boost::shared_ptr<KvInterfaceImplMem>
KvInterfaceImplMem::clone() const {
    boost::shared_ptr<KvInterfaceImplMem> newOne = boost::make_shared<KvInterfaceImplMem>();
    newOne->_kvMap = _kvMap;
    return newOne;
}

}}} // namespace lsst::qserv::css
