// -*- lsst-c++ -*-
/*
 * LSST Data Management System
 * Copyright 2014 AURA/LSST.
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

%define qserv_css_DOCSTRING
"
Access to the classes from the qserv_css library
"
%enddef

%module("threads"=1, package="lsst.qserv.css") cssLib
%{
#define SWIG_FILE_WITH_INIT
#include "css/constants.h"
#include "css/KvInterface.h"
#include "css/KvInterfaceImplMem.h"
#include "css/KvInterfaceImplMySql.h"
#include "global/constants.h"
#include "mysql/MySqlConfig.h"
#include "mysql/MySqlConnection.h"
%}

%include typemaps.i
%include cstring.i
%include "std_string.i"
%include "std_vector.i"
%include "stdint.i"

// Instantiate types
namespace std {
    %template(StringVector) vector<string>;
};

%include "css/constants.h"
%include "css/KvInterface.h"
%include "css/KvInterfaceImplMem.h"

// must be included before KvInterfaceImplMySql
%include "mysql/MySqlConfig.h" 
%include "sql/SqlConnection.h"

%include "css/KvInterfaceImplMySql.h"
%include "global/constants.h"
