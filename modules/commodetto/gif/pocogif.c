/*
* Copyright (c) 2021  Moddable Tech, Inc.
*
*   This file is part of the Moddable SDK Runtime.
*
*   The Moddable SDK Runtime is free software: you can redistribute it and/or modify
*   it under the terms of the GNU Lesser General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   The Moddable SDK Runtime is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU Lesser General Public License for more details.
*
*   You should have received a copy of the GNU Lesser General Public License
*   along with the Moddable SDK Runtime.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#include "xsmc.h"
#include "xsHost.h"
#include "commodettoPoco.h"

#if (0 != kPocoRotation)
	#pragma error rotation unimplemented
#endif

typedef struct {
	PocoPixel 	*pixels;
	uint16_t	*palette;
	uint16_t	rowPixels;
	PocoPixel	keyColor;
} GIFRecord, *GIF;


static void doGIFGray16(Poco poco, uint8_t *refcon, PocoPixel *dst, PocoDimension w, PocoDimension h);
static void doGIF565LE(Poco poco, uint8_t *refcon, PocoPixel *dst, PocoDimension w, PocoDimension h);
static void doGIFCLUT256(Poco poco, uint8_t *refcon, PocoPixel *dst, PocoDimension w, PocoDimension h);
static void doGIFKey565LE(Poco poco, uint8_t *refcon, PocoPixel *dst, PocoDimension w, PocoDimension h);
static void doGIFKeyCLUT256(Poco poco, uint8_t *refcon, PocoPixel *dst, PocoDimension w, PocoDimension h);

void xs_poco_drawGIF(xsMachine *the)
{
	Poco poco = xsmcGetHostDataPoco(xsThis);
	GIFRecord gif;
	PocoCoordinate x, y;
	PocoDimension sx, sy, sw, sh;
	int16_t d;
	CommodettoBitmap cb;
	PocoPixel *pixels;
	uint8_t hasKeyColor = false;
	PocoPixel keyColor;
	uint16_t *palette = NULL;

	if (xsmcArgc >= 4) {
		hasKeyColor = xsmcTypeOf(xsArg(3));
		hasKeyColor = (xsIntegerType == hasKeyColor) || (xsNumberType == hasKeyColor);
		if (hasKeyColor)
			keyColor = (PocoPixel)xsmcToInteger(xsArg(3));
	}
	
	x = (PocoCoordinate)xsmcToInteger(xsArg(1)) + poco->xOrigin;
	y = (PocoCoordinate)xsmcToInteger(xsArg(2)) + poco->yOrigin;

	cb = xsmcGetHostChunk(xsArg(0));
	if (!cb->havePointer)
		xsUnknownError("unsupported");

	pixels = (PocoPixel *)cb->bits.data;
	if (kCommodettoBitmapCLUT256 == cb->format) {
		//@@ check signature
		palette = pixels + 2;
		pixels += pixels[1] + 2;
	}
	sx = 0, sy = 0;
	sw = cb->w, sh = cb->h;

	if ((x >= poco->xMax) || (y >= poco->yMax))
		return;

	if (x < poco->x) {
		d = poco->x - x;
		if (sw <= d)
			return;
		sx += d;
		sw -= d;
		x = poco->x;
	}

	if (y < poco->y) {
		d = poco->y - y;
		if (sh <= d)
			return;
		sy += d;
		sh -= d;
		y = poco->y;
	}

	if ((x + sw) > poco->xMax)
		sw = poco->xMax - x;

	if ((y + sh) > poco->yMax)
		sh = poco->yMax - y;

	if ((sx >= cb->w) || (sy >= cb->h) || ((sx + sw) > cb->w) || ((sy + sh) > cb->h) || !sw || !sh)
		return;

	if (kCommodettoBitmapRGB565LE == cb->format)
		pixels += (sy * cb->w) + sx;
	else if (kCommodettoBitmapCLUT256 == cb->format)
		pixels = (uint16_t *)(((sy * cb->w) + sx) + (uint8_t *)pixels);
	else if (kCommodettoBitmapGray16 == cb->format)
		pixels = (uint16_t *)((sy * ((cb->w + 1) >> 1) + (sx >> 1)) + (uint8_t *)pixels);
	gif.pixels = pixels;
	gif.palette = palette;
	gif.rowPixels = cb->w;

	if (kCommodettoBitmapGray16 == cb->format) {
		gif.keyColor = sx & 1;		// re-use for phase
		PocoDrawExternal(poco, doGIFGray16, (void *)&gif, sizeof(gif), x, y, sw, sh);
	}
	else if (hasKeyColor) {
		gif.keyColor = keyColor;
		PocoDrawExternal(poco, (kCommodettoBitmapRGB565LE == cb->format) ? doGIFKey565LE : doGIFKeyCLUT256, (void *)&gif, sizeof(gif), x, y, sw, sh);
	}
	else
		PocoDrawExternal(poco, (kCommodettoBitmapRGB565LE == cb->format) ? doGIF565LE : doGIFCLUT256, (void *)&gif, sizeof(gif), x, y, sw, sh);
}

void doGIF565LE(Poco poco, uint8_t *refcon, PocoPixel *dst, PocoDimension w, PocoDimension h)
{
	GIF gif = (GIF)refcon;
	PocoPixel *src = gif->pixels;
	PocoCoordinate srcRowPixels = gif->rowPixels;

	do {
		PocoPixel *s = src, *d = dst;
		int count = w;
		do {
			*d++ = *s++;
		} while (--count);

		dst = (PocoPixel *)(poco->rowBytes + (char *)dst);
		src += srcRowPixels;
	} while (--h);

	gif->pixels = src;
}

void doGIFKey565LE(Poco poco, uint8_t *refcon, PocoPixel *dst, PocoDimension w, PocoDimension h)
{
	GIF gif = (GIF)refcon;
	PocoPixel *src = gif->pixels;
	PocoCoordinate srcRowPixels = gif->rowPixels;
	PocoPixel keyColor = gif->keyColor;

	do {
		PocoPixel *s = src, *d = dst;
		int count = w;
		do {
			PocoPixel pixel = *s++;
			if (keyColor != pixel)
				*d = pixel;
			d++;
		} while (--count);

		dst = (PocoPixel *)(poco->rowBytes + (char *)dst);
		src += srcRowPixels;
	} while (--h);

	gif->pixels = src;
}

void doGIFCLUT256(Poco poco, uint8_t *refcon, PocoPixel *dst, PocoDimension w, PocoDimension h)
{
	GIF gif = (GIF)refcon;
	uint8_t *src = (uint8_t *)gif->pixels;
	uint16_t *palette = gif->palette;
	PocoCoordinate srcRowPixels = gif->rowPixels;

	do {
		uint8_t *s = src;
		PocoPixel *d = dst;
		int count = w;
		do {
			*d++ = palette[*s++];
		} while (--count);

		dst = (PocoPixel *)(poco->rowBytes + (char *)dst);
		src += srcRowPixels;
	} while (--h);

	gif->pixels = (uint16_t *)src;
}

void doGIFKeyCLUT256(Poco poco, uint8_t *refcon, PocoPixel *dst, PocoDimension w, PocoDimension h)
{
	GIF gif = (GIF)refcon;
	uint8_t *src = (uint8_t *)gif->pixels;
	uint16_t *palette = gif->palette;
	PocoCoordinate srcRowPixels = gif->rowPixels;
	uint8_t keyColor = (uint8_t)gif->keyColor;

	do {
		uint8_t *s = src;
		PocoPixel *d = dst;
		int count = w;
		do {
			uint8_t pixel = *s++;
			if (keyColor != pixel)
				*d = palette[pixel];
			d++;
		} while (--count);

		dst = (PocoPixel *)(poco->rowBytes + (char *)dst);
		src += srcRowPixels;
	} while (--h);

	gif->pixels = (uint16_t *)src;
}

#define PocoMakeRGB565FromGray4(g) (((g >> 4) | (g << 1)) << 11) | (((g >> 3) | (g << 2)) << 5) | ((g >> 4) | (g << 1))
static const uint16_t Gray4ToRGB565[16] = {
	PocoMakeRGB565FromGray4(15),
	PocoMakeRGB565FromGray4(14),
	PocoMakeRGB565FromGray4(13),
	PocoMakeRGB565FromGray4(12),
	PocoMakeRGB565FromGray4(11),
	PocoMakeRGB565FromGray4(10),
	PocoMakeRGB565FromGray4(9),
	PocoMakeRGB565FromGray4(8),
	PocoMakeRGB565FromGray4(7),
	PocoMakeRGB565FromGray4(6),
	PocoMakeRGB565FromGray4(5),
	PocoMakeRGB565FromGray4(4),
	PocoMakeRGB565FromGray4(3),
	PocoMakeRGB565FromGray4(2),
	PocoMakeRGB565FromGray4(1),
	PocoMakeRGB565FromGray4(0)
};

void doGIFGray16(Poco poco, uint8_t *refcon, PocoPixel *dst, PocoDimension w, PocoDimension h)
{
	GIF gif = (GIF)refcon;
	uint8_t *src = (uint8_t *)gif->pixels;
	PocoCoordinate srcRowPixels = (gif->rowPixels + 1) >> 1;

	do {
		uint8_t *s = src;
		PocoPixel *d = dst;
		int count = w;

		if (gif->keyColor) {		// using keyColor for phase
			*d++ = Gray4ToRGB565[*s++ & 0x0F];
			count--;
		}

		while (count >= 2) {
			uint8_t pixels = *s++;
			count -= 2;
			d[0] = Gray4ToRGB565[(pixels & 0xF0) >> 4];
			d[1] = Gray4ToRGB565[pixels & 0x0F];
			d += 2;
		}

		if (count)
			*d = Gray4ToRGB565[(*s++ & 0xF0) >> 4];

		dst = (PocoPixel *)(poco->rowBytes + (char *)dst);
		src += srcRowPixels;
	} while (--h);

	gif->pixels = (uint16_t *)src;
}
