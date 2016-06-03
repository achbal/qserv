#!/usr/bin/env python

"""
Boot instances from an image already created in cluster infrastructure, and use cloud config to create users
on virtual machines

Script performs these tasks:
  - launch instances from image and manage ssh key
  - create gateway vm
  - check for available floating ip adress
  - add it to gateway
  - create users via cloud-init
  - update /etc/hosts on each VM
  - print ssh client config

@author  Oualid Achbal, ISIMA student , IN2P3

"""

# -------------------------------
#  Imports of standard modules --
# -------------------------------
import logging
import os
import sys
import warnings

# ----------------------------
# Imports for other modules --
# ----------------------------
import cloudmanager

NB_SERVER = 5

# -----------------------
# Exported definitions --
# -----------------------
def get_cloudconfig():
    """
    Write cloud init file
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
        - [mkdir, -p, '/qserv/data']
        - [mkdir, -p, '/qserv/log']
        - [chown, "1000:1000", '/qserv/data']
        - [chown, "1000:1000", '/qserv/log']
        - [sed, -i, -e, 's/After=cloud-final.service/# After=cloud-final.service/g', /usr/lib/systemd/system/docker-storage-setup.service] 
        - [sed, -i, -e, '$a STORAGE_DRIVER=overlay', /etc/sysconfig/docker-storage-setup ]
        - [sed, -i, -e, "s/OPTIONS='--selinux-enabled'/# OPTIONS='--selinux-enabled'/", /etc/sysconfig/docker]
        - [/bin/systemctl, daemon-reload]
        - [/bin/systemctl, restart,  docker.service]
        '''
    fpubkey = open(os.path.expanduser(cloudManager.key_filename + ".pub"))
    public_key=fpubkey.read()
    userdata = cloud_config_tpl.format(key=public_key)

    return userdata


if __name__ == "__main__":
    try:
        logging.basicConfig(format='%(asctime)s %(levelname)-8s %(name)-15s'
                                   ' %(message)s',
                            level=logging.DEBUG)
        # Disable requests and urllib3 package logger and warnings
        logging.getLogger("requests").setLevel(logging.ERROR)
        logging.getLogger("urllib3").setLevel(logging.ERROR)
        warnings.filterwarnings("ignore")

        # CC-IN2P3
        # image_name = "CentOS-7-x86_64-GenericCloud"
        # flavor_name = "m1.medium"
        # network_name = "lsst"
        # nics = []

        # Petasky
        # image_name = "CentOS 7"
        # flavor_name = "c1.medium"
        # network_name = "petasky-net"
        # nics = []

        # NCSA
        # image_name = "centos-7-qserv"
        # flavor_name = "m1.medium"
        # network_name = "LSST-net"
        # nics = [{'net-id': u'fc77a88d-a9fb-47bb-a65d-39d1be7a7174'}]
        # ssh_security_group = "Remote SSH"

        cloudManager = cloudmanager.CloudManager(add_ssh_key=True)

        cloudManager.manage_ssh_key()

        # Find a floating ip for gateway
        floating_ip = cloudManager.get_floating_ip()
        if not floating_ip:
            logging.fatal("Unable to add public ip to Qserv gateway")
            sys.exit(2)

        userdata = get_cloudconfig()

        # Create instances list
        instances = []

        # Create gateway instance and add floating_ip to it
        gateway_id = 0
        gateway_instance = cloudManager.nova_servers_create(gateway_id,
                                                            userdata)
        logging.info("Add floating ip ({0}) to {1}".format(floating_ip,
            gateway_instance.name))
        gateway_instance.add_floating_ip(floating_ip)

        # Manage ssh security group
        if cloudManager.ssh_security_group:
            gateway_instance.add_security_group(cloudManager.ssh_security_group)

        instances.append(gateway_instance)

        # Create worker instances
        for instance_id in range(1, NB_SERVER):
            worker_instance = cloudManager.nova_servers_create(instance_id,
                                                               userdata)
            instances.append(worker_instance)

        cloudManager.print_ssh_config(instances, floating_ip)

        # Wait for cloud config completion for all machines
        for instance in instances:
            cloudManager.detect_end_cloud_config(instance)

        cloudManager.check_ssh_up(instances)

        cloudManager.update_etc_hosts(instances)

        logging.debug("SUCCESS: Qserv Openstack cluster is up")

    except Exception as exc:
        logging.critical('Exception occured: %s', exc, exc_info=True)
        sys.exit(3)
