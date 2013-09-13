export QSERV_BASE=%(QSERV_BASE_DIR)s

# qserv 
# WARNING : set qserv binary directory before running qserv in order to use
# %(QSERV_BASE_DIR)s/bin/python

if [ -z "${QSERV_ENV_SETTED}" ]; then
	export PATH=${QSERV_BASE}/bin:${PATH}
	# in order to load numpy
	export PYTHONPATH=${PYTHONPATH}:/usr/lib64/python2.6/site-packages/
	export QSERV_ENV_SETTED=1
fi 

alias qserv-start="qserv-admin --start"
alias qserv-stop="qserv-admin --stop  --dbpass \"%(MYSQLD_PASS)s\"; rm -f ${QSERV_BASE}/xrootd-run/result/*"
# TODO : manage MySQL pass correctly
alias qserv-status="qserv-admin --status --dbpass \"%(MYSQLD_PASS)s\""

if [[ $1 ]]
then
case $1 in 
start)
qserv-start
;;
stop)
qserv-stop
;;
status)
qserv-status
;;
*)
exit 1
;;
esac
fi
