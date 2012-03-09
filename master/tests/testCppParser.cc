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
 
#define BOOST_TEST_MODULE testCppParser
#include "boost/test/included/unit_test.hpp"
#include <list>
#include <map>
#include <string>
#include "lsst/qserv/master/SqlSubstitution.h"
#include "lsst/qserv/master/SqlParseRunner.h"

using lsst::qserv::master::ChunkMapping;
using lsst::qserv::master::SqlSubstitution;
using lsst::qserv::master::SqlParseRunner;
namespace test = boost::test_tools;

namespace {


std::auto_ptr<ChunkMapping> newTestMapping() {
    std::auto_ptr<ChunkMapping> cm(new ChunkMapping());
    cm->addChunkKey("Source");
    cm->addChunkKey("Object");
    return cm;
}

} // anonymous namespace

struct ParserFixture {
    ParserFixture(void) 
        : delimiter("%$#") {
	cMapping.addChunkKey("Source");
	cMapping.addSubChunkKey("Object");
        tableNames.push_back("Object");
        tableNames.push_back("Source");
        config["table.defaultdb"] ="LSST";
        config["table.alloweddbs"] = "LSST";
        config["table.partitioncols"] = "Object:ra_Test,decl_Test,objectIdObjTest;"
            "Source:raObjectTest,declObjectTest,objectIdSourceTest";

    };
    ~ParserFixture(void) { };

    SqlParseRunner::Ptr getRunner(std::string const& stmt) {
        return getRunner(stmt, config);
    }

    SqlParseRunner::Ptr getRunner(std::string const& stmt, 
                                  std::map<std::string,std::string> const& cfg) {
        SqlParseRunner::Ptr p;
        
        p = SqlParseRunner::newInstance(stmt, delimiter, cfg);
        p->setup(tableNames);
        return p;
    }
    ChunkMapping cMapping;
    std::list<std::string> tableNames;
    std::string delimiter;
    std::map<std::string, std::string> config;
    std::map<std::string, int> whiteList;
    std::string defaultDb;
};

//BOOST_FIXTURE_TEST_SUITE(QservSuite, ParserFixture)


//BOOST_AUTO_TEST_CASE(SqlSubstitution) {
void tryStmt(std::string const& s, bool withSubchunks=false) {
    std::map<std::string,std::string> cfg; // dummy config
    char const* imported[] = {"Source","Object"};
    ChunkMapping c;
    c.addChunkKey(imported[0]);
    c.addSubChunkKey(imported[1]);
    SqlSubstitution ss(s, c, cfg);
    if(withSubchunks) {
        ss.importSubChunkTables(imported);
    }
    if(!ss.getError().empty()) {
	std::cout << "ERROR constructing substitution: " 
		  << ss.getError() << std::endl;
	return;
    }

    for(int i = 4; i < 6; ++i) {
	std::cout << "--" << ss.transform(i, 3) 
                  << std::endl;
    }
}

void tryAutoSubstitute() {
    std::string stmt = "select * from LSST.Object as o1, LSST.Source where o1.id = 4 and LSST.Source.flux > 4 and ra < 5 and dista(ra,decl,ra,decl) < 1; select * from Temp;";
    tryStmt(stmt);
}

void tryNnSubstitute() {
    std::string stmt = "select * from LSST.Object as o1, LSST.Object as o2 where o1.id != o2.id and spdist(o1.ra,o1.decl,o2.ra,o2.decl) < 1;";
    stmt = "select * from LSST.Object as o1, LSST.Object as o2 where o1.id != o2.id and LSST.spdist(o1.ra,o1.decl,o2.ra,o2.decl) < 1 AND o1.id != o2.id;";
    tryStmt(stmt, true);
}

void tryTriple() {
    std::string stmt = "select * from LSST.Object as o1, LSST.Object as o2, LSST.Source where o1.id != o2.id and dista(o1.ra,o1.decl,o2.ra,o2.decl) < 1 and Source.oid=o1.id;";
    std::map<std::string,std::string> cfg; // dummy config
    ChunkMapping c;
    c.addChunkKey("Source");
    c.addSubChunkKey("Object");
    c.addSubChunkKey("ObjectSub");
    SqlSubstitution ss(stmt, c, cfg);
    for(int i = 4; i < 6; ++i) {
	std::cout << "--" << ss.transform(i,3) << std::endl;
    }
}

