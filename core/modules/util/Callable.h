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
#ifndef LSST_QSERV_UTIL_CALLABLE_H
#define LSST_QSERV_UTIL_CALLABLE_H

// Third-party headers
#include <boost/shared_ptr.hpp>

namespace lsst {
namespace qserv {
namespace util {

template <typename Ret>
class VoidCallable {
public:
    typedef boost::shared_ptr<VoidCallable<Ret> > Ptr;
    virtual ~VoidCallable() {}
    virtual Ret operator()() = 0;
};

template <typename Ret, typename Arg>
class UnaryCallable {
public:
    typedef boost::shared_ptr<UnaryCallable<Ret,Arg> > Ptr;
    virtual ~UnaryCallable() {}
    virtual Ret operator()(Arg a) = 0;
};

}}} // lsst::qserv::util
#endif // LSST_QSERV_UTIL_CALLABLE_H
