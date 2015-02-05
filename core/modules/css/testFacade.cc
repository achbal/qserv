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
  * @brief Unit test for the Facade class.
  *
  * @Author Jacek Becla, SLAC
  */


// System headers
#include <algorithm> // sort
#include <cstdlib>   // rand, srand
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <time.h>    // time

// Third-party headers
#include "boost/lexical_cast.hpp"
#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/make_shared.hpp"

// Qserv headers
#include "css/CssError.h"
#include "css/Facade.h"
#include "css/KvInterfaceImplMem.h"

// Boost unit test header
#define BOOST_TEST_MODULE TestFacade
#include "boost/test/included/unit_test.hpp"

using std::cout;
using std::endl;
using std::make_pair;
using std::string;
using std::vector;

namespace lsst {
namespace qserv {
namespace css {

struct InitRand {
    InitRand() {
        srand(time(NULL));
    }
};
InitRand _initRand;

// some tests do not need fixure
struct EmptyFixture {
};

// Test fixure that instantiates a Facade with pre-loaded data
struct FacadeFixture {
    FacadeFixture() {

        // version key must exist before Facade is instantiated
        std::istringstream stream("/css_meta\t\\N\n/css_meta/version\t" +
                boost::lexical_cast<string>(Facade::cssVersion()));
        kvI.reset(new KvInterfaceImplMem(stream));
        facade = FacadeFactory::createCacheFacade(kvI, ".");

        kv.push_back(make_pair("/", ""));

        kv.push_back(make_pair("/PARTITIONING", ""));
        string p = "/PARTITIONING/_0000000001";
        kv.push_back(make_pair(p, ""));
        kv.push_back(make_pair(p+"/nStripes", "60"));
        kv.push_back(make_pair(p+"/nSubStripes", "18"));
        kv.push_back(make_pair(p+"/overlap", "0.025"));

        kv.push_back(make_pair("/DBS", ""));
        kv.push_back(make_pair("/DBS/dbA", ""));
        kv.push_back(make_pair("/DBS/dbA/partitioningId", "0000000001"));
        kv.push_back(make_pair("/DBS/dbB", ""));
        kv.push_back(make_pair("/DBS/dbC", ""));
        p = "/DBS/dbA/TABLES";
        kv.push_back(make_pair(p, ""));
        kv.push_back(make_pair(p + "/Object", ""));
        kv.push_back(make_pair(p + "/Object/partitioning", ""));
        kv.push_back(make_pair(p + "/Object/partitioning/lonColName", "ra_PS"));
        kv.push_back(make_pair(p + "/Object/partitioning/latColName", "decl_PS"));
        kv.push_back(make_pair(p + "/Object/partitioning/subChunks", "1"));
        kv.push_back(make_pair(p + "/Object/partitioning/dirColName","objectId"));
        kv.push_back(make_pair(p + "/Source", ""));
        kv.push_back(make_pair(p + "/Source/partitioning", ""));
        kv.push_back(make_pair(p + "/Source/partitioning/lonColName", "ra"));
        kv.push_back(make_pair(p + "/Source/partitioning/latColName", "decl"));
        kv.push_back(make_pair(p + "/Source/partitioning/subChunks", "0"));
        kv.push_back(make_pair(p + "/FSource", ""));
        kv.push_back(make_pair(p + "/FSource/partitioning", ""));
        kv.push_back(make_pair(p + "/FSource/partitioning/lonColName", "ra"));
        kv.push_back(make_pair(p + "/FSource/partitioning/latColName", "decl"));
        kv.push_back(make_pair(p + "/FSource/partitioning/subChunks", "0"));
        kv.push_back(make_pair(p + "/Exposure", ""));

        p = "/DBS/dbB/TABLES";
        kv.push_back(make_pair(p, ""));
        kv.push_back(make_pair(p + "/Exposure", ""));

        vector<std::pair<string, string> >::const_iterator itr;
        cout << "--------------" << endl;
        for (itr=kv.begin() ; itr!=kv.end() ; ++itr) {
            cout << itr->first << " --> " << itr->second << endl;
            kvI->create(itr->first, itr->second);
        }
        cout << "--------------" << endl;
    };

