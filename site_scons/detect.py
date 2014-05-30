#
# LSST Data Management System
# Copyright 2012-2014 LSST Corporation.
#
# This product includes software developed by the
# LSST Project (http://www.lsst.org/).
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the LSST License Statement and
# the GNU General Public License along with this program.  If not,
# see <http://www.lsstcorp.org/LegalNotices/>.
#
# detect_deps.py handles dependency detection for Qserv software

import SCons
import os, subprocess, sys
import state

def checkMySql(env):
    """Checks for MySQL includes and libraries in the following directories:
    * each prefix in searchRoots (lib/, lib64/, include/)
    * a built MySQL directory specified by the env var MYSQL_ROOT
    """
    conf = env.Configure()
    state.log.debug("checkMySql() %s %s" % (env["LIBPATH"], env["CPPPATH"]))

    conf.CheckCXXHeader('mysql/mysql.h')
    
    if conf.CheckLibWithHeader("mysqlclient_r", "mysql/mysql.h",
                                   language="C++", autoadd=0):
        if conf.CheckDeclaration("mysql_next_result",
                                 "#include <mysql/mysql.h>","c++" ):
            conf.Finish()
            return True
        else:
            state.log.fail("mysqlclient too old")
    else:
        # MySQL support not found or inadequate.
        state.log.fail("Could not locate MySQL headers (mysql/mysql.h)"\
            + " or find multithreaded mysql lib (mysqlclient_r)")

    conf.Finish()
    return None

class BoostChecker:
    def __init__(self, env):
        self.env = env
        self.suffix = None
        self.suffixes = ["-gcc41-mt", "-gcc34-mt", "-mt", ""]
        self.cache = {}
        pass

    def getLibName(self, libName):
        if libName in self.cache:
            return self.cache[libName]

        r = self._getLibName(libName)
        self.cache[libName] = r
        return r

    def _getLibName(self, libName):
        if self.suffix == None:
            conf = self.env.Configure()

            def checkSuffix(sfx):
                return conf.CheckLib(libName + sfx, language="C++", autoadd=0)
            for i in self.suffixes:
                if checkSuffix(i):
                    self.suffix = i
                    break
            if self.suffix == None:
                print "Can't find boost_" + libName
                assert self.suffix != None
            conf.Finish()
            pass
        return libName + self.suffix
    pass # BoostChecker

class AntlrChecker:
    def __init__(self, env):
        self.env = env
        self.suffix = None
        self.suffixes = ["-pic", ""]
        self.cache = {}
        pass

    def getLibName(self, libName):
        if libName in self.cache:
            return self.cache[libName]

        r = self._getLibName(libName)
        self.cache[libName] = r
        return r

    def _getLibName(self, libName):
        if self.suffix == None:
            conf = self.env.Configure()

            def checkSuffix(sfx):
                return conf.CheckLib(libName + sfx, language="C++", autoadd=0)
            for i in self.suffixes:
                if checkSuffix(i):
                    self.suffix = i
                    break
            if self.suffix == None:
                print "Can't find libantlr : " + libName
                assert self.suffix != None
            conf.Finish()
            pass
        return libName + self.suffix
    pass # AntlrChecker

null_source_file = """
int main(int argc, char **argv) {
        return 0;
}
"""

def checkLibs(context, libList):
    lastLIBS = context.env['LIBS']
    state.log.debug("checkLibs() %s" % lastLIBS)
    context.Message('checkLibs() : Checking for %s...' % ",".join(libList))
    context.env.Append(LIBS=libList)
    result = context.TryLink(null_source_file, '.cc')
    context.Result(result)
    context.env.Replace(LIBS=lastLIBS)
    return result


## Look for xrootd headers
def findXrootdInclude(env):
    hdrName = os.path.join("XrdPosix","XrdPosixLinkage.hh")
    conf = env.Configure()
    foundPath = None

    if conf.CheckCXXHeader(hdrName): # Try std location
        conf.Finish()
        return (True, None)

    # Extract CPPPATHs and look for xrootd/ within them.
    pList = env.Dump("CPPPATH") # Dump returns a stringified list

    # Convert to list if necessary
    if pList and type(pList) == type("") and str(pList)[0] == "[":
        pList = eval(pList)
    elif type(pList) != type(list): pList = [pList] # Listify
    pList.append("/usr/include")
    #pList.append("/usr/local/include")
    for p in pList:
        path = os.path.join(p, "xrootd")
        if os.access(os.path.join(path, hdrName), os.R_OK):
            conf.Finish()
            return (True, path)
    conf.Finish()
    return (False,None)

def checkXrootdLink(env, autoadd=0):
    libList = "XrdUtils XrdClient XrdPosix XrdPosixPreload".split()
    header = "XrdPosix/XrdPosixLinkage.hh"

    conf = env.Configure(custom_tests={
            'CheckLibs' : lambda c: checkLibs(c,libList)})

    found = conf.CheckLibs() and conf.CheckCXXHeader(header)
    conf.Finish()
    if not found:
        state.log.fail("Missing at least one xrootd lib or header file")
    return found


def setXrootd(env):
    (found, path) = findXrootdInclude(env)
    if not found :
        state.log.fail("Missing Xrootd include path")
    elif found and path:
        env.Append(CPPPATH=[path])
    return found


# --extern root handling
def _addInst(env, root):
    iPath = os.path.join(root, "include")
    if os.path.isdir(iPath): env.Append(CPPPATH=[iPath])
    for e in ["lib", "lib64"]:
        ep = os.path.join(root, e)
        if os.path.isdir(ep): env.Append(LIBPATH=[ep])
        pass

def addExtern(env, externPaths):
    if externPaths:
        for p in externPaths.split(":"):
            _addInst(env, p)
    pass


########################################################################
# custom.py mechanism
########################################################################
def importCustom(env, extraTgts):

    def getExt(ext):
        varNames = filter(lambda s: s.endswith(ext), env.Dictionary())
        vals = map(lambda varName: env[varName], varNames)
        state.log.debug("varNames : %s, vals %s" % (varNames, vals))
        return vals

    env.Append(LIBPATH=getExt("_LIB")) ## *LIB --> LIBPATH
    env.Append(CPPPATH=getExt("_INC")) ## *INC --> CPPPATH

    state.log.debug("CPPPATH : %s" % env['CPPPATH'])

    # Automagically steal PYTHONPATH from envvar
    extraTgts["PYTHONPATH"] = env.get("PYTHONPATH", []) 
    return None 

def checkTwisted():
    try:
        import twisted.internet
        state.log.info("Twisted python library found")
        return True
    except ImportError, e:
        return None
    pass
