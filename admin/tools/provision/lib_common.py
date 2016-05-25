#!/usr/bin/env python

"""

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

def detect_end_cloud_config(instance):
    # Add clean wait for cloud-init completion
    checkConfig = "Started Execute cloud user/final scripts."
    is_finished = False
    while not is_finished:
        time.sleep(15)
        output = instance.get_console_output()
        logging.debug("output: {}".format(output))
        word = re.search(checkConfig, output)
        logging.debug("word: {}".format(word))
        print "----------------------------"
        if word != None:
            is_finished = True

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

        key_filename = '~/.ssh/id_rsa'

        # Upload ssh public key
        key = "{}-qserv".format(creds['username'])
        # Remove unsafe characters
        key = key.replace('.', '')
        fpubkey = open(os.path.expanduser(key_filename+".pub"))
        public_key=fpubkey.read()
        # Find an image and a flavor to launch an instance

        nics = []
        image_name = "centos-7-qserv"

        # NCSA
        flavor_name = "m1.medium"
        network_name = "LSST-net"
        nics = [ { 'net-id': u'fc77a88d-a9fb-47bb-a65d-39d1be7a7174' } ]
        ssh_security_group = "Remote SSH"

        image = nova.images.find(name=image_name)
        flavor = nova.flavors.find(name=flavor_name)
        userdata = cloud_config()

    except Exception as exc:
        logging.critical('Exception occured: %s', exc, exc_info=True)
        sys.exit(3)