    ~FacadeFixture(void) {
    };

    std::string prefix;
    boost::shared_ptr<KvInterfaceImplMem> kvI;
    vector<std::pair<string, string> > kv;
    boost::shared_ptr<Facade> facade;
};

BOOST_FIXTURE_TEST_SUITE(FacadeTest, FacadeFixture)

BOOST_FIXTURE_TEST_CASE(test_noVersion, EmptyFixture) {
    // check that Facade throws exception for missing version key
    boost::shared_ptr<KvInterfaceImplMem> kvI = boost::make_shared<KvInterfaceImplMem>();
    BOOST_CHECK_THROW(FacadeFactory::createCacheFacade(kvI, "."), VersionMissingError);
}

BOOST_FIXTURE_TEST_CASE(test_wrongVersion, EmptyFixture) {
    // check that Facade throws exception for mismatching version key
    std::istringstream stream("/css_meta\t\\N\n/css_meta/version\t1000000000");
    boost::shared_ptr<KvInterfaceImplMem> kvI(new KvInterfaceImplMem(stream));
    BOOST_CHECK_THROW(FacadeFactory::createCacheFacade(kvI, "."), VersionMismatchError);
}

BOOST_FIXTURE_TEST_CASE(test_okVersion, EmptyFixture) {
    // check that Facade throws exception for mismatching version key
    std::istringstream stream("/css_meta\t\\N\n/css_meta/version\t" +
            boost::lexical_cast<string>(Facade::cssVersion()));
    boost::shared_ptr<KvInterfaceImplMem> kvI(new KvInterfaceImplMem(stream));
    BOOST_CHECK_NO_THROW(FacadeFactory::createCacheFacade(kvI, "."));
}

BOOST_AUTO_TEST_CASE(containsDb) {
    BOOST_CHECK_EQUAL(facade->containsDb("dbA"), true);
    BOOST_CHECK_EQUAL(facade->containsDb("dbB"), true);
    BOOST_CHECK_EQUAL(facade->containsDb("Dummy"), false);
}

BOOST_AUTO_TEST_CASE(containsTable) {
    // it does
    BOOST_CHECK_EQUAL(facade->containsTable("dbA", "Object"), true);

    // it does not
    BOOST_CHECK_EQUAL(facade->containsTable("dbA", "NotHere"), false);

    // for non-existing db
    BOOST_CHECK_THROW(facade->containsTable("Dummy", "NotHere"), NoSuchDb);
}

BOOST_AUTO_TEST_CASE(tableIsChunked) {
    // normal, table exists
    BOOST_CHECK_EQUAL(facade->tableIsChunked("dbA", "Object"), true);
    BOOST_CHECK_EQUAL(facade->tableIsChunked("dbA", "Source"), true);
    BOOST_CHECK_EQUAL(facade->tableIsChunked("dbA", "Exposure"), false);

    // normal, table does not exist
    BOOST_CHECK_THROW(facade->tableIsChunked("dbA", "NotHere"), NoSuchTable);

    // for non-existing db
    BOOST_CHECK_THROW(facade->tableIsChunked("Dummy", "NotHere"), NoSuchDb);
}

BOOST_AUTO_TEST_CASE(tableIsSubChunked) {
    // normal, table exists
    BOOST_CHECK_EQUAL(facade->tableIsSubChunked("dbA", "Object"), true);
    BOOST_CHECK_EQUAL(facade->tableIsSubChunked("dbA", "Source"), false);
    BOOST_CHECK_EQUAL(facade->tableIsSubChunked("dbA", "Exposure"), false);

    // normal, table does not exist
    BOOST_CHECK_THROW(facade->tableIsSubChunked("dbA", "NotHere"), NoSuchTable);

    // for non-existing db
    BOOST_CHECK_THROW(facade->tableIsSubChunked("Dummy", "NotHere"), NoSuchDb);
}

BOOST_AUTO_TEST_CASE(getAllowedDbs) {
    vector<string> v = facade->getAllowedDbs();
    BOOST_CHECK_EQUAL(3U, v.size());
    std::sort (v.begin(), v.end());
    BOOST_CHECK_EQUAL(v[0], "dbA");
    BOOST_CHECK_EQUAL(v[1], "dbB");
    BOOST_CHECK_EQUAL(v[2], "dbC");
}

BOOST_AUTO_TEST_CASE(getChunkedTables) {
    // normal, 3 values
    vector<string> v = facade->getChunkedTables("dbA");
    BOOST_CHECK_EQUAL(3U, v.size());
    std::sort (v.begin(), v.end());
    BOOST_CHECK_EQUAL(v[0], "FSource");
    BOOST_CHECK_EQUAL(v[1], "Object");
    BOOST_CHECK_EQUAL(v[2], "Source");

    // normal, no values
    v = facade->getChunkedTables("dbB");
    BOOST_CHECK_EQUAL(0U, v.size());

    // for non-existing db
    BOOST_CHECK_THROW(facade->getChunkedTables("Dummy"), NoSuchDb);
}

BOOST_AUTO_TEST_CASE(getSubChunkedTables) {
    // normal, 2 values
    vector<string> v = facade->getSubChunkedTables("dbA");
    BOOST_CHECK_EQUAL(1U, v.size());
    //std::sort (v.begin(), v.end());
    BOOST_CHECK_EQUAL(v[0], "Object");

    // normal, no values
    v = facade->getSubChunkedTables("dbB");
    BOOST_CHECK_EQUAL(0U, v.size());

    // for non-existing db
    BOOST_CHECK_THROW(facade->getSubChunkedTables("Dummy"), NoSuchDb);
}

BOOST_AUTO_TEST_CASE(getPartitionCols) {
    // normal, has value
    vector<string> v = facade->getPartitionCols("dbA", "Object");
    BOOST_CHECK_EQUAL(v.size(), 3U);
    BOOST_CHECK_EQUAL(v[0], "ra_PS");
    BOOST_CHECK_EQUAL(v[1], "decl_PS");
    BOOST_CHECK_EQUAL(v[2], "objectId");

    v = facade->getPartitionCols("dbA", "Source");
    BOOST_CHECK_EQUAL(v.size(), 3U);
    BOOST_CHECK_EQUAL(v[0], "ra");
    BOOST_CHECK_EQUAL(v[1], "decl");
    BOOST_CHECK_EQUAL(v[2], "");

    // for non-existing db
    BOOST_CHECK_THROW(facade->getPartitionCols("Dummy", "x"), NoSuchDb);
}

BOOST_AUTO_TEST_CASE(getChunkLevel) {
    BOOST_CHECK_EQUAL(facade->getChunkLevel("dbA", "Object"), 2);
    BOOST_CHECK_EQUAL(facade->getChunkLevel("dbA", "Source"), 1);
    BOOST_CHECK_EQUAL(facade->getChunkLevel("dbA", "Exposure"), 0);
}

BOOST_AUTO_TEST_CASE(getSecIndexColName) {
    // normal, has value
    BOOST_CHECK_EQUAL(facade->getSecIndexColNames("dbA", "Object")[0], "objectId");

    // normal, does not have value
    BOOST_CHECK_EQUAL(facade->getSecIndexColNames("dbA", "Source").empty(), true);

    // for non-existing db
    BOOST_CHECK_THROW(facade->getSecIndexColNames("Dummy", "x"), NoSuchDb);
}

BOOST_AUTO_TEST_CASE(getDbStriping) {
    StripingParams s = facade->getDbStriping("dbA");
    BOOST_CHECK_EQUAL(s.stripes, 60);
    BOOST_CHECK_EQUAL(s.subStripes, 18);
}

BOOST_AUTO_TEST_SUITE_END()

}}} // namespace lsst::qserv::css