void tryAggregate() {
    std::string stmt = "select sum(pm_declErr),sum(bMagF), count(bMagF2) bmf2 from LSST.Object where bMagF > 20.0;";
    std::string stmt2 = "select sum(pm_declErr),chunkId, avg(bMagF2) bmf2 from LSST.Object where bMagF > 20.0 GROUP BY chunkId;";
    std::map<std::string,std::string> cfg; // dummy config

    std::auto_ptr<ChunkMapping> c = newTestMapping();
    SqlSubstitution ss(stmt, *c, cfg);
    for(int i = 4; i < 6; ++i) {
	std::cout << "--" << ss.transform(i,3) << std::endl;
    }
    SqlSubstitution ss2(stmt2, *c, cfg);
    std::cout << "--" << ss2.transform(24,3) << std::endl;
    
}

#if 1
BOOST_FIXTURE_TEST_SUITE(CppParser, ParserFixture)

BOOST_AUTO_TEST_CASE(TrivialSub) {
    std::string stmt = "SELECT * FROM Object WHERE someField > 5.0;";
    SqlParseRunner::Ptr spr = SqlParseRunner::newInstance(stmt, 
                                                          delimiter,
                                                          config);
    spr->setup(tableNames);
    std::string parseResult = spr->getParseResult();
    // std::cout << stmt << " is parsed into " << parseResult
    //           << std::endl;
    BOOST_CHECK(!parseResult.empty());
    BOOST_CHECK(spr->getHasChunks());
    BOOST_CHECK(!spr->getHasSubChunks());
    BOOST_CHECK(!spr->getHasAggregate());
}

BOOST_AUTO_TEST_CASE(NoSub) {
    std::string stmt = "SELECT * FROM Filter WHERE filterId=4;";
    std::string goodRes = "SELECT * FROM LSST.Filter WHERE filterId=4;";
    SqlParseRunner::Ptr spr(SqlParseRunner::newInstance(stmt, 
                                                        delimiter,
                                                        config));
    spr->setup(tableNames);
    std::string parseResult = spr->getParseResult();
    // std::cout << stmt << " is parsed into " << parseResult 
    //           << std::endl;
    BOOST_CHECK(!parseResult.empty());
    BOOST_CHECK_EQUAL(parseResult, goodRes);
    BOOST_CHECK(!spr->getHasChunks());
    BOOST_CHECK(!spr->getHasSubChunks());
    BOOST_CHECK(!spr->getHasAggregate());
    
}

BOOST_AUTO_TEST_CASE(Aggregate) {
    std::string stmt = "select sum(pm_declErr),chunkId, avg(bMagF2) bmf2 from LSST.Object where bMagF > 20.0 GROUP BY chunkId;";

    SqlSubstitution ss(stmt, cMapping, config);
    for(int i = 4; i < 6; ++i) {
	// std::cout << "--" << ss.transform(cMapping.getMapping(i,3),i,3) 
        //           << std::endl;
        BOOST_CHECK_EQUAL(ss.getChunkLevel(), 1);
        BOOST_CHECK(ss.getHasAggregate());
        // std::cout << "fixupsel " << ss.getFixupSelect() << std::endl
        //           << "fixuppost " << ss.getFixupPost() << std::endl;
        BOOST_CHECK_EQUAL(ss.getFixupSelect(), 
                          "sum(`sum(pm_declErr)`) AS `sum(pm_declErr)`, `chunkId`, SUM(avgs_bMagF2)/SUM(avgc_bMagF2) AS `bmf2`");
        BOOST_CHECK_EQUAL(ss.getFixupPost(), "GROUP BY `chunkId`");

    }
}

BOOST_AUTO_TEST_CASE(Limit) {
    std::string stmt = "select * from LSST.Object WHERE ra_PS BETWEEN 150 AND 150.2 and decl_PS between 1.6 and 1.7 limit 2;";
    
    SqlSubstitution ss(stmt, cMapping, config);
    for(int i = 4; i < 6; ++i) {
	//std::cout << "--" << ss.transform(cMapping.getMapping(i,3),i,3) 
        //           << std::endl;
        BOOST_CHECK_EQUAL(ss.getChunkLevel(), 1);
        BOOST_CHECK(!ss.getHasAggregate());
        if(!ss.getError().empty()) { std::cout << ss.getError() << std::endl;}
        BOOST_CHECK(ss.getError().empty());
        // std::cout << "fixupsel " << ss.getFixupSelect() << std::endl
        //           << "fixuppost " << ss.getFixupPost() << std::endl;

    }
}

