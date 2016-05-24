set -x
set -e

nova list | grep "jammes-qserv" | cut -d'|' -f 2| xargs nova delete
nova image-delete centos-7-qserv
python create-image.py
sleep 60
python provision-qserv.py
