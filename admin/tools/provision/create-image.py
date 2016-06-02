#!/usr/bin/env python

"""
Create an image by taking a snapshot from an instance, and use cloud config to install docker on VM

Script performs these tasks:
  - launch instances from image
  - install docker via cloud-init
  - create a snapshot
  - shut down and delete the instance created

@author  Oualid Achbal, ISIMA student , IN2P3

"""

# -------------------------------
#  Imports of standard modules --
# -------------------------------
import logging
import sys
import warnings

# ----------------------------
# Imports for other modules --
# ----------------------------
import cloudmanager

# -----------------------
# Exported definitions --
# -----------------------
def get_cloudconfig():
    """
    Write cloud init file
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

        package_upgrade: true
        package_reboot_if_required: true
        timezone: Europe/Paris
        '''
    return userdata


if __name__ == "__main__":
    try:
        logging.basicConfig(format='%(asctime)s %(levelname)-8s %(name)-15s %(message)s',level=logging.DEBUG)
        # Disable request package logger and warnings
        logging.getLogger("requests").setLevel(logging.ERROR)
        warnings.filterwarnings("ignore")

        # CC-IN2P3
        # image_name = "CentOS-7-x86_64-GenericCloud"
        # flavor_name = "m1.medium"
        # self.network_name = "lsst"
        # self.nics = []

        # Petasky
        # image_name = "CentOS 7"
        # flavor_name = "c1.medium"
        # self.network_name = "petasky-net"
        # self.nics = []

        # NCSA
        image_name = "centos7_updated_systemd"
        _image_name = "centos-7-qserv"
        flavor_name = "m1.medium"
        network_name = "LSST-net"
        nics = [{'net-id': u'fc77a88d-a9fb-47bb-a65d-39d1be7a7174'}]

        cloudManager = cloudmanager.CloudManager(add_ssh_key=False)

        userdata = get_cloudconfig()

        instance_id = "source"
        instance = cloudManager.nova_servers_create(instance_id, userdata)

        # Wait for cloud config completion
        cloudManager.detect_end_cloud_config(instance)

        qserv_image = cloudManager.nova_image_create(instance, _image_name)

        # Delete instance after taking a snapshot
        cloudManager.nova_servers_delete(instance.name)

    except Exception as exc:
        logging.critical('Exception occured: %s', exc, exc_info=True)
        sys.exit(1)
