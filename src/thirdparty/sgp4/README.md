# Vendored SGP4 Library

Source: https://github.com/Hopperpop/Sgp4-Library (MIT license — see `LICENSE`).
Core SGP4 propagator by David Vallado (companion code for *Fundamentals of
Astrodynamics and Applications*, 2007), Arduino port and pass-prediction layer
by Hopperpop. Vendored 2026-07 from `master`.

## Why vendored instead of a pinned library

VAIL SUMMIT compiles as a single translation unit of headers. Vendoring the
library as headers keeps the build self-contained (no `arduino-cli/user/libraries`
addition, nothing extra for CI to install) and lets us fix two single-TU hazards:

1. **Symbol collisions** — the library defines very generic global names
   (`site`, `mag`, `dot`, `angle`, `jday`, `asinh`, enum value `none`).
   `asinh` in particular collides with newlib's math library. Everything is
   wrapped in `namespace vsgp4` by the umbrella header.
2. **Macro leakage** — `#define pi`, brent's `#define R` / `#define C`, etc.
   would poison every header included afterwards. The umbrella `#undef`s them.

## Layout

- `sgp4.h` — the only file to include. Opens `namespace vsgp4`, includes the
  headers and implementations in dependency order, cleans up macros.
- `*.h` / `*_impl.h` — the original headers and `.cpp` files, verbatim except
  that `#include` lines were stripped (the umbrella provides system headers).
  `*_impl.h` files correspond to the upstream `.cpp` files.

## Usage

```cpp
#include "src/thirdparty/sgp4/sgp4.h"

vsgp4::Sgp4 sat;
sat.init(name, line1, line2);   // NOTE: mutates line1/line2 while parsing -
                                // pass scratch copies, never cached TLE data
sat.site(lat, lon, altMeters);
sat.initpredpoint(unixTime, 0.0);
vsgp4::passinfo p;
sat.nextpass(&p, 20, false, minElevationDeg);
sat.findsat(unixTime);          // -> satAz / satEl / satDist
```

Do not edit the vendored files; put adaptations in `src/satellites/`.
