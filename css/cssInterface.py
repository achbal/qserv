#!/usr/bin/env python

# LSST Data Management System
# Copyright 2013 LSST Corporation.
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


"""
This module implements interface to the Central State System (CSS).

@author  Jacek Becla, SLAC


Known issues and todos:
 - recover from lost connection by reconnecting
 - need to catch all exceptions that ZooKeeper and Kazoo might raise, see:
   http://kazoo.readthedocs.org/en/latest/api/client.html
 - issue: watcher is currently using the "_zk", and bypasses the official API!

"""

import time

from kazoo.client import KazooClient
from kazoo.exceptions import NodeExistsError, NoNodeError

import logging

class CssException(Exception):
    """
    Exception raised by CSSInterface.
    """

    ERR_DB_EXISTS               = 2001
    ERR_DB_DOES_NOT_EXIST       = 2005
    ERR_INVALID_CONNECTION      = 2007
    ERR_KEY_EXISTS              = 2010
    ERR_KEY_DOES_NOT_EXIST      = 2015
    ERR_KEY_INVALID             = 2020
    ERR_TB_EXISTS               = 2025
    ERR_TB_DOES_NOT_EXIST       = 2030
    ERR_NOT_IMPLEMENTED         = 9998
    ERR_INTERNAL                = 9999

    def __init__(self, errNo, extraMsgList=None):
        """
        Initialize the shared data.

        @param errNo      Error number.
        @param extraMsgList  Optional list of extra messages.
        """
        self._errNo = errNo
        self._extraMsgList = extraMsgList

        self._errors = { 
            CssException.ERR_DB_EXISTS: "Database already exists.",
            CssException.ERR_DB_DOES_NOT_EXIST: "Database does not exist.",
            CssException.ERR_INVALID_CONNECTION: "Invalid connection information.",
            CssException.ERR_KEY_EXISTS: "Key already exists.",
            CssException.ERR_KEY_INVALID: "Invalid key.",
            CssException.ERR_KEY_DOES_NOT_EXIST: "Key does not exist.",
            CssException.ERR_TB_EXISTS: "Table already exists.",
            CssException.ERR_TB_DOES_NOT_EXIST: "Table does not exist.",
            CssException.ERR_NOT_IMPLEMENTED: "Fature not implemented yet.",
            CssException.ERR_INTERNAL: "Internal error."
        }

    def __str__(self):
        """
        Return string representation of the error.

        @return string  Error message string, including all optional messages.
        """
        msg = self._errors.get(self._errNo, 
                               "Unrecognized css error: %d." % self._errNo)
        if self._extraMsgList is not None: 
            for s in self._extraMsgList: msg += " (%s)" % s
        return msg

    def getErrNo(self):
        """
        Return error number.
        """
        return self._errNo

####################################################################################
class CssInterface(object):
    """
    @brief CssInterface class defines interface to the Central State Service CSS).

    @param connInfo  Connection information.
    """

    def __init__(self, connInfo):
        """
        Initialize the interface.
        """
        self._logger = logging.getLogger("CSS")
        if connInfo is None:
            raise CssException(CssException.ERR_INVALID_CONNECTION, ["<None>"])
        self._logger.info("conn is: %s" % connInfo)
        self._zk = KazooClient(hosts=connInfo)
        self._zk.start()

    def create(self, k, v='', sequence=False):
        """
        Add a new key/value entry. Create entire path as necessary. 

        @param sequence  Sequence flag -- if set to True, a 10-digid, 0-padded
                         suffix (unique sequential number) will be added to the key.

        @return string   Real path to the just created node.

        @raise     CssException if the key k already exists.
        """
        self._logger.info("CREATE '%s' --> '%s'" % (k, v))
        try:
            return self._zk.create(k, v, sequence=sequence, makepath=True)
        except NodeExistsError:
            self._logger.error("in create(), key %s exists" % k)
            raise CssException(CssException.ERR_KEY_EXISTS, [k])

    def exists(self, k):
        """
        Check if a given key exists.

        @param k Key.

        @return boolean  True if the key exists, False otherwise.
        """
        return self._zk.exists(k)

    def get(self, k):
        """
        Return value for a key.

        @param k   Key.

        @return string  Value for a given key. 

        @raise     Raise CssException if the key doesn't exist.
        """
        try:
            v, stat = self._zk.get(k)
            self._logger.info("GET '%s' --> '%s'" % (k, v))
            return v
        except NoNodeError:
            self._logger.error("in get(), key %s does not exist" % k)
            raise CssException(CssException.ERR_KEY_DOES_NOT_EXIST, [k])

    def getChildren(self, k):
        """
        Return the list of the children of the node k.

        @param k   Key.

        @return    List_of_children of the node k. 

        @raise     Raise CssException if the key does not exists.
        """
        try:
            self._logger.info("GETCHILDREN '%s'" % (k))
            return self._zk.get_children(k)
        except NoNodeError:
            self._logger.error("in getChildren(), key %s does not exist" % k)
            raise CssException(CssException.ERR_KEY_DOES_NOT_EXIST, [k])

    def set(self, k, v):
        """
        Set value for a given key. Raise exception if the key doesn't exist.

        @param k  Key.
        @param v  Value.

        @raise     Raise CssException if the key doesn't exist.
        """
        try:
            self._logger.info("SET '%s' --> '%s'" % (k, v))
            self._zk.set(k, v)
        except NoNodeError:
            self._logger.error("in set(), key %s does not exist" % k)
            raise CssException(CssException.ERR_KEY_DOES_NOT_EXIST, [k])

    def delete(self, k, recursive=False):
        """
        Delete a key, including all children if recursive flag is set.

        @param k         Key.
        @param recursive Flag. If set, all existing children nodes will be
                         deleted.

        @raise     Raise CssException if the key doesn't exist.
        """
        try:
            self._logger.info("DELETE '%s'" % (k))
            self._zk.delete(k, recursive=recursive)
        except NoNodeError:
            self._logger.error("in delete(), key %s does not exist" % k)
            raise CssException(CssException.ERR_KEY_DOES_NOT_EXIST, [k])

    def deleteAll(self, p):
        """
        Delete everything recursively starting from a given point in the tree.
        This can be used to wipe out everything. It is too dangerous to expose
        to users, it'll be well hidden, or disabled when we move to production.

        @param p  Path.

        Raise exception if the key doesn't exist.
        """
        if self._zk.exists(p):
            self._deleteOne(p)

    def printAll(self):
        """
        Print entire contents to stdout. This is handy for debugging.
        """
        self._printOne("/")

    def _printOne(self, p):
        """
        Print content of one key/value to stdout. Note, this function is recursive.

        @param p  Path.
        """
        children = None
        data = None
        stat = None
        try:
            children = self._zk.get_children(p)
            data, stat = self._zk.get(p)
            print p, "=", data
            for child in children:
                if p == "/":
                    if child != "zookeeper":
                        self._printOne("%s%s" % (p, child))
                else:
                    self._printOne("%s/%s" % (p, child))
        except NoNodeError:
            self._logger.warning("Caught NoNodeError, someone deleted node just now")
            None

    def _deleteOne(self, p):
        """
        Delete one znode. Note, this function is recursive.

        @param p  Path.
        """
        try:
            children = self._zk.get_children(p)
            for child in children:
                if p == "/":
                    if child != "zookeeper": # skip "/zookeeper"
                        self._deleteOne("%s%s" % (p, child))
                else:
                    self._deleteOne("%s/%s" % (p, child))
            if p != "/": 
                self._zk.delete(p)
        except NoNodeError:
            None
