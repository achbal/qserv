// -*- lsst-c++ -*-
/*
 * LSST Data Management System
 * Copyright 2009-2014 AURA/LSST.
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

%define qserv_czar_DOCSTRING
"
Access to the classes from the qserv_czar library
"
%enddef

%feature("autodoc", "1");
%module("threads"=1, package="lsst.qserv.czar", docstring=qserv_czar_DOCSTRING) czarLib

%{
#define SWIG_FILE_WITH_INIT
#include "ccontrol/QueryState.h"
#include "css/KvInterface.h"
#include "css/KvInterfaceImplMem.h"
#include "ccontrol/UserQueryFactory.h"
#include "ccontrol/userQueryProxy.h"
#include "global/constants.h"
%}

%include std_map.i
%include std_string.i
%include std_vector.i

%template(StringMap) std::map<std::string, std::string>;

%include "boost_shared_ptr.i"
%shared_ptr(lsst::qserv::css::KvInterface)
%shared_ptr(lsst::qserv::css::KvInterfaceImplMem)

%apply int *OUTPUT { int* chunkId, int* code, time_t* timestamp };

%include "ccontrol/QueryState.h"
%include "css/KvInterface.h"
%include "css/KvInterfaceImplMem.h"
%include "ccontrol/UserQueryFactory.h"
%include "ccontrol/userQueryProxy.h"
%include "global/constants.h"
