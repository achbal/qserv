# 
# LSST Data Management System
# Copyright 2012-2013 LSST Corporation.
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

def detectProtobufs():
    """Checks for protobufs support (in the broadest sense)
    If PROTOC, PROTOC_INC and PROTOC_LIB do not exist as environment 
    variables, try to autodetect a system-installed ProtoBufs and set the
    environment variable accordingly. No other values are modified or 
    returned."""

    if not (os.environ.has_key("PROTOC")
            and os.environ["PROTOC_INC"] 
            and os.environ["PROTOC_LIB"]):
        try:
            output = subprocess.Popen(["protoc", "--version"], 
                                      stdout=subprocess.PIPE).communicate()[0]
            guessRoot = "/usr"
            incPath = os.path.join([guessRoot]
                                   + "include/google/protobuf".split("/"))
            testInc = os.path.join(incPath, "message.h")
            libPath = os.path.join(guessRoot, "lib")
            testLib = os.path.join(libPath, "libprotobuf.a")
            assert os.access(testInc, os.R_OK) and os.access(testLib, os.R_OK)
            print "Using guessed protoc and paths."
        except:
            print """
Can't continue without Google Protocol Buffers.
Make sure PROTOC, PROTOC_INC, and PROTOC_LIB env vars are set.
e.g., PROTOC=/usr/local/bin/protoc 
      PROTOC_INC=/usr/local/include 
      PROTOC_LIB=/usr/local/lib"""
            raise StandardError("FATAL ERROR: Can't build protocol without ProtoBufs")
        pass
    # Print what we're using.
    print "Protocol buffers using protoc=%s with include=%s and lib=%s" %(
        os.environ["PROTOC"], os.environ["PROTOC_INC"], 
        os.environ["PROTOC_LIB"])

def composeEnv(env, roots=[], includes=[], libs=[]):
    assert env
    env.Append(CPPPATH=includes)
    env.Append(CPPPATH=[os.path.join(x, "include") for x in roots])
    env.Append(LIBPATH=libs)    
    env.Append(LIBPATH=[os.path.join(x, "lib") for x in roots])
    return env

def checkMySql(env, Configure):
    """Checks for MySQL includes and libraries in the following directories:
    * each prefix in searchRoots (lib/, lib64/, include/)
    * a built MySQL directory specified by the env var MYSQL_ROOT
    Must pass Configure class from SCons
    """
    if os.environ.has_key('MYSQL_ROOT'):
        mysqlRoots = [os.environ['MYSQL_ROOT']]
        env.Prepend(CPPPATH=[os.path.join(mysqlRoots[0], "include")])
        # Look for mysql sub-directories. lib64 is important on RH/Fedora
        searchLibs = filter(os.path.exists, 
                            [os.path.join(r, lb, "mysql") 
                             for r in mysqlRoots for lb in ["lib","lib64"]])
        if searchLibs:
            env.Prepend(LIBPATH=searchLibs)
        pass

    conf = Configure(env)
    if conf.CheckLibWithHeader("mysqlclient_r", "mysql/mysql.h",
                                   language="C++"):
        if conf.CheckDeclaration("mysql_next_result", 
                                 "#include <mysql/mysql.h>","c++" ):
            return conf.Finish()
        else:
            print >> sys.stderr, "mysqlclient too old. (check MYSQL_ROOT)."
    else:
        print >> sys.stderr, "Could not locate MySQL headers (mysql/mysql.h)"\
            + " or find multithreaded mysql lib(mysqlclient_r)"
    # MySQL support not found or inadequate.
    return None
