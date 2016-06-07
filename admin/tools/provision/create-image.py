#!/usr/bin/env python

"""
Create an image by taking a snapshot from an instance, and use cloud config to install docker on VM

Script performs these tasks:
  - launch instances from image
  - install docker via cloud-init
  - create a snapshot
  - shut down and delete the instance created

@author  Oualid Achbal, IN2P3

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
        # Configure logging
        logging.basicConfig(format='%(asctime)s %(levelname)-8s %(name)-15s'
                                   ' %(message)s',
                            level=logging.DEBUG)
        # Disable requests and urllib3 package logger and warnings
        logging.getLogger("requests").setLevel(logging.ERROR)
        logging.getLogger("urllib3").setLevel(logging.ERROR)
        warnings.filterwarnings("ignore")

        cloudManager = cloudmanager.CloudManager(add_ssh_key=False)

        userdata = get_cloudconfig()

        instance_id = "source"
        instance = cloudManager.nova_servers_create(instance_id, userdata)

        # Wait for cloud config completion
        cloudManager.detect_end_cloud_config(instance)

        _image_name = "centos-7-qserv"
        qserv_image = cloudManager.nova_image_create(instance, _image_name)

        # Delete instance after taking a snapshot
        cloudManager.nova_servers_delete(instance.name)

    except Exception as exc:
        logging.critical('Exception occured: %s', exc, exc_info=True)
        sys.exit(1)
