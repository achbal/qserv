#!/usr/bin/env python

"""
Create a snapshot from an instance, and use cloud config to install packages on virtual machines

Script performs these tasks:
- launch instances from image
- create a snapshot
- use cloud config
- shut down the instance created


@author  Oualid Achbal, ISIMA student , IN2P3

"""

# -------------------------------
#  Imports of standard modules --
# -------------------------------
import logging
import sys
import time
import warnings

# ----------------------------
# Imports for other modules --
# ----------------------------
from novaclient import client
import lib_common

# -----------------------
# Exported definitions --
# -----------------------
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
    """
    cloud init
    """
    userdata = '''
        #cloud-config
        groups:
        - docker

        packages:
        - docker
        - util-linux

        runcmd:
        - ['systemctl', 'enable', 'docker']
        - ['/tmp/detect_end_cloud_config.sh']

        write_files:
        -   path: "/tmp/detect_end_cloud_config.sh"
            permissions: "0544"
            owner: "root"
            content: |
              #!/bin/sh
              (while [ ! -f /var/lib/cloud/instance/boot-finished ] ;
              do
                sleep 2
                echo "---CLOUD-INIT-DETECT RUNNING---"
              done
              sync
              fsfreeze -f / && read x; fsfreeze -u /
              echo "---SYSTEM READY FOR SNAPSHOT---") &

        #package_upgrade: true
        #package_reboot_if_required: true
        #timezone: Europe/Paris
        '''
    return userdata

def nova_image_create():
    """
    Create an openstack image containing Docker
    """
    _image_name = "centos-7-qserv"
    logging.info("Creating Qserv image")
    qserv_image = instance.create_image(_image_name)
    status = nova.images.get(qserv_image).status
    while status != 'ACTIVE':
        time.sleep(5)
        status = nova.images.get(qserv_image).status
    logging.info ("status: {}".format(status))
    logging.debug("SUCCESS: Qserv image created")
    logging.info ("Image {} is active".format(_image_name))

    return qserv_image


if __name__ == "__main__":
    try:
        logging.basicConfig(format='%(asctime)s %(levelname)-8s %(name)-15s %(message)s',level=logging.DEBUG)

        # Disable request package logger
        logging.getLogger("requests").setLevel(logging.ERROR)

        # Disable warnings
        warnings.filterwarnings("ignore")

        creds = lib_common.get_nova_creds()
        nova = client.Client(**creds)


        # Write cloud init file
        userdata = cloud_config()

        # Add cloud fixes

        # CC-IN2P3
        # image_name = "CentOS-7-x86_64-GenericCloud"
        # flavor_name = "m1.medium"
        # network_name = "lsst"

        # Petasky
        # image_name = "CentOS 7"
        # flavor_name = "c1.medium"
        # network_name = "petasky-net"

        # NCSA
        image_name = "centos7_updated_systemd"
        flavor_name = "m1.medium"
        network_name = "LSST-net"
        nics = []
        nics = [ { 'net-id': u'fc77a88d-a9fb-47bb-a65d-39d1be7a7174' } ]

        # Find an image and a flavor to launch an instance
        image = nova.images.find(name=image_name)
        flavor = nova.flavors.find(name=flavor_name)

        # Launch instance from image
        instance = nova_servers_create()

        # Wait for cloud config completion
        lib_common.detect_end_cloud_config(instance)

        # Take a snapshot
        qserv_image = nova_image_create()

        # Delete instance after taking a snapshot
        lib_common.nova_servers_delete(instance.name)
    except Exception as exc:
        logging.critical('Exception occured: %s', exc, exc_info=True)
        sys.exit(3)
