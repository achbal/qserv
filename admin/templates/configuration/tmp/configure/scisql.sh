#!/usr/bin/env bash

set -e

QSERV_RUN_DIR='{{QSERV_RUN_DIR}}'
SCISQL_DIR='{{SCISQL_DIR}}'
MYSQL_DIR='{{MYSQL_DIR}}'
MYSQLD_SOCK='{{MYSQLD_SOCK}}'
MYSQLD_USER='{{MYSQLD_USER}}'
MYSQLD_PASS='{{MYSQLD_PASS}}'
PYTHON_BIN='{{PYTHON_BIN}}'
export PYTHONPATH='{{PYTHONPATH}}'

PYTHON_DIR=$(dirname ${PYTHON_BIN})
export PATH=${PYTHON_DIR}:${PATH}
export LD_LIBRARY_PATH='{{LD_LIBRARY_PATH}}'

${QSERV_RUN_DIR}/etc/init.d/mysqld start
echo "-- Deploying sciSQL plugin in MySQL database"
echo "${MYSQLD_PASS}" | "${SCISQL_DIR}/bin/scisql-deploy.py" --mysql-dir="$MYSQL_DIR" \
				 --mysql-socket="${MYSQLD_SOCK}" \
				 --mysql-user="${MYSQLD_USER}"
"${QSERV_RUN_DIR}/etc/init.d/mysqld" stop ||
exit 1

echo "INFO: sciSQL installation and configuration SUCCESSFUL"
