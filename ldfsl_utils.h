/*
 * ldfsl_utils.h
 *
 *  Created on: Jul 10, 2013
 *      Author: laughlin
 */

#ifndef LDFSL_UTILS_H_
#define LDFSL_UTILS_H_

#include <string>

using namespace std;

void systemCall(const string &command); // call a command, exits on error
void systemCall(const string &command, const string &message);

bool bedpost_complete(const string &bedpost_dir); // checks log files and determines if bedpost has finished

#endif /* LDFSL_UTILS_H_ */
