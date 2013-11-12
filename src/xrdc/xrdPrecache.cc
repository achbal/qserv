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
 

// Standard
#include <iostream>
#include <fcntl.h> // for O_RDONLY, O_WRONLY, etc.
#include <unistd.h>
// Boost
#include <boost/make_shared.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>

// Xrootd
#include "XrdPosix/XrdPosixXrootd.hh"

// Package
#include "lsst/qserv/common/WorkQueue.h"
#include "lsst/qserv/master/xrootd.h"
#include "lsst/qserv/master/xrdfile.h"

using lsst::qserv::common::WorkQueue;
namespace qMaster = lsst::qserv::master;

namespace {

class Callable : public WorkQueue::Callable {
public:
    typedef boost::shared_ptr<WorkQueue::Callable> CPtr;
    static CPtr makeShared(std::string const& url) {
        return CPtr(new Callable(url));
    }

    Callable(std::string const& url) : _url(url) {
    }
    virtual void operator()() {
        int result = qMaster::xrdOpen(_url.c_str(), O_WRONLY);
        {
            boost::lock_guard<boost::mutex> l(unitsMutex);
            int* ptr = &unitsDone;        
            ++unitsDone;
        }
        if(result < 0) {
            std::cout << "error " << result << " " << errno 
                      << " with path " 
                      << _url << std::endl;
            return;
        }
        result = qMaster::xrdClose(result);
        if(result < 0) {
            std::cout << "error closing path " << _url << std::endl;
            return;
        }
        
    }
    virtual void abort() {}
    static int unitsDone;
    static boost::mutex unitsMutex;
private:
    std::string _url;
};
int Callable::unitsDone = 0;


class App {
public:
    App(std::string const& hostport, int low, int high) 
        :_hostport(hostport), _low(low), _high(high) {
    }

    void initFromCmdLine(int argc, char const** argv) {
        if(argc > 1 ) {
            _hostport = argv[1];
            if(argc == 4) {
                _low = atoi(argv[2]);
                _high = atoi(argv[3]);
                            
            }
        }
    }
    
    void run() {
        int const poolSize = 1000;
        std::cout << "Using host=" << _hostport 
                  << " range: " << _low << " " << _high << std::endl;

        WorkQueue wq(poolSize);
        for(int i=_low; i < _high; ++i) {
            wq.add(Callable::makeShared(qMaster::makeUrl(_hostport.c_str(), 
                                                         "query2", i)));
        }
        while(Callable::unitsDone < (_high-_low)) {
            sleep(1);
        }
    }

private:
    std::string _hostport;
    std::string _urlTemp;
    int _low;
    int _high;
};

} // anonymous namespace


int main(int argc, char const** argv) {
    App a("boer0021:1094", 0, 7200);
    a.initFromCmdLine(argc, argv);
    a.run();
    return 0;
}
