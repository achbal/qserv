/* 
 * LSST Data Management System
 * Copyright 2008, 2009, 2010 LSST Corporation.
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

#include "lsst/qserv/worker/QservPathStructure.h"
#include <sys/stat.h>
#include <fstream>
#include <iostream>

namespace qWorker = lsst::qserv::worker;

using std::string;
using std::vector;

bool
qWorker::QservPathStructure::insert(const vector<string>& paths) {
    _paths.clear();
    _uniqueDirs.clear();

    vector<string>::const_iterator pItr;
    for ( pItr=paths.begin(); pItr!=paths.end(); ++pItr) {
        if ( pathsContains(*pItr) ) { // don't store duplicates
            continue;
        }
        _paths.push_back(*pItr);
        int pos = pItr->find_last_of('/');
        if ( pos == -1 ) {
            std::cerr << "Problems with path: " << *pItr << std::endl;
            return false;
        }
        bool isDbDir = true;
        if ( ! processOneDir(pItr->substr(0, pos), isDbDir) ) {
            return false;
        }
    }
    return true;
}

bool
qWorker::QservPathStructure::persist() {
    if ( isRegistered() ) {
        return false;
    }
    if ( !createDirectories() ) {
        return false;
    }
    if ( !createPaths() ) {
        return false;
    }
    return true;
}

bool
qWorker::QservPathStructure::createDirectories() const {
    vector<string>::const_iterator dItr;
    for ( dItr=_uniqueDirs.begin(); dItr!=_uniqueDirs.end(); ++dItr) {
        const char* theDir = dItr->c_str();

        struct stat st;
        if ( stat(theDir, &st) != 0 ) {
            std::cout << "mkdir: " << theDir << std::endl;
            int n = mkdir(theDir, 0755);
            if ( n != 0 ) {
                std::cerr << "Failed to mkdir(" << *dItr << "), err: " 
                          << n << std::endl;
                return false;
            }
        } else {
            std::cout << theDir << " exists" << std::endl;
        }
    }
    return true;
}

// returns true if at least one db is already registered
bool
qWorker::QservPathStructure::isRegistered() const {
    std::vector<std::string>::const_iterator i;
    for ( i=_uniqueDbDirs.begin(); i!=_uniqueDbDirs.end(); ++i) {
        if ( isRegistered(i->c_str()) ) {
            std::cerr << "Database already registered ("
                      << *i << ")" << std::endl;
            return true;
        }
    }
    return false;
}

bool
qWorker::QservPathStructure::isRegistered(const char* dbPath) const {
    struct stat statbuf;
    if (stat(dbPath, &statbuf) != -1) {
        return S_ISDIR(statbuf.st_mode);
    }
    return false;
}

bool
qWorker::QservPathStructure::createPaths() const {
    vector<string>::const_iterator itr;
    for ( itr=_paths.begin(); itr!=_paths.end(); ++itr) {
        const char* path = itr->c_str();
        std::cout << "creating file: " << path << std::endl;
        std::ofstream f(path, std::ios::out);
        f.close();
    }
    return true;
}

const std::vector<std::string>
qWorker::QservPathStructure::uniqueDirs() const {
    return _uniqueDirs;
}

const std::vector<std::string>
qWorker::QservPathStructure::uniqueDbDirs() const {
    return _uniqueDbDirs;
}

void
qWorker::QservPathStructure::printUniqueDirs() const {
    std::vector<std::string>::const_iterator i;
    for ( i=_uniqueDirs.begin(); i!=_uniqueDirs.end(); ++i) {
        std::cout << "Unique dir: " << *i << std::endl;
    }
}

void
qWorker::QservPathStructure::printUniqueDbDirs() const {
    std::vector<std::string>::const_iterator i;
    for ( i=_uniqueDbDirs.begin(); i!=_uniqueDbDirs.end(); ++i) {
        std::cout << "Unique db dir: " << *i << std::endl;
    }
}

bool
qWorker::QservPathStructure::processOneDir(const string& s, 
                                           bool isDbDir)
{
    int pos = s.find_last_of('/');
    if ( pos == -1 ) {
        std::cerr << "Problems with path: " << s << std::endl;
        return false;
    } else if ( pos > 2 ) { // there is at least one more parent dir
        bool isDbDir = false;
        if ( !processOneDir(s.substr(0, pos), isDbDir) ) {
            return false;
        }
    }
    if ( !uniqueDirsContains(s) ) {
        _uniqueDirs.push_back(s);
    }
    if ( isDbDir && !uniqueDbDirsContains(s) ) {
        _uniqueDbDirs.push_back(s);
    }        
    return true;
}

bool
qWorker::QservPathStructure::pathsContains(const std::string& s) const {
    vector<string>::const_iterator i;
    for (i=_paths.begin() ; i!=_paths.end(); ++i) {
        if (*i == s) {
            return true;
        }
    }
    return false;
}

bool
qWorker::QservPathStructure::uniqueDirsContains(const std::string& s) const {
    vector<string>::const_iterator i;
    for (i=_uniqueDirs.begin() ; i!=_uniqueDirs.end(); ++i) {
        if (*i == s) {
            return true;
        }
    }
    return false;
}

bool
qWorker::QservPathStructure::uniqueDbDirsContains(const std::string& s) const {
    vector<string>::const_iterator i;
    for (i=_uniqueDbDirs.begin() ; i!=_uniqueDbDirs.end(); ++i) {
        if (*i == s) {
            return true;
        }
    }
    return false;
}
