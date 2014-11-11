#!/bin/sh
while true
do
  condor_q | grep ' R '
  echo
  sleep 10
done
