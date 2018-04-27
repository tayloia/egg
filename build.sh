#!/bin/bash
make --jobs=$(nproc --ignore=2) $*
