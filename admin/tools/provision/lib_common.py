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
import time

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
    creds = get_nova_creds()
    nova = client.Client(**creds)
    server = nova.servers.find(name=vm_name)
    server.delete()

