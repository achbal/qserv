set -x
set -e

nova list | grep "bvulpescu-qserv-" | cut -d'|' -f 2| xargs nova delete
./provision-qserv.py -f galactica.provision.conf

