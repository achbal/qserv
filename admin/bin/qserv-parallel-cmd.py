#!/usr/bin/env python

# LSST Data Management System
# Copyright 2014 AURA/LSST.
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
User-friendly tool allowing to manage a set of Qserv nodes

Script is ed with
these input arguments:
  - TODO

Script performs these tasks:
  - TODO


@author  Fabrice Jammes, IN2P3

"""

# --------------------------------
#  Imports of standard modules  --
# --------------------------------
import sys
import argparse
import logging

# -----------------------------
# Imports for other modules  --
# -----------------------------
from lsst.qserv.admin.workerAdmin import WorkerAdmin
from lsst.qserv.admin.nodePool import NodePool
import lsst.qserv.admin.logger

# ----------------------------------
# Local non-exported definitions  --
# ----------------------------------
_LOG = logging.getLogger(__name__)


# ------------------------
# Exported definitions  --
# ------------------------
class ParallelCmd(object):
    """
    Application class for parallel command application
    """

    def __init__(self):
        """
        Constructor parse all arguments and prepares for execution.
        """

        # define all command-line arguments
        parser = argparse.ArgumentParser(description='Parallel command launcher for Qserv.')

        parser.add_argument('-v', '--verbose', dest='verbose', default=[],
                            action='append_const',
                            const=None,
                            help='More verbose output, can use several times.')
        parser = lsst.qserv.admin.logger.add_logfile_opt(parser)

        group = parser.add_argument_group('Nodes options',
                                          'Options related to Qserv machines')
        group.add_argument('-H', '--host', dest='hosts', default=[],
                           action='append', metavar='HOST',
                           help='DNS name for Qserv node, may be specified '
                           'more than once. If missing then --host-template '
                           'option is used')
        group.add_argument('-T', '--host-template', dest='host_tpl', default=None,
                           help='Template for node names, for example qserv{0}.mydomain.edu.'
                                ' Used only if --host isn\'t provided.')
        group.add_argument('-f', '--node-first', dest='node_first', default=0,
                           type=int,
                           help='First node number'
                           ' default: %(default)s.')
        group.add_argument('-l', '--node-last', dest='node_last', default=None,
                           type=int,
                           help='Last node number'
                           ' default: %(default)s.')
        group.add_argument('-u', '--user', dest='user', default=None,
                           help='User name to use when connecting to server.')
        group.add_argument('-P', '--port', dest='sshPort', default=22,
                           metavar='PORT_NUMBER', type=int,
                           help='Port number to use for ssh connection,'
                           ' default: %(default)s.')
        group.add_argument('-k', '--kerberos', dest='kerberos', action='store_true',
                           default=False,
                           help='Authentication on Qserv nodes is performed using kerberos.')

        group = parser.add_argument_group('SSH command options',
                                          'Options related to Qserv machines')
        group.add_argument('command',
                           help='ssh command to launch on all nodes')
        group.add_argument('-R', '--run-dir', dest='run_dir',
                           default='/bin',
                           help='absolute path to the directory where the command is runned'
                           )
        group.add_argument('-s', '--sudo-user', dest='sudo_user',
                           default=None,
                           help='sudo to this user account before launching command'
                           )
        group.add_argument('-p', '--sudo-password', dest='sudo_password',
                           default=None,
                           help='sudo-user password, WARNING: insecure, prefer'
                           'passwordless sudo'
                           )

        # parse all arguments
        self.args = parser.parse_args()

        verbosity = len(self.args.verbose)
        if verbosity != 0:
            levels = {0: logging.WARNING, 1: logging.INFO, 2: logging.DEBUG}
            simple_format = "[%(levelname)s] %(name)s: %(message)s"
            level = levels.get(verbosity, logging.DEBUG)
            logging.basicConfig(format=simple_format, level=level)
        else:
            # if -v(vv) option isn't used then
            # switch to global configuration file for logging
            lsst.qserv.admin.logger.setup_logging(self.args.log_conf)

        # instantiate nodes
        if self.args.hosts:
            hosts = self.args.hosts
        else:
            hosts = [self.args.host_tpl.format(n)
                     for n in range(self.args.node_first,
                                    self.args.node_last)]

        nodes = [WorkerAdmin(host=h,
                             runDir=self.args.run_dir,
                             kerberos=self.args.kerberos,
                             ssh_user=self.args.user)
                 for h in hosts]

        self.nodePool = NodePool(nodes)

    def run(self):
        """
        Run a command on a set of nodes  based on parameters defined in
        constructor. This will throw exception if anything goes wrong.
        : TODO check if exception is effectively launched
        """
        params = {'command': self.args.command}
        cmd_tpl='{command}'
        if self.args.sudo_user:
            params['sudo_user'] = self.args.sudo_user
            params['stdin_opt'] = ''
            cmd_tpl = '/bin/sudo {stdin_opt} su {sudo_user} -c "{command}"'
            if self.args.sudo_password:
                params['stdin_opt'] = '-kS'
                params['sudo_password'] = self.args.sudo_password
                cmd_tpl = "/bin/echo '{sudo_password}' | " + cmd_tpl

        cmd = cmd_tpl.format(**params)
        self.nodePool.execParallel(cmd)
        _LOG.info("Run successfully command: '%s'", cmd)
        return 0


if __name__ == "__main__":
    try:
        parallelCmd = ParallelCmd()
        r = parallelCmd.run()
        sys.exit(r)
    except Exception as exc:
        _LOG.critical('Exception occured: %s', exc, exc_info=True)
        sys.exit(1)
