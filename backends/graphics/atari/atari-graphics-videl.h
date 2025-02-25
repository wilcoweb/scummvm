/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef BACKENDS_GRAPHICS_ATARI_VIDEL_H
#define BACKENDS_GRAPHICS_ATARI_VIDEL_H

#include "backends/graphics/atari/atari-graphics.h"

#include "backends/graphics/atari/atari_c2p-asm.h"
#include "common/system.h"

class AtariVidelManager : public AtariGraphicsManager {
public:
	AtariVidelManager() {
		// using virtual methods so must be done here
		allocateSurfaces();
	}

	~AtariVidelManager() {
		// using virtual methods so must be done here
		freeSurfaces();
	}

	virtual const OSystem::GraphicsMode *getSupportedGraphicsModes() const override {
		static const OSystem::GraphicsMode graphicsModes[] = {
			{"single", "Single buffering", (int)GraphicsMode::SingleBuffering},
			{"triple", "Triple buffering", (int)GraphicsMode::TripleBuffering},
			{nullptr, nullptr, 0 }
		};
		return graphicsModes;
	}

	OSystem::TransactionError endGFXTransaction() override {
		int error = OSystem::TransactionError::kTransactionSuccess;

		if (_pendingState.mode == GraphicsMode::DirectRendering)
			error |= OSystem::TransactionError::kTransactionModeSwitchFailed;

		if (error != OSystem::TransactionError::kTransactionSuccess) {
			// all our errors are fatal but engine.cpp takes only this one seriously
			error |= OSystem::TransactionError::kTransactionSizeChangeFailed;
			return static_cast<OSystem::TransactionError>(error);
		}

		return AtariGraphicsManager::endGFXTransaction();
	}

private:
	void copyRectToSurface(Graphics::Surface &dstSurface, int dstBitsPerPixel, const Graphics::Surface &srcSurface,
						   int destX, int destY,
						   const Common::Rect &subRect) const override {
		// 'pChunkyEnd' is a delicate parameter: the c2p routine compares it to the address register
		// used for pixel reading; two common mistakes:
		// 1. (subRect.left, subRect.bottom) = beginning of the next line *including the offset*
		// 2. (subRect.right, subRect.bottom) = even worse, end of the *next* line, not current one
		const byte *pChunky    = (const byte *)srcSurface.getBasePtr(subRect.left, subRect.top);
		const byte *pChunkyEnd = (const byte *)srcSurface.getBasePtr(subRect.right, subRect.bottom-1);

        byte *pScreen = (byte *)dstSurface.getPixels() + destY * dstSurface.pitch + destX * dstBitsPerPixel/8;

		if (dstBitsPerPixel == 8) {
			if (srcSurface.pitch == subRect.width()) {
				if (srcSurface.pitch == dstSurface.pitch) {
					asm_c2p1x1_8(pChunky, pChunkyEnd, pScreen);
					return;
				} else if (srcSurface.pitch == dstSurface.pitch/2) {
                    asm_c2p1x1_8_tt(pChunky, pChunkyEnd, pScreen, dstSurface.pitch);
					return;
				}
			}

			asm_c2p1x1_8_rect(
				pChunky, pChunkyEnd,
				subRect.width(),
				srcSurface.pitch,
				pScreen,
                dstSurface.pitch);
		} else {
            if (srcSurface.pitch == subRect.width() && srcSurface.pitch/2 == dstSurface.pitch) {
				asm_c2p1x1_4(pChunky, pChunkyEnd, pScreen);
				return;
			}

			asm_c2p1x1_4_rect(
				pChunky, pChunkyEnd,
				subRect.width(),
				srcSurface.pitch,
				pScreen,
                dstSurface.pitch);
		}
	}

	void copyRectToSurfaceWithKey(Graphics::Surface &dstSurface, int dstBitsPerPixel, const Graphics::Surface &srcSurface,
								  int destX, int destY,
								  const Common::Rect &subRect, uint32 key,
								  const Graphics::Surface &bgSurface, const byte srcPalette[256*3]) const override {
		const Common::Rect backgroundRect = alignRect(destX, destY, subRect.width(), subRect.height());

		static Graphics::Surface cachedSurface;

		if (cachedSurface.w != backgroundRect.width()
				|| cachedSurface.h != backgroundRect.height()
				|| cachedSurface.format != bgSurface.format) {
			cachedSurface.create(
				backgroundRect.width(),
				backgroundRect.height(),
				bgSurface.format);
		}

		// copy background
		cachedSurface.copyRectToSurface(bgSurface, 0, 0, backgroundRect);

		// copy cursor
		convertRectToSurfaceWithKey(cachedSurface, srcSurface, destX - backgroundRect.left, 0, subRect, key, srcPalette);

		copyRectToSurface(
			dstSurface, dstBitsPerPixel, cachedSurface,
			backgroundRect.left, backgroundRect.top,
			Common::Rect(cachedSurface.w, cachedSurface.h));
	}

	Common::Rect alignRect(int x, int y, int w, int h) const override {
		return Common::Rect(x & 0xfff0, y, (x + w + 15) & 0xfff0, y + h);
	}
};

#endif
