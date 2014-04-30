#!/bin/bash
# Loading of qms meta for test cases on small datasets
# author Fabrice Jammes, fabrice.jammes@in2p3.fr

if [ "$#" != 3 ] ; then
    SCRIPT=`basename $0`
    echo $"Usage: ${SCRIPT} {QSERV_DIR} {DB_NAME} {TEST_ID}"
    exit 1
fi

BASE_DIR=$1
DB_NAME=$2
TEST_ID=$3

ERR_NO_META=25

DATA_DIR=${BASE_DIR}/tests/testdata/${TEST_ID}/data

if [ ! -d ${DATA_DIR} ]; then
    echo "Directory containing test dataset (${DATA_DIR}) doesn't exists."
    exit 1
fi

META_CMD="qserv_admin.py"

die() { echo "$@" 1>&2 ; exit 1; }

run_cmd() {
    if [ "$#" != 2 ] ; then
		echo $"Usage:  {CMD} {MSG}"
		return 1
    fi
    CMD=$1
    ACTION=$2
    echo
    echo "${ACTION}"
    echo "     ${CMD}"
    echo "${CMD};" | ${META_CMD}|| die "error (errno:$?) in step : ${ACTION}"

}

# qserv_admin.py does not have equvalent of checkDbExists yet
#ACTION="Checking if meta db ${DB_NAME} exists :";
#echo ${ACTION}
#CMD="${META_CMD} checkDbExists ${DB_NAME}"
#echo "     ${CMD}"
DB_EXISTS=yes$(${CMD})
#RET=$?

echo "Already existing database ${DB_NAME} : ${DB_EXISTS}"

# installing meta if needed
#if [ ${RET} -eq ${ERR_NO_META} ]
#then
    #    run_cmd "${META_CMD} installMeta" "Meta not currently installed : installing it"
#fi

# creating db if needed
if [ "${DB_EXISTS}" == "yes" ]
then
    run_cmd "DROP DATABASE ${DB_NAME}" "Dropping previous meta db ${DB_NAME}"
fi

run_cmd "CREATE DATABASE ${DB_NAME} ${DATA_DIR}/db.params" "Creating meta db ${DB_NAME}"

# needed to access schema files, specified with relative path in .params files
cd ${DATA_DIR} || die "Error while looking for tables meta-files in ${DATA_DIR}"

PARAMS_ORDER=${DATA_DIR}/params-loading-order.txt
if [ -r ${PARAMS_ORDER} ]
then
    echo "Specific loading order required for meta files"
    META_FILE_LST=`cat ${PARAMS_ORDER}`
else
    META_FILE_LST=`ls *.params | egrep -v "^db.params$"`
fi

if [ -z "${META_FILE_LST}" ]
then
    die "Can't load qms meta : no params files found in ${DATA_DIR}"
fi

for META_TABLE in ${META_FILE_LST}
do
	tablename=$(basename $META_TABLE .params)
    run_cmd "CREATE TABLE ${DB_NAME}.${tablename} ${DATA_DIR}/${META_TABLE}" "Creating meta table ${META_TABLE}"
done
