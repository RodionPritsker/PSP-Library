#ifndef PSPLIBRARY_TEXT_H
#define PSPLIBRARY_TEXT_H

#include <stddef.h>
#include <stdint.h>

size_t pspl_text_width(const char *text, int scale);
void pspl_text_draw(float x, float y, const char *text, int scale, uint32_t color);
void pspl_text_draw_clipped(float x, float y, float width, const char *text,
                            int scale, uint32_t color);
void pspl_text_draw_centered(float x, float y, float width, const char *text,
                             int scale, uint32_t color);

#endif
