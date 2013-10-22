/* 
 * LSST Data Management System
 * Copyright 2013 LSST Corporation.
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
#ifndef LSST_QSERV_CONSTANTS_H
#define LSST_QSERV_CONSTANTS_H
#include <string>
/**
  * @file 
  *
  * @brief Common Qserv Constants (to be moved to common package)
  * (SWIG must link in constants.cc)
  */
namespace lsst {
namespace qserv { 
// For now, we import using SWIG in masterLib.i, into
// lsst.qserv.master, ignoring the C++ declaration in the Python
// layer. 

char const CHUNK_COLUMN[] = "chunkId";
char const SUBCHUNK_COLUMN[] = "subChunkId";
}} // namespace lsst::qserv

#endif // LSST_QSERV_MASTER_CONSTANTS_H
