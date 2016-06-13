Provision Qserv using Docker+Openstack
======================================

Pre-requisite
-------------

#Install the pre-requisite software
sudo apt-get install python-dev python-pip

#Install the OpenStack client
pip install python-openstackclient

#Install the nova client
sudo apt-get install python-novaclient

Provision Qserv & run multinode tests
-------------------------------------

# clone Qserv repository
SRC_DIR=${HOME}/src
mkdir ${SRC_DIR}
cd ${SRC_DIR}
git clone https://github.com/achbal/qserv.git
git checkout tickets/DM-6062
cd qserv/admin/tools/provision

# Download the cloud openrc file in Openstack platform and source it
# This is an example of chicago cloud
. ./LSST-openrc.sh.example

# Update the configuration file which contains instance parameters
mv ncsa.conf.example ncsa.conf
vi ncsa.conf

# Update the test file to run
# by giving the conf file, the snapshot name and number of servers to boot
vi test.sh

# Install shmux before running multi node tests in test.sh
# http://web.taranis.org/shmux/

# run test
./test.sh