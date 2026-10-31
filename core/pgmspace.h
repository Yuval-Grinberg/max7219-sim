// pgmspace.h — on a PC there is no separate flash, so PROGMEM data is plain memory.
#pragma once
#include <string.h>
#include <stdint.h>
#ifndef PROGMEM
#define PROGMEM
#endif
#define PGM_P const char *
#define PSTR(s) (s)
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#define pgm_read_byte_near(addr) pgm_read_byte(addr)
#define pgm_read_byte_far(addr) pgm_read_byte(addr)
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#define pgm_read_word_near(addr) pgm_read_word(addr)
#define pgm_read_dword(addr) (*(const uint32_t *)(addr))
#define pgm_read_float(addr) (*(const float *)(addr))
#define pgm_read_ptr(addr) (*(void *const *)(addr))
#define memcpy_P memcpy
#define memcmp_P memcmp
#define strcpy_P strcpy
#define strncpy_P strncpy
#define strcat_P strcat
#define strcmp_P strcmp
#define strncmp_P strncmp
#define strlen_P strlen
#define sprintf_P sprintf
#define snprintf_P snprintf
#define printf_P printf
