 #!/bin/sh

# Test script which performs the following tasks:

# Create image
# Boot instances
# Launch Qserv containers

# @author  Oualid Achbal, IN2P3


# Source the cloud openrc file
#. ./LSST-openrc.sh
#. ./petasky-openrc.sh
#. ./lsst-openrc.sh

set -e
set -x

# Delete the previous instances for a new test
nova list | grep "$OS_USERNAME-qserv" | cut -d'|' -f 2| xargs nova delete

# Delete the snapshot created for a new test
nova image-delete centos-7-qserv

# Take a snapshot
python create-image.py -f ncsa.conf

# Execute provision-qserv with an input cloud conf file
python provision-qserv.py -f ncsa.provision.conf

cp ssh_config ~/.ssh/config

cd ../docker/deployment/parallel

#update env.sh
#mv env.example.sh env.sh
sed -i -e 's/$HOSTNAME_FORMAT/$OS_USERNAME-qserv-%g/g' env.sh
sed -i -e "s/MASTER_ID=1/MASTER_ID=0/" env.sh
sed -i -e "s/WORKER_FIRST_ID=2/WORKER_FIRST_ID=1/" env.sh
sed -i -e "s/WORKER_LAST_ID=3/WORKER_LAST_ID=2/" env.sh

# Install shmux before running multi node tests
#http://web.taranis.org/shmux/

# Run multi node tests
./run-multinode-tests.sh


