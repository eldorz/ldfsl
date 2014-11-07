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

# start a log file
if [ -e $out_dir/ldfsl.log ]
then
  echo "Recommencing processing `date`" >> $out_dir/ldfsl.log  
else
  echo "Started processing `date`" > $out_dir/ldfsl.log
fi

# time starts now
start_time=`date +%s`

# check log and do if necessary
if ! grep 'finished nii conversion' $out_dir/ldfsl.log > /dev/null
then
  # convert structural dicom to nii
  echo "$0: Converting structural DICOM to nii"
  dcm2nii -o $out_dir $str_dir/*

  # rename and remove files
  mv $out_dir/co*.nii.gz $out_dir/mprage
  rm $out_dir/*.nii.gz

  # convert dti dicom to nii
  echo "$0: Converting dti DICOM to nii"
  dcm2nii -o $out_dir $dti_dir/*

  # rename and remove files
  mv $out_dir/*.nii.gz $out_dir/diff.nii.gz
  mv $out_dir/mprage $out_dir/mprage.nii.gz
  mv $out_dir/*bvec $out_dir/bvecs
  mv $out_dir/*bval $out_dir/bvals

  # write log
  $end_time=`date +%s`
  $elapsed=`expr \( $end_time - $start_time \) / 60`
  echo "finished nii conversion at $elapsed min" >> $out_dir/ldfsl.log
fi

# now we want to do some background work
export out_dir
export start_time
if ! grep 'finished structural segmentation' $out_dir/ldfsl.log > /dev/null
then
  (
  echo "$0: cropping structural volume..."
  robustfov -r $out_dir/mprage_ax -i $out_dir/mprage
  echo "$0: brain-segmenting structural volume..."
  bet $out_dir/mprage_ax $out_dir/mprage_brain -R -f 0.5 -g 0
  end_time=`date +%s`
  elapsed=`expr \( $end_time - $start_time \) / 60`
  echo "finished structural segmentation at $elapsed min" >> $out_dir/ldfsl.log
  )&
  str_process=$!
  echo "$0: structural analysis started with pid $str_process"
fi

if ! grep 'finished dti segmentation' $out_dir/ldfsl.log > /dev/null
then
  (
  echo "$0: extracting b0 volume..."
  fslroi $out_dir/diff $out_dir/nodif 0 1
  echo "$0: segmenting b0 volume..."
  bet $out_dir/nodif $out_dir/nodif_brain -f 0.3 -g 0 -m
  echo "$0: performing motion and eddy correction..."
  eddy_correct $out_dir/diff $out_dir/data 0
  end_time=`date +%s`
  elapsed=`expr \( $end_time - $start_time \) / 60`
  echo "finished dti segmentation at $elapsed min" >> $out_dir/ldfsl.log
  )&
  dti_process=$!
  echo "$0: diffusion analysis started with pid $dti_process"
fi

# dti process must be complete before crossing-fibre analysis
wait $dti_process

# peform crossing fibre analysis
if ! grep 'finished crossing fibre analysis' $out_dir/ldfsl.log > /dev/null
then
  echo "$0: performing crossing fibre analysis (may take hours)"
  FSLPARALLEL=condor
  export FSLPARALLEL
  if [ -d "$out_dir.bedpostX" ]
  then
    rm -r "$out_dir.bedpostX"
  fi
  bedpostx $out_dir --nf=2 --fudge=1 --bi=1000
  # wait for bedpost to complete
  $out_dir.bedpostX/monitor
  # write to log
  end_time=`date +%s`
  elapsed=`expr \( $end_time - $start_time \) / 60`
  echo "finished crossing fibre analysis at $elapsed min" >> $out_dir/ldfsl.log
fi

# both processes must be complete before continuing
wait $str_process

# generate registration transforms
std_space=/usr/share/fsl/data/standard/MNI152_T1_2mm_brain
bedpost_dir=$out_dir.bedpostX
if ! grep 'finished registration transforms' $out_dir/ldfsl.log > /dev/null
then
  echo "$0: performing registration transforms..."
  flirt -in $bedpost_dir/nodif_brain -ref $out_dir/mprage_brain.nii.gz \
    -omat $bedpost_dir/xfms/diff2str.mat -searchrx -90 90 -searchry -90 90
  convert_xfm -omat $bedpost_dir/xfms/str2diff.mat \
    -inverse $bedpost_dir/xfms/diff2str.mat
  flirt -in $out_dir/mprage_brain.nii.gz -ref $std_space \
    -omat $bedpost_dir/xfms/str2standard.mat -searchrx -90 90 -searchry -90 90 \
    -searchrz -90 90 -dof 12 -cost corratio
  convert_xfm -omat $bedpost_dir/xfms/standard2str.mat \
    -inverse $bedpost_dir/xfms/str2standard.mat
  convert_xfm -omat $bedpost_dir/xfms/diff2standard.mat \
    -concat $bedpost_dir/xfms/str2standard.mat $bedpost_dir/xfms/diff2str.mat
  convert_xfm -omat $bedpost_dir/xfms/standard2diff.mat \
    -inverse $bedpost_dir/xfms/diff2standard.mat
  # write to log
  end_time=`date +%s`
  elapsed=`expr \( $end_time - $start_time \) / 60`
  echo "finished registration transforms at $elapsed min" >> $out_dir/ldfsl.log
fi

# transform structural to standard space
flirt -in $out_dir/mprage_brain.nii.gz -applyxfm \
  -init $bedpost_dir/xfms/str2standard.mat -out $out_dir/mprage_std.nii.gz \
  -paddingsize 0.0 -interp trilinear -ref $std_space

# perform probabilistic tracking corticospinal tract
if ! grep 'finished tracking' $out_dir/ldfsl.log > /dev/null
then
  (
  [ -d $out_dir/cst ] && rm -r $out_dir/cst
  mkdir -p $out_dir/cst
  midbrain_mask=/home/brain/fsl/templates/std_midbrain.nii.gz
  waypoints_list=/home/brain/fsl/templates/waypoints.txt
  probtrackx2  -x $midbrain_mask -l --onewaycondition -c 0.2 -S 2000 \
    --steplength=0.5 -P 5000 --fibthresh=0.01 --distthresh=0.0 --sampvox=0.0 \
    --xfm=$bedpost_dir/xfms/standard2diff.mat --forcedir --opd \
    -s $bedpost_dir/merged -m $bedpost_dir/nodif_brain_mask --dir=$out_dir/cst \
    --waypoints=$waypoints_list --waycond=AND
  )&
  cst_process=$!

  # perform probabilistic tracking right optic radiation
  (
  [ -d $out_dir/right_or ] && rm -r $out_dir/right_or
  mkdir -p $out_dir/right_or
  r_lat_gen_bod=/home/brain/fsl/templates/right_lateral_geniculate_body.nii.gz
  r_or_waypoints_list=/home/brain/fsl/templates/r_or_waypoints.txt
  probtrackx2  -x $r_lat_gen_bod -l --onewaycondition -c 0.2 -S 2000 \
    --steplength=0.5 -P 5000 --fibthresh=0.01 --distthresh=0.0 --sampvox=0.0 \
    --xfm=$bedpost_dir/xfms/standard2diff.mat --forcedir --opd \
    -s $bedpost_dir/merged -m $bedpost_dir/nodif_brain_mask \
    --dir=$out_dir/right_or --waypoints=$r_or_waypoints_list --waycond=AND
  )&
  ror_process=$!

  # perform probabilistic tracking left optic radiation
  (
  [ -d $out_dir/left_or ] && rm -r $out_dir/left_or
  mkdir -p $out_dir/left_or
  l_lat_gen_bod=/home/brain/fsl/templates/left_lateral_geniculate_body.nii.gz
  l_or_waypoints_list=/home/brain/fsl/templates/l_or_waypoints.txt
  probtrackx2  -x $l_lat_gen_bod -l --onewaycondition -c 0.2 -S 2000 \
    --steplength=0.5 -P 5000 --fibthresh=0.01 --distthresh=0.0 --sampvox=0.0 \
    --xfm=$bedpost_dir/xfms/standard2diff.mat --forcedir --opd \
    -s $bedpost_dir/merged -m $bedpost_dir/nodif_brain_mask \
    --dir=$out_dir/left_or --waypoints=$l_or_waypoints_list --waycond=AND
  )&
  lor_process=$!
  
  # wait for finish
  wait $cst_process
  wait $ror_process
  wait $lor_process
  
  # write to log
  end_time=`date +%s`
  elapsed=`expr \( $end_time - $start_time \) / 60`
  echo "finished tracking at $elapsed min" >> $out_dir/ldfsl.log
fi
  
# print elapsed time
end_time=`date +%s`
elapsed=`expr \( $end_time - $start_time \) / 60`
echo "$0 has taken $elapsed minutes" 
echo "finished all at $elapsed min" >> $out_dir/ldfsl.log

# launch fslview
fslview $out_dir/mprage_std -b 0,1000 $out_dir/cst/fdt_paths -b 1000,5000 \
  $out_dir/right_or/fdt_paths -b 1000,5000 \
  $out_dir/left_or/fdt_paths -b 1000,5000       
