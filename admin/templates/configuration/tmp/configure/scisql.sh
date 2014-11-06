#!/usr/bin/env bash

set -e

QSERV_RUN_DIR={{QSERV_RUN_DIR}}
SCISQL_DIR={{SCISQL_DIR}}
MYSQL_DIR={{MYSQL_DIR}}
MYSQLD_SOCK={{MYSQLD_SOCK}}
MYSQLD_USER={{MYSQLD_USER}}
MYSQLD_PASS={{MYSQLD_PASS}}
PYTHON_BIN={{PYTHON_BIN}}
export PYTHONPATH={{PYTHONPATH}}

PYTHON_DIR=`dirname ${PYTHON_BIN}`
export PATH=${PYTHON_DIR}:${PATH}
export LD_LIBRARY_PATH={{LD_LIBRARY_PATH}}

${QSERV_RUN_DIR}/etc/init.d/mysqld start
cd ${SCISQL_DIR}
echo "-- Deploying sciSQL plugin in MySQL database"
# password is given on stdin, so that is can't be catched by ps command
PASSFILE=${QSERV_RUN_DIR}/tmp/pass.txt
cat <<EOF > ${PASSFILE}
${MYSQLD_PASS} 
EOF
cat  ${PASSFILE} | scisql-deploy.py --mysql-socket=${QSERV_RUN_DIR}/var/lib/mysql-worker/mysql.sock
rm ${PASSFILE}
${QSERV_RUN_DIR}/etc/init.d/mysqld stop ||
exit 1

echo "INFO: sciSQL installation and configuration SUCCESSFUL"
