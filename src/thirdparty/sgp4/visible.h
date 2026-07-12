// Vendored from https://github.com/Hopperpop/Sgp4-Library (MIT). Do not include directly - include sgp4.h
// Modifications: #include lines removed; code is wrapped in namespace vsgp4 by sgp4.h.
/*
This file contains miscellaneous functions required for calculating visible satellits.
Functions were originally written for Matlab as companion code for "Fundamentals of Astrodynamics 
and Applications" by David Vallado (2007). (w) 719-573-2600, email dvallado@agi.com

Ported to C++ by Bram Gurdebeke with some modifications, November 2015.
*/

#ifndef _visible_
#define _visible_


void sun ( double jd, double rsun[3] ); //calculates sun coordinates


#endif
