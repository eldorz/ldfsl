/*
 * ldfsl_utils.cpp
 *
 *  Created on: Jul 10, 2013
 *      Author: laughlin
 *
 *  Utility functions for ldfsl
 */

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <dirent.h>

void systemCall(const std::string &command) {
	int ret_val = system(command.c_str());
	if (ret_val != 0) {
		std::cerr << "Error: \nCommand issued: " << command << std::endl;
		std::cerr << "Value returned: " << ret_val << std::endl;
		exit(EXIT_FAILURE);
	}
}

void systemCall(const std::string &command, const std::string &message) {
	std::cout << message << std::endl;
	systemCall(command);
}

// warning this will work for 64 directions only
bool bedpost_complete(const std::string &bedpost_dir) {
	using std::cerr;
	using std::cout;
	using std::endl;
	using std::string;
	using std::vector;

	static bool initialised = false;
	static const string logdir = bedpost_dir + "/logs";
	static vector<string> logsLeft;
	static int bpxNum = -1;
	static int bedpostNum = -1;
	static string bpxName = "bpx_postproc.o";
	static string bedpostName = "bedpostx.o";

	if (!initialised) {

		// find the filenames of the bedpostx files
		DIR *dirStream;
		struct dirent *dirEntry;

		if ((dirStream = opendir(logdir.c_str())) == NULL) {
			cout << "Could not open log directory" << endl;
			exit(1);
		}

		while((dirEntry = readdir(dirStream))) {
			string entryString = dirEntry->d_name;
			if (bpxNum == -1) {
				auto found = entryString.find(bpxName);
				if (found != string::npos) {
					string digits = entryString.substr(bpxName.size(),
							entryString.size() - bpxName.size());
					bpxNum = stoi(digits);
					bpxName.append(digits);
				}
			}
			if (bedpostNum == -1) {
				auto found = entryString.find(bedpostName);
				if (found != string::npos) {
					auto secondDot = entryString.find(".", found
							+ bedpostName.size());
					string digits = entryString.substr(bedpostName.size(),
							secondDot - bedpostName.size());
					bedpostNum = stoi(digits);
					bedpostName.append(digits).append(".");
				}
			}
		}

		// set up our vector of logs to check

		for (int i = 1; i <= 65; i++) {
			string digit = std::to_string(i);
			string filename = logdir + "/" + bedpostName + digit;
			logsLeft.push_back(filename);
		}

		initialised = true;
	}

	// check each bedpostx log to see if reports done.
	auto iter = logsLeft.begin();
	while (iter != logsLeft.end()) {

		std::ifstream file(*iter);

		if (file) {
			string data;
			if ((file >> data) && (data == "Done")) {
				string filename = *iter;
				cout << filename << " complete." << endl;
				iter = logsLeft.erase(iter);
			} else {
				++iter;
			}
		} else {
			string s = *iter;
			cerr << "Failed to open file " << s << endl;
			exit(1);
		}
	}

	cout << "logs left: " << logsLeft.size() << endl;

	// if there are no bedpostx logs left to check, check bpx_postproc
	if (logsLeft.size() == 0) {
		string filename = logdir;
		filename.append("/").append(bpxName);
		std::ifstream file(filename);
		if (file) {
			string data;
			while (file >> data) {
				if (data.find("Done") != string::npos) {
					cout << filename << " complete." << endl;
					return true;
				}
			}
		} else {
			string s = *iter;
			cerr << "Failed to open file " << s << endl;
			exit(1);
		}
	}

	return false;
}


