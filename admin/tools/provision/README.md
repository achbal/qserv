# load openstack env
. ./petasky-openrc.sh
# provision Qserv cluster
./provision-qserv.py -f galactica.provision.conf
cp ssh_config ~/.ssh/config
cd ../docker/deployment/parallel/
# Edit env.sh
diff env.example.sh env.sh 
17c17
< # HOST_DATA_DIR=/qserv/data               
---
> HOST_DATA_DIR=/qserv/data               
20c20
< # HOST_LOG_DIR=/qserv/log
---
> HOST_LOG_DIR=/qserv/log
27c27
< HOSTNAME_FORMAT="qserv%g.domain.org"
---
> HOSTNAME_FORMAT="bvulpescu-qserv-%g"
30c30
< MASTER_ID=1
---
> MASTER_ID=0
33,34c33,34
< WORKER_FIRST_ID=2
< WORKER_LAST_ID=3
---
> WORKER_FIRST_ID=1
> WORKER_LAST_ID=4

# run multinode test
./run-multinode-tests.sh

# Log on master VM
ssh bvulpescu-qserv-0

# WARN: wait for container to be downloaded and up
# open shell in docker
docker exec -it qserv bash 
# open root shell in container
docker exec -it -u root qserv bash 
