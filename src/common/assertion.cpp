/*
 * common/assertion.h  - Program assertion header
 * Copyright (C) 2013  D Haley
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "assertion.h"
#include <iostream>

void userAskAssert(const char * const filename, const unsigned int lineNumber) 
{

	static bool skipAll=false;

	std::cerr << "ASSERTION ERROR!" << std::endl;
	std::cerr << "Filename: " << filename << std::endl;
	std::cerr << "Line number: " << lineNumber << std::endl;

	if(skipAll)
	{
		std::cerr << "\tContinuing, as previously requested" << std::endl;
		return;
	}
	
	std::cerr << "Do you wish to continue? - (y)es/(n)o/(a)lways -";
	char y = '_';
	while (y != 'n' && y != 'y' && y!= 'a')
		std::cin >> y;

	if (y == 'n')
		exit(1);

	if(y == 'a')
		skipAll=true;
}
