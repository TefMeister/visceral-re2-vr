/* miniaudio_impl.c -- the one translation unit that compiles miniaudio + stb_vorbis (both public domain;
   third_party/miniaudio/LICENSE). Kept apart so their warnings stay out of our /W4 build. 2026-10-03. */
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define STB_VORBIS_HEADER_ONLY
#include "miniaudio/stb_vorbis.c"
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"
#undef STB_VORBIS_HEADER_ONLY
#include "miniaudio/stb_vorbis.c"