BOOST_AUTO_TEST_CASE(OrderBy) {
    std::string stmt = "select * from LSST.Object WHERE ra_PS BETWEEN 150 AND 150.2 and decl_PS between 1.6 and 1.7 ORDER BY objectId;";
    
    SqlSubstitution ss(stmt, cMapping, config);
    for(int i = 4; i < 6; ++i) {
	//std::cout << "--" << ss.transform(cMapping.getMapping(i,3),i,3) 
        //           << std::endl;
        BOOST_CHECK_EQUAL(ss.getChunkLevel(), 1);
        BOOST_CHECK(!ss.getHasAggregate());
        if(!ss.getError().empty()) { std::cout << ss.getError() << std::endl;}
        BOOST_CHECK(ss.getError().empty());
        // std::cout << "fixupsel " << ss.getFixupSelect() << std::endl
        //           << "fixuppost " << ss.getFixupPost() << std::endl;

    }
}
void testStmt2(SqlParseRunner::Ptr spr, bool shouldFail=false) {
    std::cout << "Testing: " << spr->getStatement() << std::endl;
    std::string parseResult = spr->getParseResult();
    // std::cout << stmt << " is parsed into " << parseResult
    //           << std::endl;
    
    if(shouldFail) {
        BOOST_CHECK(!spr->getError().empty());
    } else {
        if(!spr->getError().empty()) { 
            std::cout << spr->getError() << std::endl;
        }
        BOOST_CHECK(spr->getError().empty());
        BOOST_CHECK(!parseResult.empty());
    }
}

BOOST_AUTO_TEST_CASE(RestrictorBox) {
    std::string stmt = "select * from Object where qserv_areaspec_box(0,0,1,1);";
    SqlParseRunner::Ptr spr = getRunner(stmt);
    testStmt2(spr);
    BOOST_CHECK(spr->getHasChunks());
    BOOST_CHECK(!spr->getHasSubChunks());
    BOOST_CHECK(!spr->getHasAggregate());

}
BOOST_AUTO_TEST_CASE(RestrictorObjectId) {
    std::string stmt = "select * from Object where qserv_objectId(2,3145,9999);";
    SqlParseRunner::Ptr spr = getRunner(stmt);
    testStmt2(spr);
    BOOST_CHECK(spr->getHasChunks());
    BOOST_CHECK(!spr->getHasSubChunks());
    BOOST_CHECK(!spr->getHasAggregate());

}
BOOST_AUTO_TEST_CASE(RestrictorObjectIdAlias) {
    std::string stmt = "select * from Object as o1 where qserv_objectId(2,3145,9999);";
    SqlParseRunner::Ptr spr = getRunner(stmt);
    testStmt2(spr);
    BOOST_CHECK(spr->getHasChunks());
    BOOST_CHECK(!spr->getHasSubChunks());
    BOOST_CHECK(!spr->getHasAggregate());
}
BOOST_AUTO_TEST_CASE(RestrictorNeighborCount) {
    std::string stmt = "select count(*) from Object as o1, Object as o2 where qserv_areaspec_box(6,6,7,7) AND o1.ra_PS between 6 and 7 and o1.decl_PS between 6 and 7 ;";
    SqlParseRunner::Ptr spr = getRunner(stmt);
    testStmt2(spr);
    BOOST_CHECK(spr->getHasChunks());
    BOOST_CHECK(spr->getHasSubChunks());
    BOOST_CHECK(spr->getHasAggregate());
}

BOOST_AUTO_TEST_CASE(BadDbAccess) {
    std::string stmt = "select count(*) from Bad.Object as o1, Object o2 where qserv_areaspec_box(6,6,7,7) AND o1.ra_PS between 6 and 7 and o1.decl_PS between 6 and 7 ;";
    SqlParseRunner::Ptr spr = getRunner(stmt);
    testStmt2(spr, true);
}

BOOST_AUTO_TEST_CASE(ObjectSourceJoin) {
    std::string stmt = "select * from LSST.Object o, Source s WHERE "
        "qserv_areaspec_box(2,2,3,3) AND o.objectId = s.objectId;";
    std::string expected = "select * from LSST.%$#Object%$# o,LSST.%$#Source%$# s WHERE (scisql_s2PtInBox(o.ra_Test,o.decl_Test,2,2,3,3) = 1) AND (scisql_s2PtInBox(s.raObjectTest,s.declObjectTest,2,2,3,3) = 1) AND o.objectId=s.objectId;";
    SqlParseRunner::Ptr spr = getRunner(stmt);
    testStmt2(spr);
    //std::cout << "Parse result: " << spr->getParseResult() << std::endl;
    BOOST_CHECK(spr->getHasChunks());
    BOOST_CHECK(!spr->getHasSubChunks());
    BOOST_CHECK(!spr->getHasAggregate());
    BOOST_CHECK_EQUAL(spr->getParseResult(), expected);
}

