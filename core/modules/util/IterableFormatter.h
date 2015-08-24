// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2015 AURA/LSST.
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
#ifndef LSST_QSERV_UTIL_CONTAINERFORMATTER_H
#define LSST_QSERV_UTIL_CONTAINERFORMATTER_H
 /**
  * @file
  *
  * @brief  Misc. lightweight vector manipulation.
  *
  * @author Fabrice Jammes, IN2P3/SLAC
  */

// System headers
#include <vector>
#include <iterator>
#include <sstream>
#include <string>

// Third-party headers

// LSST headers

// Qserv headers

namespace lsst {
namespace qserv {
namespace util {

/**
 *  Provide generic output operator for iterable data structures
 *
 *  Output format is [a, b, c, ...]
 *  The elements stored in the iterable data structure
 *  must provide an ouput operator.
 *  Examples:
 *  - works fine for std::vector<std::string>,
 *  - doesn't work for std::multimap.
 */
template <typename Iterable>
class IterableFormatter
{
public:
    explicit IterableFormatter(const Iterable& x) :
            ref(x)
    { }

    friend std::ostream& operator<<(std::ostream& os,
                                    const IterableFormatter<Iterable>& self
                                   )
    {
        os << "[";
        size_t last = self.ref.size() - 1;
        for(size_t i=0; i<self.ref.size(); ++i) {
            os <<  " " << self.ref[i];
            if (i != last)
                os << ",";
        }
        os << "]";
        return os;
    }
private:
    const Iterable& ref;
};

/**
 *  Create a printable wrapper for an iterable data structure
 *
 *  @param x:   an iterable data structure
 *  @return:    a printable wrapper for x
 */
template <typename Iterable>
IterableFormatter<Iterable> formatable(const Iterable& x)
{
    return IterableFormatter<Iterable>(x);
}

}}} // namespace lsst::qserv::util
#endif //  LSST_QSERV_UTIL_CONTAINERFORMATTER_H