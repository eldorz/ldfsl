#!/bin/sh

# Shell script to automate running of fsl to process DTI (diffusion) and 
# MPRAGE (structural) data from DICOMS, for probabilistic tractography.

# Requires fsl, dcmtk, condor, fslview, dcn2nii

# Expected command line inputs are <MPRAGE directory>, <dti directory>,
# <output directory>. Each input directory should contain only DICOMs.

# test correct number of command line arguments and 1st 2 are directories
if [ $# -ne 3 ] || ! [ -d $1 ] || ! [ -d $2 ]
then
  echo "Usage: $0 <MPRAGE dir> <DTI dir> <output dir>" >&2
  exit 1
fi

str_dir=$1
dti_dir=$2
out_dir=$3

# check that directories contain dicom files
if ! dcmftest $str_dir/* >/dev/null
then
  echo "$0: $str_dir contains one or more non-DICOM files" >&2
  exit 1
fi
if ! dcmftest $dti_dir/* >/dev/null
then
  echo "$0: $dti_dir contains one or more non-DICOM files" >&2
  exit 1
fi

# if destination directory exists, but is not a directory, exit
if [ -e $out_dir ] && ! [ -d $out_dir ]
then
  echo "$0: $out_dir exists and is not a directory" >&2
  exit 1
fi 

# if destination directory doesn't exist, create it
if ! [ -d $out_dir ]
then
  mkdir $out_dir || echo "$0: Couldn't create $out_dir" >&2; exit 1
fi

# if destination directory is non-empty, prompt to delete
if [ "`ls -A $out_dir`" ]
then
  echo "$out_dir is not empty - delete contents? (y/n)"
  read answer
  if [ "$answer" = 'y' ]
  then
    rm -r $out_dir/*
  fi
fi

       
