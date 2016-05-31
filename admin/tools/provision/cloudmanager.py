#!/usr/bin/env python

"""

@author  Oualid Achbal, ISIMA student , IN2P3

"""

# -------------------------------
#  Imports of standard modules --
# -------------------------------
from subprocess import CalledProcessError, check_output
import logging
import os
import re
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
    """A simple example class"""

    def __init__(self, image_name, flavor_name, network_name, nics, security_groups=None, add_ssh_key=False):
        """
        Constructor
        """
        self.network_name = network_name
        self.nics = nics
        self.ssh_security_group = security_groups

        self._creds = self.get_nova_creds()
        self.nova = client.Client(**self._creds)

        if add_ssh_key:
            # Upload ssh public key
            self.key = "{}-qserv".format(self._creds['safe_username'])
            # Remove unsafe characters
            #self.key = self.key.replace('.', '')
        else:
            self.key = None

        self.key_filename = '~/.ssh/id_rsa'
        self.image = self.nova.images.find(name=image_name)
        self.flavor = self.nova.flavors.find(name=flavor_name)


    def get_nova_creds(self):
        """
        Extract the login information from the environment
        """
        creds = {}
        creds['version'] = 2
        creds['username'] = os.environ['OS_USERNAME']
        creds['api_key'] = os.environ['OS_PASSWORD']
        creds['auth_url'] = os.environ['OS_AUTH_URL']
        creds['project_id'] = os.environ['OS_TENANT_NAME']
        creds['insecure'] = True
        creds['safe_username'] = creds['username'].replace('.', '')
        logging.debug("Openstack user: {}".format(creds['username']))

        return creds

    def nova_image_create(self, instance, _image_name):
        """
        Create an openstack image containing Docker
        """
        logging.info("Creating Qserv image")
        qserv_image = instance.create_image(_image_name)
        status = self.nova.images.get(qserv_image).status
        while status != 'ACTIVE':
            time.sleep(5)
            status = self.nova.images.get(qserv_image).status
        logging.debug("SUCCESS: Qserv image created")
        logging.info("status: {}".format(status))
        logging.info("Image {} is active".format(_image_name))

        return qserv_image

    def nova_servers_create(self, instance_id, userdata):
        """
        Boot an instance and check status
        """
        instance_name = "{}-qserv-{:02d}".format(self._creds['safe_username'], instance_id)
        logging.info("Launch an instance {}".format(instance_name))

        # Launch an instance from an image
        instance = self.nova.servers.create(name=instance_name, image=self.image,
                                            flavor=self.flavor, userdata=userdata, key_name=self.key, nics=self.nics)
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
        checkConfig = "---SYSTEM READY FOR SNAPSHOT---"
        is_finished = False
        while not is_finished:
            time.sleep(15)
            output = instance.get_console_output()
            #logging.debug("output: {}".format(output))
            word = re.search(checkConfig, output)
            #logging.debug("word: {}".format(word))
            #logging.debug("----------------------------")
            if word != None:
                is_finished = True

    def nova_servers_delete(self, vm_name):
        """
        Retrieve an instance by name and shut it down
        """
        server = self.nova.servers.find(name=vm_name)
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
            except novaclient.exceptions.Forbidden as e:
                logging.fatal("Unable to retrieve public IP: {0}".format(e))
                sys.exit(4)

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

        logging.debug("SSH client config: ")

        f = open("ssh_config", "w")
        f.write(ssh_config_extract)
        f.close()

    def update_etc_hosts(self,instances):
        """
        Modify /etc/hosts on each machine
        """
        hostfile_tpl = "{ip}    {host}\n"

        hostfile = ""
        for instance in instances:
            # Collect IP adresses
            fixed_ip = instance.networks[self.network_name][0]
            hostfile += hostfile_tpl.format(host=instance.name, ip=fixed_ip)

        # logging.debug("hostfile.txt:\n---\n{}\n---".format(hostfile))

        # Update /etc/hosts on each machine
        for instance in instances:
            cmd = ['ssh', '-t', '-F', './ssh_config', instance.name,
                   'sudo sh -c "echo \'{hostfile}\' >> /etc/hosts"'.format(hostfile=hostfile)]
            try:
                check_output(cmd)
            except CalledProcessError as exc:
                logging.error("ERROR while updating /etc/hosts: {}".format(exc.output))
                sys.exit(5)


