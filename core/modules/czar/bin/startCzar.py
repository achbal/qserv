#!/usr/bin/env python

# 
# LSST Data Management System
# Copyright 2008-2014 LSST Corporation.
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

# startCzar.py -- This is a "driver" program that provides a
# command-line interface to start qserv or its tests.  Specifically,
# you can:
# * Run tests (invoke cases in testHintedParser.py and testAppInterface.py)
# * Start up a qserv frontend for testing.  Does not include starting
#   up the required xrootd/cmsd instances.

import unittest
from optparse import OptionParser
import sys

from lsst.qserv.czar.appTest import TestAppFunctions
from lsst.qserv.czar import server
from lsst.qserv.czar import app
from lsst.qserv.czar import client
from lsst.qserv.czar import config
from lsst.qserv.czar import indexing

import lsst.log as newlog

def runParserTest():
    """Invokes the test cases in the lsst.qserv.czar.testparser module
    """
    suite = unittest.TestLoader().loadTestsFromTestCase(TestAppFunctions)
    unittest.TextTestRunner(verbosity=2).run(suite)
    pass

def runNamedTest(name):
    suite = unittest.TestSuite()
    suite.addTest(TestAppFunctions('test'+name))
    unittest.TextTestRunner(verbosity=2).run(suite)
    pass

def resetTables():
    p = app.Persistence()
    p.activate()
    p.makeTables()
    pass

def makeIndexes():
    indexing.makeQservIndexes()
    pass

def main():    
    parser = OptionParser()

    # Db-backed task tracking is not supported right now.
    # parser.add_option("--reset-tables", action="store_true", 
    #                   dest="resettables", default=False, 
    #                   help="Reset tables instead of starting the server ()")
    parser.add_option("-t", "--test", action="store_true", 
                      dest="test", default=False, 
                      help="Run tests instead of starting the server")
    parser.add_option("-T",
                      dest="testName", default=None, metavar="NAME", 
                      help="Run a test named NAME.")
    parser.add_option("--sanity-client", action="store_true",
                      dest="sanityClient", default=False,
                      help="Sanity-check a running server.")
    parser.add_option("--index", action="store_true",
                      dest="makeIndex", default=False,
                      help="Rebuild indexes.")
    parser.add_option("-c", "--config", dest="configFile", default=None,
                      help="Use config file. Can also be specified with\n" +
                      "%s as an environment variable." % config.envFilenameVar)
    (options, args) = parser.parse_args()

    # Modifying options
    if options.configFile:
        config.load(options.configFile)
    else:
        config.load()
    print "Configuration:"
    config.printTo(sys.stdout)

    # Configure logging
        #----- note this section should be done in Fabrice's script
    runDir = "/usr/local/home/becla/qservDev/qserv-run/"
    log4jFPath = "%s/etc/log4cxx.log4j" % runDir
    contents = """
log4j.rootLogger=DEBUG, QLogger
log4j.appender.QLogger=org.apache.log4j.ConsoleAppender
log4j.appender.QLogger.layout=org.apache.log4j.PatternLayout
log4j.appender.QLogger.layout.ConversionPattern=NEWLOG! %d [%t] %-5p %c{2} %M (%F:%L) - %m%n
"""
    log4jF = open(log4jFPath, 'w')
    log4jF.write(contents)
    log4jF.close()
        #----- end of section that should be done in Fabrice's script
    newlog.configure(log4jFPath)

    if options.test == True:
        runParserTest()
        return
    elif options.testName:
        runNamedTest(options.testName)
        return
    elif options.sanityClient:
        client.runSanityClient()
        return
    elif options.makeIndex:
        makeIndexes()
        return
    else:
        server.runServer()
    return

if __name__ == '__main__':
    main()