BOOST_AUTO_TEST_CASE(ObjectSelfJoin) {
    std::string stmt = "select count(*) from Object as o1, Object as o2;";
    std::string expected = "select count(*) from LSST.%$#Object_sc1%$# as o1,LSST.%$#Object_sc2%$# as o2 UNION select count(*) from LSST.%$#Object_sc1%$# as o1,LSST.%$#Object_sfo%$# as o2;";

    SqlParseRunner::Ptr spr = getRunner(stmt);
    testStmt2(spr);
    //std::cout << "Parse result: " << spr->getParseResult() << std::endl;
    BOOST_CHECK(spr->getHasChunks());
    BOOST_CHECK(spr->getHasSubChunks());
    BOOST_CHECK(spr->getHasAggregate());
}

BOOST_AUTO_TEST_CASE(ObjectSelfJoinQualified) {
    std::string stmt = "select count(*) from LSST.Object as o1, LSST.Object as o2;";
    std::string expected = "select count(*) from LSST.%$#Object_sc1%$# as o1,LSST.%$#Object_sc2%$# as o2 UNION select count(*) from LSST.%$#Object_sc1%$# as o1,LSST.%$#Object_sfo%$# as o2;";
    SqlParseRunner::Ptr spr = getRunner(stmt);
    testStmt2(spr);
    //std::cout << "Parse result: " << spr->getParseResult() << std::endl;
    BOOST_CHECK(spr->getHasChunks());
    BOOST_CHECK(spr->getHasSubChunks());
    BOOST_CHECK(spr->getHasAggregate());
    BOOST_CHECK_EQUAL(spr->getParseResult(), expected);
}

BOOST_AUTO_TEST_CASE(ObjectSelfJoinOutBand) {
    std::string stmt = "select count(*) from LSST.Object as o1, LSST.Object as o2;";
    std::string expected ="select count(*) from LSST.%$#Object_sc1%$# as o1,LSST.%$#Object_sc2%$# as o2 WHERE (scisql_s2PtInCircle(o1.ra_Test,o1.decl_Test,1,1,1.3) = 1) AND (scisql_s2PtInCircle(o2.ra_Test,o2.decl_Test,1,1,1.3) = 1) AND (scisql_s2PtInBox(o1.ra_Test,o1.decl_Test,5,2,6,3) = 1) AND (scisql_s2PtInBox(o2.ra_Test,o2.decl_Test,5,2,6,3) = 1) UNION select count(*) from LSST.%$#Object_sc1%$# as o1,LSST.%$#Object_sfo%$# as o2 WHERE (scisql_s2PtInCircle(o1.ra_Test,o1.decl_Test,1,1,1.3) = 1) AND (scisql_s2PtInCircle(o2.ra_Test,o2.decl_Test,1,1,1.3) = 1) AND (scisql_s2PtInBox(o1.ra_Test,o1.decl_Test,5,2,6,3) = 1) AND (scisql_s2PtInBox(o2.ra_Test,o2.decl_Test,5,2,6,3) = 1);";

    std::map<std::string, std::string> hintedCfg(config);
    hintedCfg["query.hints"] = "circle,1,1,1.3;box,5,2,6,3";
    SqlParseRunner::Ptr spr = getRunner(stmt, hintedCfg);
    testStmt2(spr);
    //std::cout << "Parse result: " << spr->getParseResult() << std::endl;
    BOOST_CHECK(spr->getHasChunks());
    BOOST_CHECK(spr->getHasSubChunks());
    BOOST_CHECK(spr->getHasAggregate());
    BOOST_CHECK_EQUAL(spr->getParseResult(), expected);
}

