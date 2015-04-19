#include "gsl_helper.h"

#include <iostream>


using std::cerr;
using std::endl;

void gslPrint(const gsl_matrix *m)
{
	for(unsigned int ui=0;ui<m->size1; ui++)
	{
		cerr << "|";
		for(unsigned int uj=0; uj<m->size2; uj++)
		{
		
			cerr << gsl_matrix_get(m,ui,uj);
			
			if (uj +1 < m->size2)
				cerr << ",\t" ;
		}
		cerr << "\t|" << endl;
	}	
}
