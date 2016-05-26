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

def nova_servers_create(instance_id,userdata):
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
    checkConfig = "---SYSTEM READY FOR SNAPSHOT---"
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
        creds = get_nova_creds()
        nova = client.Client(**creds)



    except Exception as exc:
        logging.critical('Exception occured: %s', exc, exc_info=True)
        sys.exit(3)