BOOST_AUTO_TEST_CASE(ObjectSelfJoinDistance) {
    std::string stmt = "select count(*) from LSST.Object o1,LSST.Object o2 WHERE scisql_angSep(o1.ra_PS,o1.decl_PS,o2.ra_PS,o2.decl_PS) < 0.2";
    std::string expected = "select count(*) from LSST.%$#Object_sc1%$# o1,LSST.%$#Object_sc2%$# o2 WHERE (scisql_s2PtInBox(o1.ra_Test,o1.decl_Test,5.5,5.5,6.1,6.1) = 1) AND (scisql_s2PtInBox(o2.ra_Test,o2.decl_Test,5.5,5.5,6.1,6.1) = 1) AND scisql_angSep(o1.ra_PS,o1.decl_PS,o2.ra_PS,o2.decl_PS)<0.2 UNION select count(*) from LSST.%$#Object_sc1%$# o1,LSST.%$#Object_sfo%$# o2 WHERE (scisql_s2PtInBox(o1.ra_Test,o1.decl_Test,5.5,5.5,6.1,6.1) = 1) AND (scisql_s2PtInBox(o2.ra_Test,o2.decl_Test,5.5,5.5,6.1,6.1) = 1) AND scisql_angSep(o1.ra_PS,o1.decl_PS,o2.ra_PS,o2.decl_PS)<0.2;";

    std::map<std::string, std::string> hintedCfg(config);
    hintedCfg["query.hints"] = "box,5.5,5.5,6.1,6.1";
    SqlParseRunner::Ptr spr = getRunner(stmt, hintedCfg);
    testStmt2(spr);
    
    BOOST_CHECK(spr->getHasChunks());
    BOOST_CHECK(spr->getHasSubChunks());
    BOOST_CHECK(spr->getHasAggregate());
    BOOST_CHECK_EQUAL(spr->getParseResult(), expected);
}

BOOST_AUTO_TEST_CASE(SelfJoinAliased) {
    // This is actually invalid for Qserv right now because it produces
    // a result that can't be stored in a table as-is.
    // It's also a non-distance-bound spatially-unlimited query. Qserv should
    // reject this. But the parser should still handle it. 
    std::string stmt = "select o1.ra_PS, o1.ra_PS_Sigma, o2.ra_PS, o2.ra_PS_Sigma from Object o1, Object o2 where o1.ra_PS_Sigma < 4e-7 and o2.ra_PS_Sigma < 4e-7;";
    std::string expected = "select o1.ra_PS,o1.ra_PS_Sigma,o2.ra_PS,o2.ra_PS_Sigma from LSST.%$#Object_sc1%$# o1,LSST.%$#Object_sc2%$# o2 where o1.ra_PS_Sigma<4e-7 and o2.ra_PS_Sigma<4e-7 UNION select o1.ra_PS,o1.ra_PS_Sigma,o2.ra_PS,o2.ra_PS_Sigma from LSST.%$#Object_sc1%$# o1,LSST.%$#Object_sfo%$# o2 where o1.ra_PS_Sigma<4e-7 and o2.ra_PS_Sigma<4e-7;"; 

    SqlParseRunner::Ptr spr = getRunner(stmt);
    testStmt2(spr);
    //std::cout << "Parse result: " << spr->getParseResult() << std::endl;
    BOOST_CHECK(spr->getHasChunks());
    BOOST_CHECK(spr->getHasSubChunks());
    BOOST_CHECK(!spr->getHasAggregate());
    BOOST_CHECK_EQUAL(spr->getParseResult(), expected);
}

BOOST_AUTO_TEST_CASE(AliasHandling) {
    std::string stmt = "select o1.ra_PS, o1.ra_PS_Sigma, s.dummy, Exposure.exposureTime from LSST.Object o1,  Source s, Exposure WHERE o1.id = s.objectId AND Exposure.id = o1.exposureId;";
    std::string expected = "select o1.ra_PS,o1.ra_PS_Sigma,s.dummy,Exposure.exposureTime from LSST.%$#Object%$# o1,LSST.%$#Source%$# s,LSST.Exposure WHERE o1.id=s.objectId AND Exposure.id=o1.exposureId;";
    SqlParseRunner::Ptr spr = getRunner(stmt);
    testStmt2(spr);
    //std::cout << "Parse result: " << spr->getParseResult() << std::endl;
    BOOST_CHECK(spr->getHasChunks());
    BOOST_CHECK(!spr->getHasSubChunks());
    BOOST_CHECK(!spr->getHasAggregate());
    BOOST_CHECK_EQUAL(spr->getParseResult(), expected);
}