def checkMySql2(env):
    """Checks for MySQL includes and libraries in the following directories:
    * each prefix in searchRoots (lib/, lib64/, include/)
    * a built MySQL directory specified by the env var MYSQL_ROOT
    Must pass Configure class from SCons
    """
    if os.environ.has_key('MYSQL_ROOT'):
        mysqlRoots = [os.environ['MYSQL_ROOT']]
        env.Prepend(CPPPATH=[os.path.join(mysqlRoots[0], "include")])
        # Look for mysql sub-directories. lib64 is important on RH/Fedora
        searchLibs = filter(os.path.exists, 
                            [os.path.join(r, lb, "mysql") 
                             for r in mysqlRoots for lb in ["lib","lib64"]])
        if searchLibs:
            env.Prepend(LIBPATH=searchLibs)
        pass

    conf = env.Configure()
    if conf.CheckLibWithHeader("mysqlclient_r", "mysql/mysql.h",
                                   language="C++", autoadd=0):
        if conf.CheckDeclaration("mysql_next_result", 
                                 "#include <mysql/mysql.h>","c++" ):
            conf.Finish()
            return True
        else:
            print >> sys.stderr, "mysqlclient too old. (check MYSQL_ROOT)."
    else:
        print >> sys.stderr, "Could not locate MySQL headers (mysql/mysql.h)"\
            + " or find multithreaded mysql lib(mysqlclient_r)"
    # MySQL support not found or inadequate.
    return None


def guessMySQL(env):
    """Guesses the detected mysql dependencies based on the environment.
    Would be nice to reuse conf.CheckLib, but that doesn't report what 
    actually got detected, just that it was available. This solution 
    doesn't actually check beyond simple file existence.
    Returns (includepath, libpath, libname) """
    libName = "mysqlclient_r"
    libp = env["LIBPREFIX"]
    libs = env["SHLIBSUFFIX"]
    foundLibs = filter(lambda (p,f): os.path.exists(f),
                       [(p, os.path.join(p, libp + libName + libs))
                         for p in env["LIBPATH"]])
    assert foundLibs
    foundIncs = filter(os.path.exists, 
                       [os.path.join(p, "mysql/mysql.h") 
                        for p in env["CPPPATH"]])
    assert foundIncs
    return (foundIncs[0], foundLibs[0][0], libName)
    
# Xrootd/Scalla search helper
class XrdHelper:
    def __init__(self, roots):
        self.cands = roots
        if os.environ.has_key('XRD_DIR'):
            self.cands.insert(0, os.environ['XRD_DIR'])

        self.platforms = ["x86_64_linux_26", "x86_64_linux_26_dbg",
                          "i386_linux26", "i386_linux26_dbg"]
        if os.environ.has_key('XRD_PLATFORM'):
            self.platforms.insert(0, os.environ['XRD_PLATFORM'])
        pass

    def getXrdLibIncCmake(self):
        lib = self._findXrdLibCmake()
        print lib

    def _findXrdLibCmake(self, path):
        for lib in ["lib", "lib64"]:
            libpath = os.path.join(path, lib)
            if os.path.exists(os.path.join(libpath, "libXrdPosix.so")):
                return libpath
        return None
    def _findXrdIncCmake(self, path):
        neededFile = os.path.join(path, "include", "xrootd", "XrdPosix/XrdPosix.hh")
        if os.path.exists(neededFile):
            return p
        return None
        
    def getXrdLibInc(self):
        for c in self.cands:
            (inc, lib) = (self._findXrdInc(c), self._findXrdLibCmake(c))
            if inc and lib:
                return (inc, lib)
        return (None, None)

    def _findXrdLib(self, path):
        for p in self.platforms:
            libpath = os.path.join(path, "lib", p)
            if os.path.exists(libpath):
                print "Looking in ", libpath
                ## FIXME: not platform independent
                if os.path.exists(os.path.join(libpath, "libXrdPosix.so")):
                    return libpath
        return None

    def _findXrdInc(self, path):
        paths = map(lambda p: os.path.join(path, p), ["include/xrootd", "src"])
        for p in paths:
            neededFile = os.path.join(p, "XrdPosix/XrdPosix.hh")
            if os.path.exists(neededFile):
                return p
        return None
    pass
XRDFLAGS = ["-D_LARGEFILE_SOURCE", "-D_LARGEFILE64_SOURCE",
            "-D_FILE_OFFSET_BITS=64", "-D_REENTRANT",]

## Boost checker
def checkBoostHeader(conf, pkgList=[]):
    for p in pkgList:
        if not conf.CheckCXXHeader('boost/' + p + '.hpp'):
            return False
    return True
    
def checkAddBoost(conf, lib):
    """Check for a boost lib with various suffixes and add it to a Configure
    if found. (e.g. 'boost_regex' or 'boost_thread')"""
    return (conf.CheckLib(lib + "-gcc34-mt", language="C++") 
            or conf.CheckLib(lib + "-gcc41-mt", language="C++") 
            or conf.CheckLib(lib + "-mt", language="C++")
            or conf.CheckLib(lib, language="C++") 
            )

