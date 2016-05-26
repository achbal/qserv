#!/usr/bin/env python

"""
Boot instances from an image already created in cluster infrastructure, and use cloud config to create users
and install packages on virtual machines

Script performs these tasks:
- launch instances from image and manage ssh key
- create gateway vm
- check for available floating ip adress
- add it to gateway
- cloud config

@author  Oualid Achbal, ISIMA student , IN2P3

"""

# -------------------------------
#  Imports of standard modules --
# -------------------------------
import logging
import os
import re
import subprocess
import sys
import time
import warnings

# ----------------------------
# Imports for other modules --
# ----------------------------
from novaclient import client
import novaclient.exceptions
import lib_common

# -----------------------
# Exported definitions --
# -----------------------
def manage_ssh_key():
    """
    Upload ssh public key
    """
    logging.info('Manage ssh keys: {}'.format(key))
    if nova.keypairs.findall(name=key):
        logging.debug('Remove previous ssh keys')
        nova.keypairs.delete(key=key)

    with fpubkey:
        nova.keypairs.create(name=key, public_key=public_key)

def nova_servers_create(instance_id):
    """
    Boot an instance from an image and check status
    """
    username = creds['username'].replace('.', '')
    instance_name = "{0}-qserv-{1}".format(username, instance_id)
    logging.info("Launch an instance {}".format(instance_name))

    # Launch an instance from an image
    instance = nova.servers.create(name=instance_name, image=image,
            flavor=flavor, userdata=userdata, key_name=key, nics=nics)
    # Poll at 5 second intervals, until the status is no longer 'BUILD'
    status = instance.status
    while status == 'BUILD':
        time.sleep(5)
        instance.get()
        status = instance.status
    logging.info ("status: {}".format(status))
    logging.info ("Instance {} is active".format(instance_name))

    return instance

def cloud_config():
    """
    cloud config
    """
    cloud_config_tpl = '''
        #cloud-config
        users:
        - name: qserv
          gecos: Qserv daemon
          groups: docker
          lock-passwd: true
          shell: /bin/bash
          ssh-authorized-keys:
          - {key}
          sudo: ALL=(ALL) NOPASSWD:ALL

        runcmd:
        - ['/tmp/detect_end_cloud_config.sh']
        '''

    userdata = cloud_config_tpl.format(key=public_key)

    return userdata


def get_floating_ip():
    """
    Allocate floating ip to project
    """
    i=0
    floating_ips = nova.floating_ips.list()
    floating_ip = None
    floating_ip_pool = nova.floating_ip_pools.list()[0].name

    # Check for available public ip in project
    while i<len(floating_ips) and floating_ip is None:
        if floating_ips[i].instance_id is None:
            floating_ip=floating_ips[i]
            logging.debug('Available floating ip found {}'.format(floating_ip))
        i+=1

    # Check for available public ip in ext-net pool
    if floating_ip is None:
        try:
            logging.debug("Use floating ip pool: {}".format(floating_ip_pool))
            floating_ip = nova.floating_ips.create(floating_ip_pool)
        except novaclient.exceptions.Forbidden as e:
            logging.fatal("Unable to retrieve public IP: {0}".format(e))
            sys.exit(1)

    return floating_ip

def print_ssh_config(instances, floating_ip):
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
        fixed_ip = instance.networks[network_name][0]
        ssh_config_extract += ssh_config_tpl.format(host=instance.name,
                                                    fixed_ip=fixed_ip,
                                                    floating_ip=floating_ip.ip,
                                                    key_filename=key_filename)

    logging.debug("SSH client config: ")

    f = open("ssh_config", "w")
    f.write(ssh_config_extract)
    f.close()

def update_etc_hosts():

    hostfile_tpl = "{ip}    {host}\n"

    hostfile=""
    for instance in instances:
        # Collect IP adresses
        fixed_ip = instance.networks[network_name][0]
        hostfile += hostfile_tpl.format(host=instance.name, ip=fixed_ip)

    #logging.debug("hostfile.txt:\n---\n{}\n---".format(hostfile))

    # Update /etc/hosts on each machine
    for instance in instances:
        cmd=['ssh', '-t', '-F', './ssh_config', instance.name,
             'sudo sh -c "echo \'{hostfile}\' >> /etc/hosts"'.format(hostfile=hostfile)]
        #logging.debug("cmd:\n---\n{}\n---".format(cmd))
        subprocess.check_output(cmd)


if __name__ == "__main__":
    try:
        logging.basicConfig(format='%(asctime)s %(levelname)-8s %(name)-15s %(message)s',level=logging.DEBUG)

        # Disable request package logger
        logging.getLogger("requests").setLevel(logging.ERROR)

        # Disable warnings
        warnings.filterwarnings("ignore")

        creds = lib_common.get_nova_creds()
        nova = client.Client(**creds)

        key_filename = '~/.ssh/id_rsa'

        # Upload ssh public key
        key = "{}-qserv".format(creds['username'])
        # Remove unsafe characters
        key = key.replace('.', '')
        fpubkey = open(os.path.expanduser(key_filename+".pub"))
        public_key=fpubkey.read()
        manage_ssh_key()

        # Find a floating ip for gateway
        floating_ip = get_floating_ip()
        if not floating_ip:
            logging.fatal("Unable to add public ip to Qserv gateway")
            sys.exit(2)

        # Find an image and a flavor to launch an instance

        nics = []
        image_name = "centos-7-qserv"

        # CC-IN2P3
        # image_name = "CentOS-7-x86_64-GenericCloud"
        # flavor_name = "m1.medium"
        # network_name = "lsst"

        # Petasky
        # image_name = "CentOS 7"
        # flavor_name = "c1.medium"
        # network_name = "petasky-net"

        # NCSA
        flavor_name = "m1.medium"
        network_name = "LSST-net"
        nics = [ { 'net-id': u'fc77a88d-a9fb-47bb-a65d-39d1be7a7174' } ]
        ssh_security_group = "Remote SSH"

        image = nova.images.find(name=image_name)
        flavor = nova.flavors.find(name=flavor_name)
        userdata = cloud_config()

        # Create instances list
        instances = []

        # Create gateway instance and add floating_ip to it
        gateway_id = 0
        gateway_instance = nova_servers_create(gateway_id)
        logging.info("Add floating ip ({0}) to {1}".format(floating_ip,
            gateway_instance.name))
        gateway_instance.add_floating_ip(floating_ip)

        if ssh_security_group:
            gateway_instance.add_security_group(ssh_security_group)

        # Add gateway to instances list
        instances.append(gateway_instance)

        # Create worker instances
        for instance_id in range(1,3):
            worker_instance = nova_servers_create(instance_id)
            # Add workers to instances list
            instances.append(worker_instance)

        # Show ssh client config
        print_ssh_config(instances, floating_ip)

        # Modify /etc/hosts on each machine
        lib_common.detect_end_cloud_config(worker_instance)
        #time.sleep(40)
        update_etc_hosts()

        #for instance in instances:
        #    lib_common.nova_servers_delete(instance.name)

        logging.debug("SUCCESS: Qserv Openstack cluster is up")
    except Exception as exc:
        logging.critical('Exception occured: %s', exc, exc_info=True)
        sys.exit(3)
