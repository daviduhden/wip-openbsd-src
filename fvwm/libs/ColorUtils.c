/*
 * Copyright (c) 2025 David Uhden Collado <david@uhden.dev>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include <stdio.h>
#include <X11/Xproto.h>
#include "fvwmlib.h"

#define SCALE 65535.0
#define HALF_SCALE (SCALE * 0.5)

enum ColorChannel {
  CHANNEL_RED = 0,
  CHANNEL_GREEN = 1,
  CHANNEL_BLUE = 2
};

static void
color_mult(unsigned short *red, unsigned short *green,
           unsigned short *blue, double factor)
{
  double components[3];
  components[CHANNEL_RED] = (double)*red;
  components[CHANNEL_GREEN] = (double)*green;
  components[CHANNEL_BLUE] = (double)*blue;

  if (components[CHANNEL_RED] == components[CHANNEL_GREEN] &&
      components[CHANNEL_RED] == components[CHANNEL_BLUE]) {
    double level = components[CHANNEL_RED] * factor;
    if (level > SCALE) {
      level = SCALE;
    }
    *red = (unsigned short)level;
    *green = *red;
    *blue = *red;
    return;
  }

  int max_index = CHANNEL_RED;
  int min_index = CHANNEL_RED;
  for (int idx = CHANNEL_GREEN; idx <= CHANNEL_BLUE; ++idx) {
    if (components[idx] > components[max_index]) {
      max_index = idx;
    }
    if (components[idx] < components[min_index]) {
      min_index = idx;
    }
  }

  int mid_index = CHANNEL_RED + CHANNEL_GREEN + CHANNEL_BLUE - max_index - min_index;
  double max_value = components[max_index];
  double min_value = components[min_index];
  double span = max_value - min_value;
  double ratio = (components[mid_index] - min_value) / span;

  double lightness = 0.5 * (max_value + min_value);
  double extrema_sum = max_value + min_value;
  double saturation_denominator = (lightness <= HALF_SCALE)
    ? extrema_sum
    : (2.0 * SCALE - extrema_sum);
  double saturation = span / saturation_denominator;

  lightness *= factor;
  if (lightness > SCALE) {
    lightness = SCALE;
  }
  saturation *= factor;
  if (saturation > 1.0) {
    saturation = 1.0;
  }

  double new_max;
  if (lightness <= HALF_SCALE) {
    new_max = lightness * (1.0 + saturation);
  } else {
    new_max = saturation * SCALE + lightness - saturation * lightness;
  }

  double new_min = 2.0 * lightness - new_max;
  double new_span = new_max - new_min;
  double new_mid = new_min + new_span * ratio;

  double updated[3];
  updated[max_index] = new_max;
  updated[min_index] = new_min;
  updated[mid_index] = new_mid;

  *red = (unsigned short)updated[CHANNEL_RED];
  *green = (unsigned short)updated[CHANNEL_GREEN];
  *blue = (unsigned short)updated[CHANNEL_BLUE];
}

static Pixel
adjust_pixel_brightness(Pixel pixel, double factor)
{
  extern Colormap PictureCMap;
  extern Display *PictureSaveDisplay;
  XColor color_spec;

  color_spec.pixel = pixel;
  XQueryColor(PictureSaveDisplay, PictureCMap, &color_spec);
  color_mult(&color_spec.red, &color_spec.green, &color_spec.blue, factor);
  XAllocColor(PictureSaveDisplay, PictureCMap, &color_spec);

  return color_spec.pixel;
}

#define DARKNESS_FACTOR 0.5
Pixel
GetShadow(Pixel background)
{
  return adjust_pixel_brightness(background, DARKNESS_FACTOR);
}

#define BRIGHTNESS_FACTOR 1.4
Pixel
GetHilite(Pixel background)
{
  return adjust_pixel_brightness(background, BRIGHTNESS_FACTOR);
}
