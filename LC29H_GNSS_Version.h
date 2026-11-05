#pragma once
// Compile-time version for consumer static_assert pins (Mozihao / Base Station).
// Keep in sync with library.properties. Optional peer RTCM helper libs stay out of this library — not required.

#ifndef LC29H_GNSS_VERSION_MAJOR
#define LC29H_GNSS_VERSION_MAJOR 0
#endif
#ifndef LC29H_GNSS_VERSION_MINOR
#define LC29H_GNSS_VERSION_MINOR 3
#endif
#ifndef LC29H_GNSS_VERSION_PATCH
#define LC29H_GNSS_VERSION_PATCH 3
#endif

#ifndef LC29H_GNSS_VERSION_NUMBER
#define LC29H_GNSS_VERSION_NUMBER \
  ((LC29H_GNSS_VERSION_MAJOR * 10000) + (LC29H_GNSS_VERSION_MINOR * 100) + LC29H_GNSS_VERSION_PATCH)
#endif

#ifndef LC29H_GNSS_VERSION_STRING
#define LC29H_GNSS_VERSION_STRING "0.3.3"
#endif
