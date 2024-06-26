#!/usr/bin/env bash

#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set -ex

get_neo_version() {
    commit_time=$(git -C ${REPO_DIR} show -s --format=%ct)
    commit_ww=($(${REPO_DIR}/scripts/neo_ww_calculator.py ${commit_time}))
    date_m=${commit_ww[1]}
    __NEO_VERSION_MAJOR_TMP=$(echo $commit_ww | awk -F '.' '{print $1;}')
    NEO_VERSION_MAJOR="${NEO_VERSION_MAJOR:-$__NEO_VERSION_MAJOR_TMP}"
    unset __NEO_VERSION_MAJOR_TMP
    __NEO_VERSION_MINOR_TMP=$(echo $commit_ww | awk -F '.' '{print $2;}')
    NEO_VERSION_MINOR="${NEO_VERSION_MINOR:-$__NEO_VERSION_MINOR_TMP}"
    unset __NEO_VERSION_MINOR_TMP
    __NEO_TAG_TMP=$(git -C ${REPO_DIR} describe --abbrev=1 --tags | awk -F"." '{ nn=split($NF, nfa, "."); if(nn==2) {printf("%s-%s", nfa[1], nfa[2]);} else {print $NF;} }')
    NEO_TAG="${NEO_TAG:-$__NEO_TAG_TMP}"
    NEO_TAG="${NEO_TAG:-9999}"
    unset __NEO_TAG_TMP
    __NEO_VERSION_BUILD_TMP=$(echo $NEO_TAG | awk -F '-' '{ print $1; }' | sed 's/^0*//')
    NEO_VERSION_BUILD="${NEO_VERSION_BUILD:-$__NEO_VERSION_BUILD_TMP}"
    unset __NEO_VERSION_BUILD_TMP
    __NEO_VERSION_HOTFIX_TMP=$(echo $NEO_TAG | awk -F '-' '{ if(NF>1) { print $2; } }')
    NEO_VERSION_HOTFIX="${NEO_VERSION_HOTFIX:-$__NEO_VERSION_HOTFIX_TMP}"
    NEO_VERSION_HOTFIX="${NEO_VERSION_HOTFIX:-0}"
    unset __NEO_VERSION_HOTFIX_TMP
}
