// Vendored from https://github.com/Hopperpop/Sgp4-Library (MIT). Do not include directly - include sgp4.h
// Modifications: #include lines removed; code is wrapped in namespace vsgp4 by sgp4.h.
#ifndef _sgp4io_
#define _sgp4io_
/*     ----------------------------------------------------------------
*
*                                 sgp4io.h;
*
*    this file contains a function to read two line element sets. while
*    not formerly part of the sgp4 mathematical theory, it is
*    required for practical implemenation.
*
*                            companion code for
*               fundamentals of astrodynamics and applications
*                                    2007
*                              by david vallado
*
*       (w) 719-573-2600, email dvallado@agi.com
*
*    current :
*               3 sep 07  david vallado
*                           add operationmode for afspc (a) or improved (i)
*    changes :
*              20 apr 07  david vallado
*                           misc updates for manual operation
*              14 aug 06  david vallado
*                           original baseline
*       ----------------------------------------------------------------      */



// ------------------------- function declarations -------------------------

void twoline2rv
     (
      char      longstr1[130], char longstr2[130],
      char opsmode, gravconsttype  whichconst,
      elsetrec& satrec
     );

 bool twolineChecksum
      (
       const char      longstr[]
      );
#endif
