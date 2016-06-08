#!/usr/bin/env python

"""
Different methods used for creation of the snapshot and provision qserv

@author  Oualid Achbal, IN2P3

"""

# -------------------------------
#  Imports of standard modules --
# -------------------------------
import argparse
import ConfigParser
import logging
import os
import re
from subprocess import CalledProcessError, check_output
import sys
import time

# ----------------------------
# Imports for other modules --
# ----------------------------
from novaclient import client
import novaclient.exceptions

# -----------------------
# Exported definitions --
# -----------------------
class CloudManager(object):
    """Application class for common definitions of provision qserv and creation of image"""

    def __init__(self, go_for_snapshot=False, add_ssh_key=False):
        """
        Constructor parse all arguments

        @param add_ssh_key Add ssh key only while launching instances in provision qserv.
        @param go_for_snapshot find new_image until it's created
        """
        # define all command-line arguments
        parser = argparse.ArgumentParser(description='Single-node data loading script for Qserv.')

        parser.add_argument('-v', '--verbose', dest='verbose', default=[], action='append_const',
                            const=None, help='More verbose output, can use several times.')
        parser.add_argument('--verbose-all', dest='verboseAll', default=False, action='store_true',
                            help='Apply verbosity to all loggers, by default only loader level is set.')
        # parser = lsst.qserv.admin.logger.add_logfile_opt(parser)
        group = parser.add_argument_group('Cloud configuration options',
                                           'Options defining parameters to access remote cloud-platform')

        group.add_argument('-f', '--config', dest='configFile',
                            required=True, metavar='PATH',
                            help='Add cloud config file which contain instance characteristics')

        group.add_argument('-n', '--nb-servers', dest='nbServers',
                           required=False, default=3, type=int,
                           help='Choose the number of servers to boot')

        # parse all arguments
        self.args = parser.parse_args()

        logging.debug("Use configuration file: {}".format(self.args.configFile))

        config = ConfigParser.ConfigParser({'net-id': None,
                                            'ssh_security_group': None})

        try:
            config_file = open(self.args.configFile, 'r')
            try:
                config.readfp(config_file)
            finally:
                config_file.close()
        except IOError as exc:
            logging.fatal('Unable to read configuration file: {}'.format(exc))
            sys.exit(1)

        base_image_name = config.get('openstack', 'base_image_name')
        self.snapshot_name = config.get('openstack', 'snapshot_name')
        flavor_name = config.get('openstack', 'flavor_name')
        self.network_name = config.get('openstack', 'network_name')
        if config.get('openstack', 'net-id'):
            unicode_net_id = unicode(config.get('openstack', 'net-id'))
            self.nics = [{'net-id': unicode_net_id}]
        else:
            self.nics = []
        self.ssh_security_group = config.get('openstack', 'ssh_security_group')

        self._creds = {}
        self._set_nova_creds()
        self._safe_username = self._creds['username'].replace('.', '')
        self.nova = client.Client(**self._creds)

        # Upload ssh public key
        if add_ssh_key:
            self.key = "{}-qserv".format(self._safe_username)
        else:
            self.key = None

        self.key_filename = '~/.ssh/id_rsa'

        if go_for_snapshot:
            self.image = self.nova.images.find(name=self.snapshot_name)
        else:
            self.image = self.nova.images.find(name=base_image_name)

        self.flavor = self.nova.flavors.find(name=flavor_name)

    def get_nb_servers(self):
        """
        Get number of servers to boot
        """
        return self.args.nbServers

    def _set_nova_creds(self):
        """
        Extract the login information from the environment
        """
        self._creds['version'] = 2
        self._creds['username'] = os.environ['OS_USERNAME']
        self._creds['api_key'] = os.environ['OS_PASSWORD']
        self._creds['auth_url'] = os.environ['OS_AUTH_URL']
        self._creds['project_id'] = os.environ['OS_TENANT_NAME']
        self._creds['insecure'] = True
        logging.debug("Openstack user: {}".format(self._creds['username']))

    def nova_image_create(self, instance):
        """
        Create an openstack image containing Docker
        """
        logging.info("Creating Qserv image")
        qserv_image = instance.create_image(self.snapshot_name)
        status = None
        while status != 'ACTIVE':
            time.sleep(5)
            status = self.nova.images.get(qserv_image).status
        logging.info("SUCCESS: Qserv image '{}' created and active".format(self.snapshot_name))

    def nova_servers_create(self, instance_id, userdata):
        """
        Boot an instance and check status
        """
        instance_name = "{0}-qserv-{1}".format(self._safe_username, instance_id)
        logging.info("Launch an instance {}".format(instance_name))

        # Launch an instance from an image
        instance = self.nova.servers.create(name=instance_name,
                                            image=self.image,
                                            flavor=self.flavor,
                                            userdata=userdata,
                                            key_name=self.key,
                                            nics=self.nics)
        # Poll at 5 second intervals, until the status is no longer 'BUILD'
        status = instance.status
        while status == 'BUILD':
            time.sleep(5)
            instance.get()
            status = instance.status
        logging.info("status: {}".format(status))
        logging.info("Instance {} is active".format(instance_name))

        return instance

    def detect_end_cloud_config(self, instance):
        """
        Add clean wait for cloud-init completion
        """
        check_word = "---SYSTEM READY FOR SNAPSHOT---"
        end_word = None
        while not end_word:
            time.sleep(10)
            output = instance.get_console_output()
            logging.debug("console output: {}".format(output))
            logging.debug("instance: {}".format(instance))
            end_word = re.search(check_word, output)

    def nova_servers_delete(self, vm_name):
        """
        Retrieve an instance by name and shut it down
        """
        server = self.nova.servers.find(name=vm_name)
        #server.get()
        server.delete()

    def manage_ssh_key(self):
        """
        Upload ssh public key
        """
        logging.info('Manage ssh keys: {}'.format(self.key))
        if self.nova.keypairs.findall(name=self.key):
            logging.debug('Remove previous ssh keys')
            self.nova.keypairs.delete(key=self.key)

        with open(os.path.expanduser(self.key_filename + ".pub")) as fpubkey:
            self.nova.keypairs.create(name=self.key, public_key=fpubkey.read())

    def get_floating_ip(self):
        """
        Allocate floating ip to project
        """
        i = 0
        floating_ips = self.nova.floating_ips.list()
        floating_ip = None
        floating_ip_pool = self.nova.floating_ip_pools.list()[0].name

        # Check for available public ip in project
        while i < len(floating_ips) and floating_ip is None:
            if floating_ips[i].instance_id is None:
                floating_ip = floating_ips[i]
                logging.debug('Available floating ip found {}'.format(floating_ip))
            i += 1

        # Check for available public ip in ext-net pool
        if floating_ip is None:
            try:
                logging.debug("Use floating ip pool: {}".format(floating_ip_pool))
                floating_ip = self.nova.floating_ips.create(floating_ip_pool)
            except novaclient.exceptions.Forbidden as exc:
                logging.fatal("Unable to retrieve public IP: {0}".format(exc))
                sys.exit(1)

        return floating_ip

    def print_ssh_config(self, instances, floating_ip):
        """
        Print ssh client configuration to file
        """
        # ssh config
        ssh_config_tpl = '''
        Host {host}
        HostName {fixed_ip}
        User qserv
        Port 22
        StrictHostKeyChecking no
        UserKnownHostsFile /dev/null
        PasswordAuthentication no
        ProxyCommand ssh -i {key_filename} -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -W %h:%p qserv@{floating_ip}
        IdentityFile {key_filename}
        IdentitiesOnly yes
        LogLevel FATAL
        '''
        ssh_config_extract = ""
        for instance in instances:
            fixed_ip = instance.networks[self.network_name][0]
            ssh_config_extract += ssh_config_tpl.format(host=instance.name,
                                                        fixed_ip=fixed_ip,
                                                        floating_ip=floating_ip.ip,
                                                        key_filename=self.key_filename)
        logging.debug("Create SSH client config ")

        f = open("ssh_config", "w")
        f.write(ssh_config_extract)
        f.close()

    def check_ssh_up(self, instances):

        for instance in instances:
            cmd = ['ssh', '-t', '-F', './ssh_config', instance.name, 'true']
            success = False
            while not success:
                try:
                    check_output(cmd)
                    success = True
                except CalledProcessError as exc:
                    logging.warn("Waiting for ssh to be avalaible on {}: {}".format(instance.name, exc.output))
            logging.debug("ssh available on {}".format(instance.name))

    def update_etc_hosts(self, instances):
        """
        Update /etc/hosts on each machine
        """
        hostfile_tpl = "{ip}    {host}\n"

        hostfile = ""
        for instance in instances:
            # Collect IP adresses
            fixed_ip = instance.networks[self.network_name][0]
            hostfile += hostfile_tpl.format(host=instance.name, ip=fixed_ip)

        for instance in instances:
            cmd = ['ssh', '-t', '-F', './ssh_config', instance.name,
                   'sudo sh -c "echo \'{hostfile}\' >> /etc/hosts"'.format(hostfile=hostfile)]
            try:
                check_output(cmd)
            except CalledProcessError as exc:
                logging.error("ERROR while updating /etc/hosts: {}".format(exc.output))
                sys.exit(1)