BOOST_AUTO_TEST_CASE(SpatialRestr) {
    std::string stmt = "select count(*) from Object where qserv_areaspec_box(359.1, 3.16, 359.2,3.17);";
    std::string expected = "select count(*) from LSST.%$#Object%$# where (scisql_s2PtInBox(LSST.%$#Object%$#.ra_Test,LSST.%$#Object%$#.decl_Test,359.1,3.16,359.2,3.17) = 1);";

    SqlParseRunner::Ptr spr = getRunner(stmt);
    testStmt2(spr);
    //std::cout << "Parse result: " << spr->getParseResult() << std::endl;
    BOOST_CHECK(spr->getHasChunks());
    BOOST_CHECK(!spr->getHasSubChunks());
    BOOST_CHECK(spr->getHasAggregate());
    BOOST_CHECK_EQUAL(spr->getParseResult(), expected);
}

BOOST_AUTO_TEST_CASE(SpatialRestr2) {
    std::string stmt = "select count(*) from LSST.Object where qserv_areaspec_box(359.1, 3.16, 359.2,3.17);";
    std::string expected = "select count(*) from LSST.%$#Object%$# where (scisql_s2PtInBox(LSST.%$#Object%$#.ra_Test,LSST.%$#Object%$#.decl_Test,359.1,3.16,359.2,3.17) = 1);";

    SqlParseRunner::Ptr spr = getRunner(stmt);
    testStmt2(spr);
    //std::cout << "Parse result: " << spr->getParseResult() << std::endl;
    BOOST_CHECK(spr->getHasChunks());
    BOOST_CHECK(!spr->getHasSubChunks());
    BOOST_CHECK(spr->getHasAggregate());
    BOOST_CHECK_EQUAL(spr->getParseResult(), expected);
}


BOOST_AUTO_TEST_CASE(ChunkDensityFail) {
    std::string stmt = " SELECT count(*) AS n, AVG(ra_PS), AVG(decl_PS), _chunkId FROM Object GROUP BY _chunkId;";
    SqlParseRunner::Ptr spr = getRunner(stmt);
    testStmt2(spr, true); // Should fail since leading _ is disallowed.
}

BOOST_AUTO_TEST_CASE(ChunkDensity) {
    std::string stmt = " SELECT count(*) AS n, AVG(ra_PS), AVG(decl_PS), x_chunkId FROM Object GROUP BY x_chunkId;";
    SqlParseRunner::Ptr spr = getRunner(stmt);
    testStmt2(spr);
    BOOST_CHECK(spr->getHasChunks());
    BOOST_CHECK(!spr->getHasSubChunks());
    BOOST_CHECK(spr->getHasAggregate());
}

BOOST_AUTO_TEST_CASE(AltDbName) {
    std::string stmt = "select count(*) from Object where qserv_areaspec_box(359.1, 3.16, 359.2, 3.17);";
    std::string expected = "select count(*) from rplante_PT1_2_u_pt12prod_im3000_qserv.%$#Object%$# where (scisql_s2PtInBox(rplante_PT1_2_u_pt12prod_im3000_qserv.%$#Object%$#.ra_Test,rplante_PT1_2_u_pt12prod_im3000_qserv.%$#Object%$#.decl_Test,359.1,3.16,359.2,3.17) = 1);";

    config["table.defaultdb"] ="rplante_PT1_2_u_pt12prod_im3000_qserv";
    config["table.alloweddbs"] = "LSST,rplante_PT1_2_u_pt12prod_im3000_qserv";
    SqlParseRunner::Ptr spr = getRunner(stmt);
    testStmt2(spr);
    BOOST_CHECK(spr->getHasChunks());
    BOOST_CHECK(!spr->getHasSubChunks());
    BOOST_CHECK(spr->getHasAggregate());
    BOOST_CHECK_EQUAL(spr->getParseResult(), expected);
}

BOOST_AUTO_TEST_SUITE_END()

#endif
// SELECT o1.id as o1id,o2.id as o2id,
//        LSST.spdist(o1.ra, o1.decl, o2.ra, o2.decl) 
//  AS dist FROM Object AS o1, Object AS o2 
//  WHERE ABS(o1.decl-o2.decl) < 0.001 
//      AND LSST.spdist(o1.ra, o1.decl, o2.ra, o2.decl) < 0.001 
//      AND o1.id != o2.id;

#if 0
int main(int, char**) {
    //    tryAutoSubstitute();
    tryNnSubstitute();
    //tryTriple();
    //    tryAggregate();
    return 0;
}
#endif
