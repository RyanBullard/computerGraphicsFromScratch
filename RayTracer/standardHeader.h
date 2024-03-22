#pragma once

#include <stdio.h>
#include <stdint.h>
#define checkalloc(ptr); if(ptr == NULL) {\
							 fprintf(stderr, "Out of memory!\n");\
							 exit(1);\
						 } // Makes code less cluttered
