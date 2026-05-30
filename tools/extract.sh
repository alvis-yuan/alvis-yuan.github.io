#!/bin/bash

filename=$(basename $1 .zip)

unzip -p ${filename}.zip > ${filename}.html
