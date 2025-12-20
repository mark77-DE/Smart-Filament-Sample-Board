// include/version_info.h
#pragma once

// Wenn der Auto-Header existiert, nutze ihn zuerst
#if defined(__has_include)
  #if __has_include("version_auto.h")
    #include "version_auto.h"
  #endif
#endif

// Fallbacks: greifen nur, wenn der Auto-Header nicht (oder fehlerhaft) gesetzt hat
#ifndef FIRMWARE_VERSION
  #define FIRMWARE_VERSION "0.0.0"
#endif
#ifndef GIT_HASH
  #define GIT_HASH "nogit"
#endif
