 #!/bin/sh

# Test script which performs the following tasks:

# Create image
# Boot instances
# Launch Qserv containers

# @author  Oualid Achbal, IN2P3


set -e

# Source the cloud openrc file
. ./LSST-openrc.sh.example
# . ./petasky-openrc.sh.example

# Choose a number of instances to boot
NB_SERVERS=3

# Choose the cloud conf file which contains instance parameters
CONF_FILE="ncsa.conf.example"

# Choose snapshot name and update the cloud conf file chosen
SNAPSHOT_NAME="centos-7-qserv"

# Move to openstackclient CLI
# sudo pip install python-openstackclient

# Delete the previous instances for a new test
PREVIOUS_INSTANCE_IDS=$(openstack server list | grep "$OS_USERNAME-qserv" | cut -d'|' -f 2)
# Test PREVIOUS_INSTANCE_IDS 
if [ "$PREVIOUS_INSTANCE_IDS" ]
then
	openstack server delete $PREVIOUS_INSTANCE_IDS
else
	echo "No existing servers to delete"
fi

# Delete the snapshot created for a new test
PREVIOUS_IMAGE_ID=$(openstack image list | grep "$SNAPSHOT_NAME" | cut -d'|' -f 2)
# Test PREVIOUS_IMAGE_ID
if [ "$PREVIOUS_IMAGE_ID" ]
then
	openstack image delete $PREVIOUS_IMAGE_ID
else
	echo "No existing image denoted $SNAPSHOT_NAME to delete"
fi

set -x

# Take a snapshot
python create-image.py -f "$CONF_FILE" -vv

# Execute provision-qserv with an input cloud conf file
python provision-qserv.py -f "$CONF_FILE" -n "$NB_SERVERS" -vv

# Warning : if  multinode tests failed save your ~/.ssh/config
# your old ~/.ssh/config is in~/.ssh/config.backup
cp ~/.ssh/config ~/.ssh/config.backup

cp ssh_config ~/.ssh/config

cd ../docker/deployment/parallel

# Update env.sh
# cp env.example.sh env.sh
sed -i -e 's/$HOSTNAME_FORMAT/$OS_USERNAME-qserv-%g/g' env.sh
sed -i -e "s/MASTER_ID=1/MASTER_ID=0/" env.sh
sed -i -e "s/WORKER_FIRST_ID=2/WORKER_FIRST_ID=1/" env.sh
sed -i -e "s/WORKER_LAST_ID=3/WORKER_LAST_ID=$NB_SERVERS/" env.sh

# Install shmux before running multi node tests
# http://web.taranis.org/shmux/

# Run multinode tests
./run-multinode-tests.sh

cp ~/.ssh/config.backup ~/.ssh/config

