#!/usr/bin/env bash

set -e

QSERV_RUN_DIR={{QSERV_RUN_DIR}}
PATH={{PATH}}
MYSQLD_SOCK={{MYSQLD_SOCK}}
MYSQLD_DATA_DIR={{MYSQLD_DATA_DIR}}
MYSQLD_HOST={{MYSQLD_HOST}}
MYSQLD_PORT={{MYSQLD_PORT}}

SQL_DIR=${QSERV_RUN_DIR}/tmp/configure/sql

is_socket_available() {
    # run command in subshell so that we can redirect errors (which are expected)
    (timeout 1 bash -c "cat < /dev/null > /dev/tcp/${MYSQLD_HOST}/${MYSQLD_PORT}") 2>/dev/null
    # status == 1 means that the socket is available
    local status=$?
    local retcode=0
    if [[ ${status} == 0 ]]; then 
        echo "WARN: A service is already running on MySQL socket : ${MYSQLD_HOST}:${MYSQLD_PORT}"
	echo "WARN: Please stop it and relaunch configuration procedure"
        retcode=1
    elif [[ ${status} == 124 ]]; then 
	echo "WARN: A time-out occured while testing MySQL socket state : ${MYSQLD_HOST}:${MYSQLD_PORT}"
	echo "WARN: Please check that a service doesn't already use this socket, possibly with an other user account"
        retcode=124
    elif [[ ${status} != 1 ]]; then 
	echo "WARN: Unable to test MySQL socket state : ${MYSQLD_HOST}:${MYSQLD_PORT}"
	echo "WARN: Please check that a service doesn't already use this socket"
        retcode=2
    fi 
    return ${retcode}
}

is_socket_available || exit 1

echo "-- Removing previous data." &&
rm -rf ${MYSQLD_DATA_DIR}/* &&
echo "-- ." &&
echo "-- Installing mysql database files." &&
mysql_install_db --defaults-file=${QSERV_RUN_DIR}/etc/my.cnf --user=${USER} >/dev/null ||
{
    echo "ERROR : mysql_install_db failed, exiting"
    exit 1
}
echo "-- Starting mysql server." &&
${QSERV_RUN_DIR}/etc/init.d/mysqld start &&
sleep 5 &&
echo "-- Changing mysql root password." &&
mysql --no-defaults -S ${MYSQLD_SOCK} -u root < ${SQL_DIR}/mysql-password.sql &&
echo "-- Shutting down mysql server." &&
${QSERV_RUN_DIR}/etc/init.d/mysqld stop || 
{
    echo -n "ERROR : Failed to set mysql root user password."
    echo "Please set the mysql root user password with : "
    echo "mysqladmin -S ${QSERV_RUN_DIR}/var/lib/mysql/mysql.sock -u root password <password>"
    echo "mysqladmin -u root -h ${MYSQLD_HOST} -P${MYSQLD_PASS} password <password>"
    exit 1
}

rm -rf ${QSERV_RUN_DIR}/var/lib/mysql-worker/* &&
echo "-- ." &&
echo "-- Installing mysql database files for worker." &&
MYSQLD_SOCK_WORKER=${QSERV_RUN_DIR}/var/lib/mysql-worker/mysql.sock
mysql_install_db --defaults-file=${QSERV_RUN_DIR}/etc/my.worker.cnf --user=${USER}
>/dev/null ||
{
    echo "ERROR : mysql_install_db failed, exiting"
    exit 1
}

mkdir -p ${QSERV_RUN_DIR}/var/run/mysqld-worker
mysqld_safe --defaults-file=${QSERV_RUN_DIR}/etc/my.worker.cnf >/dev/null 2>&1 &
sleep 1 
mysql --no-defaults -S ${MYSQLD_SOCK_WORKER} -u root < ${SQL_DIR}/mysql-password.sql &&
rm ${SQL_DIR}/mysql-password.sql
kill `cat ${QSERV_RUN_DIR}/var/run/mysqld-worker/mysqld.pid`

echo "INFO: MySQL initialization SUCCESSFUL"
