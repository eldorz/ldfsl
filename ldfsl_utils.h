/*
 * ldfsl_utils.h
 *
 *  Created on: Jul 10, 2013
 *      Author: laughlin
 */

#ifndef LDFSL_UTILS_H_
#define LDFSL_UTILS_H_

#include <string>

void systemCall(const std::string &command); // call a command, exits on error
void systemCall(const std::string &command, const std::string &message);

bool bedpost_complete(const std::string &bedpost_dir); // checks log files and
										// determines if bedpost has finished
void printElapsedTime(clock_t start);

#endif /* LDFSL_UTILS_H_ */
