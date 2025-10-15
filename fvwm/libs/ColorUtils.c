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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdbool.h>

#ifndef U16_MAX
#define U16_MAX 65535
#endif

/* External handles expected to be initialized elsewhere (same contract as original). */
extern Colormap PictureCMap;
extern Display *PictureSaveDisplay;

/* ---------- Small helpers ---------- */

static inline double clamp01(double x) {
  if (x < 0.0) return 0.0;
  if (x > 1.0) return 1.0;
  return x;
}

static inline unsigned short to_u16(double x01) {
  /* Round to nearest and clamp to [0, 65535]. */
  double v = x01 * (double)U16_MAX;
  if (v < 0.0) v = 0.0;
  if (v > (double)U16_MAX) v = (double)U16_MAX;
  return (unsigned short)(v + 0.5);
}

/* HSL helper used for RGB reconstruction */
static double hue2rgb(double p, double q, double t) {
  /* Wrap t to [0,1] */
  if (t < 0.0) t += 1.0;
  if (t > 1.0) t -= 1.0;

  if (t < 1.0/6.0) return p + (q - p) * 6.0 * t;
  if (t < 1.0/2.0) return q;
  if (t < 2.0/3.0) return p + (q - p) * (2.0/3.0 - t) * 6.0;
  return p;
}

/* ---------- Core color adjustment in HSL space ---------- */

static void apply_hsl_scale(unsigned short *r16,
                            unsigned short *g16,
                            unsigned short *b16,
                            double k)
{
  /* Normalize to [0,1] */
  const double r = (double)(*r16) / (double)U16_MAX;
  const double g = (double)(*g16) / (double)U16_MAX;
  const double b = (double)(*b16) / (double)U16_MAX;

  /* Compute HSL */
  const double maxc = (r > g ? (r > b ? r : b) : (g > b ? g : b));
  const double minc = (r < g ? (r < b ? r : b) : (g < b ? g : b));
  const double delta = maxc - minc;
  double h = 0.0; /* hue in [0,1], undefined when s == 0; we keep 0 */
  double l = 0.5 * (maxc + minc);
  double s = 0.0;

  if (delta > 0.0) {
    /* Saturation for HSL */
    s = delta / (1.0 - fabs(2.0*l - 1.0));

    /* Hue (sector-based) */
    if (maxc == r) {
      h = (g - b) / delta;
      if (g < b) h += 6.0;
    } else if (maxc == g) {
      h = (b - r) / delta + 2.0;
    } else { /* maxc == b */
      h = (r - g) / delta + 4.0;
    }
    h /= 6.0; /* normalize to [0,1] */
  }

  /* Scale Lightness and Saturation, then clamp */
  l = clamp01(l * k);
  s = clamp01(s * k);

  /* Convert back to RGB */
  double rn, gn, bn;
  if (s == 0.0) {
    /* Achromatic: keep it gray with new lightness */
    rn = gn = bn = l;
  } else {
    const double q = (l < 0.5) ? (l * (1.0 + s)) : (l + s - l*s);
    const double p = 2.0*l - q;
    rn = hue2rgb(p, q, h + 1.0/3.0);
    gn = hue2rgb(p, q, h);
    bn = hue2rgb(p, q, h - 1.0/3.0);
  }

  *r16 = to_u16(rn);
  *g16 = to_u16(gn);
  *b16 = to_u16(bn);
}

/* ---------- Public surface identical to the classic helpers ---------- */

static Pixel adjust_pixel_brightness(Pixel pixel, double factor)
{
  /* Look up current RGB for the pixel, apply scaling in HSL, then allocate the result. */
  XColor xc;
  xc.pixel = pixel;
  XQueryColor(PictureSaveDisplay, PictureCMap, &xc);

  apply_hsl_scale(&xc.red, &xc.green, &xc.blue, factor);

  /* Try to allocate the adjusted color. If allocation fails, return the input pixel. */
  if (XAllocColor(PictureSaveDisplay, PictureCMap, &xc) == 0) {
    return pixel;
  }
  return xc.pixel;
}

/* Tunable factors to mimic the classic 3D relief: shadow vs highlight. */
#define DARKNESS_FACTOR    0.5    /* similar to the deeper shadow look */
#define BRIGHTNESS_FACTOR  1.4    /* similar to pleasant highlight */

Pixel GetShadow(Pixel background) {
  return adjust_pixel_brightness(background, DARKNESS_FACTOR);
}

Pixel GetHilite(Pixel background) {
  return adjust_pixel_brightness(background, BRIGHTNESS_FACTOR);
}