def checkAddAntlr(conf):
    found = conf.CheckLibWithHeader("antlr", "antlr/AST.hpp", 
                                    language="C++")
    if not found:
        print >> sys.stderr, "Could not locate libantlr or antlr/AST.hpp"
    return found

class BoostChecker:
    def __init__(self, env):
        self.env = env
        self.suffix = None
        self.suffixes = ["-gcc41-mt", "-gcc34-mt", "-mt", ""]
        pass

    def getLibName(self, libName):
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
        

def checkAddSslCrypto(conf):
    found =  conf.CheckLib("ssl") and conf.CheckLib("crypto")
    if not found:
        print >> sys.stderr, "Could not locate libssl or libcrypto"
    return found

def checkAddMySql(conf):
    found = conf.CheckLibWithHeader("mysqlclient_r", "mysql/mysql.h", 
                                    language="C++")
    if not found:
        print >> sys.stderr, "Could not find  mysql/mysql.h or",
        print >> sys.stderr, "multithreaded MySQL (mysqlclient_r)"
    return found

null_source_file = """
int main(int argc, char **argv) {
        return 0;
}
"""

def checkLibs(context, libList):
    lastLIBS = context.env['LIBS']
    context.Message('Checking for %s...' % ",".join(libList))
    context.env.Append(LIBS=libList)
    result = context.TryLink(null_source_file, '.cc')
    context.Result(result)
    context.env.Replace(LIBS=lastLIBS)
    return result

def checkXrdPosix(env, autoadd=0):
    libList = "XrdUtils XrdClient XrdPosix XrdPosixPreload".split()
    conf = env.Configure(custom_tests={
            'CheckLibs' : lambda c: checkLibs(c,libList)})
    def require(conf, libName):
        if not conf.CheckLib(libName, language="C++", autoadd=autoadd):
            print >> sys.stderr, "Could not find %s lib" % (libName)
            return False
        return True

    found = conf.CheckLibs()

    if False:
        found = (require(conf, "XrdUtils") and 
                 require(conf, ["XrdUtils","XrdClient"]) and 
                 require(conf, "XrdPosix") and
                 require(conf, "XrdPosixPreload"))
    found = found and conf.CheckCXXHeader("XrdPosix/XrdPosixLinkage.hh")
    conf.Finish()
    if not found:
        print >> sys.stderr, "Missing at least one xrootd lib/header"
    return found


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

def setXrootd(env):
    hdrName = "XrdPosix/XrdPosixLinkage.hh"
    conf = env.Configure()
    
    haveInc = conf.CheckCXXHeader(hdrName)
    if not haveInc:
        pList = env.Dump("CPPPATH") # Dumped returns a stringified list

        if pList and type(pList) == type("") : pList = eval(pList)
        pList.append("/usr/include")
        pList.append("/usr/local/include")
        for p in pList:
            path = os.path.join(p, "xrootd")
            if os.access(os.path.join(path, hdrName), os.R_OK):
                env.Append(CPPPATH=[path])
                haveInc = conf.CheckCXXHeader(hdrName)
        conf.Finish()
        if not haveInc: 
            raise StandardError("Xrootd includes not found")
    return haveInc
    

########################################################################
# dependency propagation tools
        


### Obsolete
def importDeps(env, f):
    post = {}
    fName = f+".deps"
    if os.access(fName, os.R_OK):
        deps = eval(open(fName).read()) # import dep file directly
        if "LIBS" in deps:
            post["LIBS"] = deps.pop("LIBS")
        #print "imported deps", deps
        env.Append(**deps)
    return post

def mergeDict(d1, d2):
    """Merge list values from d2 to d1"""
    for k in d2:
        if k in d1: d1[k].extend(d2[k])
        else: d1[k] = d2[k]
    return d1

def checkLibsFromDict(conf, depDict, autoadd=1):
    if "LIBS" in depDict:
        for lib in depDict["LIBS"]:
            conf.CheckLib(lib, language="C++", autoadd=autoadd)
    return conf
