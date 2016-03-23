# Rename this file to env.sh and edit configuration parameters
# env.sh is sourced by other scripts from the directory

# shmux access
export PATH="$PATH:/opt/shmux/bin"

# Nodes names
# ===========

# Format for all node names
HOSTNAME_FORMAT="ccqserv%g.in2p3.fr"

# Master id
MASTER_ID=100

# Workers range
WORKER_FIRST_ID=101
WORKER_LAST_ID=124

# Master id
# MASTER_ID=125

# Workers range
# WORKER_FIRST_ID=126
# WORKER_LAST_ID=149

# Image names
# ===========

# Used to set images names, can be:
#   1. a git ticket branch but with _ instead of /
#   2. a git tag
# example: tickets_DM-5402
BRANCH=dev

# `docker run` settings
# =====================

# Data directory location on docker host, optional
HOST_DATA_DIR=/qserv/data               

# Log directory location on docker host, optional
HOST_LOG_DIR=/qserv/log

# Advanced configuration
# ======================

# Return a hostname list
# arg1: format of hostnames
# arg2: id of first host
# arg3: id of last host, optional
printHostname () {
    if [ -n "$3" ]; then
		local_last="$3"
	else
	    local_last="$2"
    fi
    seq --format "$1" --separator=' ' "$2" "$local_last"
}

MASTER=$(printHostname "$HOSTNAME_FORMAT" \
    "$MASTER_ID")                                   # Master hostname. Do not edit
WORKERS=$(printHostname "$HOSTNAME_FORMAT" \
	"$WORKER_FIRST_ID" "$WORKER_LAST_ID")			# Worker hostnames list. Do not edit

MASTER_IMAGE="qserv/qserv:${BRANCH}_master"	        # Do not edit
WORKER_IMAGE="qserv/qserv:${BRANCH}_worker"         # Do not edit


CONTAINER_NAME=qserv                                 # Do not edit
