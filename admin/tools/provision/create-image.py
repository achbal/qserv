#!/usr/bin/env python

"""
TODO

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

# -----------------------
# Exported definitions --
# -----------------------
def get_nova_creds():
    """
    Extract the login information from the environment
    """
    d = {}
    d['version'] = 2
    d['username'] = os.environ['OS_USERNAME']
    d['api_key'] = os.environ['OS_PASSWORD']
    d['auth_url'] = os.environ['OS_AUTH_URL']
    d['project_id'] = os.environ['OS_TENANT_NAME']
    d['insecure'] = True
    logging.debug("Openstack user: {}".format(d['username']))
    return d


def nova_servers_create():
    """
    Create an openstack image containing Docker
    """
    username = creds['username'].replace('.', '')
    instance_name = "{0}-qserv".format(username)
    logging.info("Launch an instance {}".format(instance_name))

    # Launch an instance from an image
    instance = nova.servers.create(name=instance_name, image=image,
            flavor=flavor, userdata=userdata, nics=nics)
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
    # cloud config
    userdata = '''
        #cloud-config
        groups:
        - docker

        #packages:
        - docker
        - util-linux

        runcmd:
        - ['systemctl', 'enable', 'docker']
        #- ['/tmp/detect_end_cloud_config.sh','&']

        write_files:
        -   path: "/tmp/detect_end_cloud_config.sh"
            permissions: "0544"
            owner: "root"
            content: |
              #!/bin/sh
              while [ ! -f /var/lib/cloud/instance/boot-finished ] ;
              do
                sleep 2
                echo "CLOUD-INIT-DETECT RUNNING"
              done
              sync
              fsfreeze -f / && read x; fsfreeze -u /
              echo "CLOUD-INIT-END"


        # Currently broken
        # package_upgrade: true
        #package_reboot_if_required: true
        #timezone: Europe/Paris
        '''
    return userdata

def detect_end_cloud_config():
    # Add clean wait for cloud-init completion
    checkConfig = "Cloud-init v. 0.7.5 finished at"
    #checkConfig = "CLOUD-INIT-END"
    has_finished_flag = None
    while not has_finished_flag:
        time.sleep(15)
        output = instance.get_console_output()
        logging.debug("output: {}".format(output))
        has_finished_flag = re.search(checkConfig, output)
        logging.debug("has_finished_flag: {}".format(has_finished_flag))
        print "----------------------------"

    logging.info("cloud config Success")

def nova_image_create():
    _image_name = "centos-7-qserv"
    instance.create_image(_image_name)
    logging.debug("SUCCESS: Qserv image created")

def nova_servers_delete(vm_name):
    """
    Retrieve an instance by name and shut it down
    """
    server = nova.servers.find(name=vm_name)
    server.delete()


if __name__ == "__main__":
    try:
        logging.basicConfig(format='%(asctime)s %(levelname)-8s %(name)-15s %(message)s',level=logging.DEBUG)

        # Disable request package logger
        logging.getLogger("requests").setLevel(logging.ERROR)

        # Disable warnings
        warnings.filterwarnings("ignore")

        creds = get_nova_creds()
        nova = client.Client(**creds)

        nics = []

        # CC-IN2P3
        # image_name = "CentOS-7-x86_64-GenericCloud"
        # flavor_name = "m1.medium"
        # network_name = "lsst"

        # Petasky
        # image_name = "CentOS 7"
        # flavor_name = "c1.medium"
        # network_name = "petasky-net"

        # NCSA
        image_name = "CentOS 7"
        flavor_name = "m1.medium"
        network_name = "LSST-net"
        nics = [ { 'net-id': u'fc77a88d-a9fb-47bb-a65d-39d1be7a7174' } ]
        ssh_security_group = "Remote SSH"

        image = nova.images.find(name=image_name)
        flavor = nova.flavors.find(name=flavor_name)

        userdata = cloud_config()
        instance = nova_servers_create()

        detect_end_cloud_config()
        time.sleep(20)

        nova_image_create()

        # TODO wait for image creation
        # nova_servers_delete(instance.name)
    except Exception as exc:
        logging.critical('Exception occured: %s', exc, exc_info=True)
        sys.exit(3)
