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


def nova_image_create():
    """
    Create an openstack image containing Docker 
    """
    username = creds['username'].replace('.', '')
    instance_name = "{0}-qserv".format(username)
    logging.info("Launch an instance {}".format(instance_name))

    # cloud config
    userdata = '''
    #cloud-config

    groups:
    - docker

    packages:
    - docker

    package_upgrade: true
    package_reboot_if_required: true
    timezone: Europe/Paris
    '''

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

    # TODO: add clean wait for cloud-init completion
    time.sleep(360)
    instance.create_image("centos-7-qserv")
    instance.delete()


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

        nova_image_create()

        logging.debug("SUCCESS: Qserv image created")
    except Exception as exc:
        logging.critical('Exception occured: %s', exc, exc_info=True)
        sys.exit(3)
