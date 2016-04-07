# Rename this file to env.sh and edit configuration parameters
# env.sh is sourced by other scripts from the directory

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
# HOST_DATA_DIR=/qserv/data               

# Log directory location on docker host, optional
# HOST_LOG_DIR=/qserv/log


# Nodes names
# ===========

# Format for all node names
HOSTNAME_FORMAT="qserv%g.domain.org"

# Master id
MASTER_ID=1

# Workers range
WORKER_FIRST_ID=2
WORKER_LAST_ID=3

# Disjoint sequences of host names can
# be set directly using variables below
MASTER=$(printf "$HOSTNAME_FORMAT" "$MASTER_ID")    # Master hostname. Do not edit
WORKERS=$(seq --format "$HOSTNAME_FORMAT" \
    --separator=' ' "$WORKER_FIRST_ID" \
    "$WORKER_LAST_ID")                              # Worker hostnames list. Do not edit


# Advanced configuration
# ======================

MASTER_IMAGE="qserv/qserv:${BRANCH}_master"         # Do not edit
WORKER_IMAGE="qserv/qserv:${BRANCH}_worker"         # Do not edit


CONTAINER_NAME=qserv                                # Do not edit
