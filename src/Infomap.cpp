/**********************************************************************************

 Infomap software package for multi-level network clustering

 Copyright (c) 2013, 2014 Daniel Edler, Martin Rosvall

 For more information, see <http://www.mapequation.org>


 This file is part of Infomap software package.

 Infomap software package is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Infomap software package is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with Infomap software package.  If not, see <http://www.gnu.org/licenses/>.

**********************************************************************************/

#include <iostream>
#include "utils/Log.h"
#include "io/Config.h"
#include <string>
#include "core/Infomap.h"
#ifdef _OPENMP
#include <omp.h>
#endif

namespace infomap {

int run(const std::string& flags)
{
	try
	{
		Config conf(flags, true);

		Infomap infomap(conf);
		// Infomap infomap(flags, true);

		infomap.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	catch(char const* e)
	{
		std::cerr << "Str error: " << e << std::endl;
	}

	return 0;
}

}

#ifndef AS_LIB
int main(int argc, char* argv[])
{
	std::ostringstream args("");
	for (int i = 1; i < argc; ++i)
		args << argv[i] << (i + 1 == argc? "" : " ");

	return infomap::run(args.str());

	return 0;
}
#endif
