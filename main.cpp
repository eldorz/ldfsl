/*
 * main.cpp
 *
 *  Created on: Jul 10, 2013
 *      Author: laughlin
 */

/* Automate running of fsl to process DTI (diffusion) and MPRAGE (structural)
 * data from DICOMS, and generate corticospinal tracts.
 */

#include "ldfsl_utils.h"
#include "stdlib.h"
#include <iostream>
#include <string>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// main arguments are struct_dir, diff_dir, output_dir
int main(int argc, char **argv) {
	using std::cerr;
	using std::cin;
	using std::cout;
	using std::endl;
	using std::string;

	if (argc != 4) {
		cout << "usage: ldfsl struct_dir diff_dir output_dir" << endl;
		exit(1);
	}

	if (argv[3][0] == '~' || argv[3][0] == '.') {
		cout << "Sorry, can't use '~', '.' or '..', please use full path for "
				"destination directory" << endl;
		exit(1);
	}

	// add '/' to dicom directories, remove '/' from dest_dir
	string struct_temp = argv[1];
	string::size_type size = struct_temp.size();
	if (struct_temp[size - 1] == '/') {
		struct_temp.erase(size - 1, 1);
	}

	string diff_temp = argv[2];
	size = diff_temp.size();
	if (diff_temp[size - 1] == '/') {
		diff_temp.erase(size - 1, 1);
	}

	string dest_temp = argv[3];
	size = dest_temp.size();
	if (dest_temp[size - 1] == '/') {
		dest_temp.erase(size - 1, 1);
	}

	const string dicom_struct = struct_temp;
	const string dicom_diff = diff_temp;
	const string destination_dir = dest_temp;

	// set fsl environment variables
	char fsldirenv[] = "FSLDIR=/usr/share/fsl/5.0";
	char fsloutputtype[] = "FSLOUTPUTTYPE=NIFTI_GZ";
	char fslldlib[] = "LD_LIBRARY_PATH=/usr/lib/fsl/5.0";
	char path[] = "PATH=/usr/local/bin:/usr/bin:/bin:/usr/lib/fsl/5.0";
	char fslparallel[] = "FSLPARALLEL=condor";
	putenv(fsldirenv);
	putenv(fsloutputtype);
	putenv(fslldlib);
	putenv(path);
	putenv(fslparallel);

	// declare default directories
	const string bedpost_dir = destination_dir + ".bedpostX";
	const string std_space = "/usr/share/fsl/data/standard/MNI152_T1_2mm_brain";
	const string midbrain_mask =
			"/home/brain/fsl/templates/std_midbrain.nii.gz";
	const string waypoints_list = "/home/brain/fsl/templates/waypoints.txt";
	const string right_lat_gen_body =
			"/home/brain/fsl/templates/right_lateral_geniculate_body.nii.gz";
	const string left_lat_gen_body =
			"/home/brain/fsl/templates/left_lateral_geniculate_body.nii.gz";
	const string r_or_waypoints_list =
			"/home/brain/fsl/templates/r_or_waypoints.txt";
	const string l_or_waypoints_list =
			"/home/brain/fsl/templates/l_or_waypoints.txt";

	// build required command lines
	const string make_dest_dir = "mkdir " + destination_dir;
	const string struct_to_nii = "dcm2nii -o " + destination_dir + " "
			+ dicom_struct;
	const string diff_to_nii = "dcm2nii -o " + destination_dir + " "
			+ dicom_diff;
	const string rename_struct_1 = "mv " + destination_dir + "/co*.nii.gz "
			+ destination_dir + "/mprage.ldtemp";
	const string rename_struct_2 = "mv " + destination_dir +"/mprage.ldtemp "
			+ destination_dir + "/mprage.nii.gz";
	const string remove_extra_structs = "rm -f " + destination_dir
			+ "/*mprage*.nii.gz";
	const string rename_diff = "mv " + destination_dir + "/*.nii.gz "
			+ destination_dir + "/diff.nii.gz";
	const string rename_bvec = "mv " + destination_dir + "/*bvec "
			+ destination_dir + "/bvecs";
	const string rename_bval = "mv " + destination_dir + "/*bval "
			+ destination_dir + "/bvals";
	const string wipe_dest_dir = "rm -r -f " + destination_dir;
	const string fsldir = "/usr/lib/fsl/5.0/";
	const string swapdim = fsldir + "fslswapdim " + destination_dir
			+ "/mprage RL PA IS " + destination_dir +"/mprage_ax";
	const string reorient2std = fsldir + "fslreorient2std " + destination_dir
			+ "/mprage " + destination_dir + "/mprage_ax_precrop";
	const string cropNeck = fsldir + "robustfov -r " + destination_dir
			+ "/mprage_ax -i " + destination_dir + "/mprage";
	const string remove_old_mprage = "rm " + destination_dir + "/mprage.nii.gz";
	const string get_nodif = fsldir + "fslroi " + destination_dir
			+ "/diff " + destination_dir + "/nodif 0 1";
	const string bet_nodif = fsldir + "bet " + destination_dir + "/nodif "
			+ destination_dir + "/nodif_brain  -f 0.3 -g 0 -m";
	const string eddy_correct = fsldir + "eddy_correct " + destination_dir
			+ "/diff " + destination_dir + "/data 0";
	const string bedpost = fsldir + "bedpostx " + destination_dir
			+ " --nf=2 --fudge=1  --bi=1000";
	const string bet_struct = fsldir + "bet " + destination_dir + "/mprage_ax "
			+ destination_dir + "/mprage_brain -R -f 0.5 -g 0";
	const string diff2str = fsldir + "flirt -in " + bedpost_dir
			+ "/nodif_brain -ref " + destination_dir
			+ "/mprage_brain.nii.gz -omat " + bedpost_dir
			+ "/xfms/diff2str.mat -searchrx -90 90 -searchry -90 90 "
			"-searchrz -90 90 -dof 6 -cost corratio";
	const string str2diff = fsldir + "convert_xfm -omat " + bedpost_dir
			+ "/xfms/str2diff.mat -inverse " + bedpost_dir
			+ "/xfms/diff2str.mat";
	const string str2standard = fsldir + "flirt -in " + destination_dir
			+ "/mprage_brain.nii.gz "
			"-ref " + std_space + " -omat " + bedpost_dir
			+ "/xfms/str2standard.mat -searchrx -90 90 -searchry -90 90 "
			"-searchrz -90 90 -dof 12 -cost corratio";
	const string standard2str = fsldir + "convert_xfm -omat " + bedpost_dir
			+ "/xfms/standard2str.mat -inverse " + bedpost_dir
			+ "/xfms/str2standard.mat";
	const string diff2standard = fsldir + "convert_xfm -omat " + bedpost_dir
			+ "/xfms/diff2standard.mat -concat " + bedpost_dir
			+ "/xfms/str2standard.mat " + bedpost_dir + "/xfms/diff2str.mat";
	const string standard2diff = fsldir + "convert_xfm -omat " + bedpost_dir
			+ "/xfms/standard2diff.mat -inverse " + bedpost_dir
			+ "/xfms/diff2standard.mat";
	const string clear_cst_dir = "rm -rf " + destination_dir + "/cst";
	const string make_cst_dir = "mkdir -p " + destination_dir + "/cst";
	const string clear_r_or_dir = "rm -rf " + destination_dir + "/r_or";
	const string make_r_or_dir = "mkdir -p " + destination_dir + "/r_or";
	const string clear_l_or_dir = "rm -rf " + destination_dir + "/l_or";
	const string make_l_or_dir = "mkdir -p " + destination_dir + "/l_or";
	const string probtrack_cst = fsldir + "probtrackx2  -x " + midbrain_mask
			+ " -l --onewaycondition -c 0.2 -S 2000 --steplength=0.5 -P 5000 "
			"--fibthresh=0.01 --distthresh=0.0 --sampvox=0.0 --xfm="
			+ bedpost_dir + "/xfms/standard2diff.mat --forcedir --opd -s "
			+ bedpost_dir + "/merged -m " + bedpost_dir + "/nodif_brain_mask "
			+ "--dir=" + destination_dir + "/cst --waypoints=" + waypoints_list
			+ " --waycond=AND";
	const string probtrack_r_or = fsldir + "probtrackx2  -x "
			+ right_lat_gen_body
			+ " -l --onewaycondition -c 0.2 -S 2000 --steplength=0.5 -P 5000 "
			"--fibthresh=0.01 --distthresh=0.0 --sampvox=0.0 --xfm="
			+ bedpost_dir + "/xfms/standard2diff.mat --forcedir --opd -s "
			+ bedpost_dir + "/merged -m " + bedpost_dir + "/nodif_brain_mask "
			+ "--dir=" + destination_dir + "/right_or --waypoints="
			+ r_or_waypoints_list + " --waycond=AND";
	const string probtrack_l_or = fsldir + "probtrackx2  -x "
			+ left_lat_gen_body
			+ " -l --onewaycondition -c 0.2 -S 2000 --steplength=0.5 -P 5000 "
			"--fibthresh=0.01 --distthresh=0.0 --sampvox=0.0 --xfm="
			+ bedpost_dir + "/xfms/standard2diff.mat --forcedir --opd -s "
			+ bedpost_dir + "/merged -m " + bedpost_dir + "/nodif_brain_mask "
			+ "--dir=" + destination_dir + "/left_or --waypoints="
			+ l_or_waypoints_list + " --waycond=AND";
	const string transform_str2standard = fsldir + "flirt -in "
			+ destination_dir + "/mprage_brain.nii.gz -applyxfm -init "
			+ bedpost_dir + "/xfms/str2standard.mat -out "
			+ destination_dir + "/mprage_std.nii.gz -paddingsize 0.0 "
			"-interp trilinear -ref " + std_space;
	const string fslview = "fslview " + destination_dir
			+ "/mprage_std -b 0,1000 " + destination_dir
			+ "/cst/fdt_paths -b 1000,5000";

	// delete contents of destination directory
	cout << "Delete contents of " << destination_dir << " and continue? (y/N)"
			<< endl;
	char c;
	if (cin >> c && (c == 'y' || c == 'Y')) {
		systemCall(wipe_dest_dir);
		systemCall(make_dest_dir);
	} else {
		cout << "OK, quitting." << endl;
		exit(0);
	}

	// call mcverter to convert DICOM to nii, quit on error, rename files

	systemCall(struct_to_nii, "converting mprage to nii format...");
	systemCall(rename_struct_1);
	systemCall(remove_extra_structs);
	systemCall(diff_to_nii, "converting dti to nii format...");
	systemCall(rename_diff);
	systemCall(rename_struct_2);
	systemCall(rename_bvec);
	systemCall(rename_bval);

	cout << "Forking processes..." << endl;
	pid_t childpid = fork();
	if (childpid < 0) {
		cerr << "First fork failed" << endl;
		exit(1);
	}
	if (childpid == 0) {
		// I am the child process

		// correct plane to axial
		// systemCall(swapdim, "child correcting mprage axes..."); try reorient
		// systemCall(reorient2std, "child correcting mprage axes...");
		systemCall(cropNeck, "child cropping structural volume...");
		systemCall(remove_old_mprage);

		// brain-extract structural volume
		systemCall(bet_struct, "child brain-segmenting structural volume...");

		cout << "child done, joining parent..." << endl;
		exit(0);
	} else {
		// I am the parent process

		// extract and brain-extract b0 volume
		systemCall(get_nodif, "parent extracting b0 volume...");
		systemCall(bet_nodif, "parent brain-segmenting b0 volume...");

		// eddy correction
		systemCall(eddy_correct, "parent performing motion and eddy "
				"correction...");

		// two-crossing-fibre analysis
		systemCall(bedpost, "parent performing crossing fibre analysis "
				"(this may take hours)...");

		// wait until we are happy that bedpost is complete
		while (!bedpost_complete(bedpost_dir)) {
			usleep(1000000 * 60); // 60 secs
		}

		// wait for child
		int status;
		wait(&status);
		if (status != 0) {
			cerr << "First child returned " << status << ", something is wrong"
					<< endl;
			exit(1);
		}
	}

	// generate registration transforms
	systemCall(diff2str, "generating registration transforms...");
	systemCall(str2diff);
	systemCall(str2standard);

	cout << "Forking processes again..." << endl;
	childpid = fork();
	if (childpid < 0) {
		cerr << "Second fork failed" << endl;
		exit(1);
	}
	if (childpid == 0) {
		// I am the child process

		// standard to structural transform
		systemCall(standard2str, "child performing standard to structural "
				"transform...");

		// transform structural volume to standard space
		systemCall(transform_str2standard, "child transforming structural "
				"volume to standard space...");

		cout << "child done, joining parent..." << endl;
		exit(0);
	} else {
		// I am the parent process

		// diffusion to standard and inverse transforms
		systemCall(diff2standard, "parent performing diffusion to standard "
				"transform...");
		systemCall(standard2diff, "parent performing standard to diffusion "
				"transform...");

		// perform probabilistic tracking corticospinal tract
		systemCall(clear_cst_dir);
		systemCall(make_cst_dir);
		systemCall(probtrack_cst, "parent performing probabilistic tracking "
				"corticospinal tract (this may take hours)...");

		// perform probabilistic tracking right optic radiation
		systemCall(clear_r_or_dir);
		systemCall(make_r_or_dir);
		systemCall(probtrack_r_or, "parent performing probabilistic tracking "
				"right optic radiation (this may take hours)...");

		// perform probabilistic tracking right optic radiation
		systemCall(clear_l_or_dir);
		systemCall(make_l_or_dir);
		systemCall(probtrack_l_or, "parent performing probabilistic tracking "
				"left optic radiation (this may take hours)...");

		// wait for child
		int status;
		wait(&status);
		if (status != 0) {
			cerr << "Second child returned " << status << ", something is wrong"
					<< endl;
			exit(1);
		}
	}

	// display result in fslview
	systemCall(fslview);

	cout << "Done" << endl;
}
