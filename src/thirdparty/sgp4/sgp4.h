/*
 * SGP4 satellite orbit propagator - vendored umbrella header
 *
 * Source: https://github.com/Hopperpop/Sgp4-Library (MIT license, see LICENSE)
 * Core SGP4 math by David Vallado (companion code for "Fundamentals of
 * Astrodynamics and Applications", 2007); Arduino port by Hopperpop.
 *
 * Vendored for VAIL SUMMIT's header-only single-translation-unit build:
 *  - All #include lines were stripped from the original files; system headers
 *    are included once here, BEFORE the namespace opens.
 *  - Everything is wrapped in namespace vsgp4 so the library's generic global
 *    names (site, mag, dot, angle, jday, asinh, enum value `none`, ...) cannot
 *    collide with libc/newlib or project code.
 *  - Leaky macros (`pi`, brent's `R`/`C`, etc.) are #undef'd below so they do
 *    not poison code included after this header.
 *
 * Use: #include this file only. Access the API as vsgp4::Sgp4, vsgp4::passinfo.
 * Do not include the sibling files directly, and do not edit them (they are
 * vendored verbatim apart from the include stripping).
 */

#ifndef VSGP4_SGP4_H
#define VSGP4_SGP4_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

namespace vsgp4 {

// --- declarations (dependency order) ---
#include "sgp4unit.h"
#include "sgp4ext.h"
#include "sgp4io.h"
#include "sgp4coord.h"
#include "sgp4pred.h"
#include "brent.h"
#include "visible.h"

// --- implementations ---
#include "sgp4unit_impl.h"
#include "sgp4ext_impl.h"
#include "sgp4io_impl.h"
#include "sgp4coord_impl.h"
#include "visible_impl.h"
#include "brent_impl.h"
// brent_impl.h defines single-letter macros; kill them before the next file
#undef R
#undef C
#undef ITMAX
#undef ZEPS
#undef SHFT2
#undef SHFT3
#include "sgp4pred_impl.h"

} // namespace vsgp4

// Undefine every remaining macro the vendored code leaks, so nothing included
// after this header is affected.
#undef pi
#undef SGP4Version
#undef MAX_itter
#undef tol
#undef sunradius
#undef earthradius
#undef au

#endif // VSGP4_SGP4_H
