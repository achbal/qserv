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
  /**
  * @brief Simple testing for GroupedQueue and CirclePqueue
  *
  * @author Daniel L. Wang, SLAC
  */
#define BOOST_TEST_MODULE
#include "boost/test/included/unit_test.hpp"
#include "lsst/qserv/worker/GroupedQueue.h"
#include "lsst/qserv/worker/CirclePqueue.h"

namespace test = boost::test_tools;
namespace qWorker = lsst::qserv::worker;
using lsst::qserv::worker::GroupedQueue;
using lsst::qserv::worker::CirclePqueue;

int const compareLimit = 1000;

struct KeyedElem {
    struct KeyEqual {
        bool operator()(KeyedElem const& a, KeyedElem const& b) {
            return a.id == b.id;
        };
    };
    inline bool operator==(KeyedElem const& b) const {
        return (id == b.id) &&
            (0 == strncmp(name, b.name, compareLimit));
    }
    struct Equal {
        bool operator()(KeyedElem const& a, KeyedElem const& b) {
            return a == b;
        }
    };
    inline bool operator<(KeyedElem const& b) const {
        return (id < b.id)  || 
            ((id == b.id) &&
             (strncmp(name, b.name, compareLimit) < 0));
    };
    struct Less {
        bool operator()(KeyedElem const& a, KeyedElem const& b) {
            return a < b; 
        }
    };
    struct GetKey {
        typedef int value_type;
        value_type operator()(KeyedElem const& a) { return a.id; }
    };

    int id;
    char const* name;
};
typedef boost::shared_ptr<KeyedElem> KeyedElemPtr;
std::ostream& operator<<(std::ostream& os, KeyedElem const& a) {
    return (os << a.id << ":" << a.name);    
}

template<class T, class Equal>
struct PtrEqual {
    bool operator()(boost::shared_ptr<T> const& a,
                    boost::shared_ptr<T> const& b) {
        return e(*a, *b);
    };
    Equal e;
};

struct QueueFixture {
    GroupedQueue<KeyedElem, KeyedElem::KeyEqual> gQueue;
    CirclePqueue<KeyedElem, KeyedElem::Less, KeyedElem::GetKey> circle;
};

KeyedElem const elts[] = {
    {1, "1 one"},
    {1, "1 two"},
    {2, "2 one"},
    {2, "2 two"},
    {3, "3 one"},
    {3, "3 two"},
    {4, "4 one"},
    {4, "4 two"},
};
int const eltSize = 8;

BOOST_FIXTURE_TEST_SUITE(QueueTest, QueueFixture)

BOOST_AUTO_TEST_CASE(Grouped_1) {
    // Basic test: insert in simple, pre-sorted order and verify that
    // ordering is preserved.
    for(int i=0; i < eltSize; ++i ) {
        KeyedElem e(elts[i]);
        gQueue.insert(e);
    }
    for(int i=0; i < eltSize; ++i ) {
        KeyedElem e = gQueue.front();
        BOOST_CHECK_EQUAL(elts[i], e);
        gQueue.pop_front();
    }
    BOOST_CHECK(gQueue.empty());
}

BOOST_AUTO_TEST_CASE(Grouped_2) {
    // Check ordering: insert one element from each key, then more
    // elements with the same key, and ensure that keys are grouped.
    std::set<int> seen;

    int last = -1;
    for(int i=0; i < eltSize/2; ++i ) {
        KeyedElem e(elts[i*2]);
        gQueue.insert(e);
    }
    for(int i=0; i < eltSize/2; ++i ) {
        KeyedElem e(elts[(i*2) + 1]);
        gQueue.insert(e);
    }
    BOOST_CHECK_EQUAL(gQueue.size(), eltSize);

    for(int i=0; i < eltSize; ++i ) {
        KeyedElem e = gQueue.front();
        if(seen.find(e.id) != seen.end()) { // Have we seen it?
            BOOST_CHECK_EQUAL(e.id, last); // It must continue the
                                           // current grouping
        } else {
            seen.insert(e.id);
        }
        last = e.id;
        gQueue.pop_front();
    }
    BOOST_CHECK(gQueue.empty());
}

BOOST_AUTO_TEST_CASE(Circle_1) {
    // Basic test: insert in simple, pre-sorted order and verify that
    // ordering is preserved.
    for(int i=0; i < eltSize; ++i ) {
        circle.insert(elts[i]);
    }
    for(int i=0; i < eltSize; ++i ) {
        KeyedElem e = circle.front();
        BOOST_CHECK_EQUAL(elts[i], e);
        circle.pop_front();
    }
    BOOST_CHECK(circle.empty());
}


BOOST_AUTO_TEST_CASE(Circle_2) {
    // Check ordering: insert one element from each key, then more
    // elements with the same key, and ensure that keys are grouped.
    std::set<int> seen;

    int last = -1;
    // Insert in reverse order. Circle should enforce ordering.
    for(int i=(eltSize-1)/2; i >= 0; --i ) { 
        circle.insert(elts[i*2]);
    }
    BOOST_CHECK_EQUAL(circle.size(), 4);
    for(int i=0; i < eltSize/2; ++i ) {
        circle.insert(elts[(i*2) + 1]);
    }
    BOOST_CHECK_EQUAL(circle.size(), eltSize);
    BOOST_REQUIRE(circle.size() == eltSize);

    // Now pull contents out.
    for(int i=0; i < eltSize; ++i ) {
        KeyedElem e = circle.front();
        std::cout << "Element: " << e << std::endl;
        if(seen.find(e.id) != seen.end()) { // Have we seen it?
            BOOST_CHECK_EQUAL(e.id, last); // It must continue the
                                           // current grouping
        } else {
            seen.insert(e.id);
        }
        last = e.id;
        circle.pop_front();
    }
    BOOST_CHECK(circle.empty());
}

BOOST_AUTO_TEST_SUITE_END()
