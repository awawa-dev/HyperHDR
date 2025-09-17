/* VectorizedDecoders.h
*
*  MIT License
*
*  Copyright (c) 2020-2025 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
*/

#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <array>

namespace VECTOR_RGB {
	template<bool isByte3Input, bool quarterMode, bool toneMapping>
	static inline void decode(
		uint8_t* currentSourceRGB,
		const uint8_t* lutBuffer,
		uint8_t* dest,
		const uint8_t* destEnd
	)
	{
		constexpr int stride = (isByte3Input ? 3 : 4) * (quarterMode ? 2 : 1);
		while (dest < destEnd) {
			if constexpr (toneMapping)
			{
				const uint32_t rgb = *((uint32_t*)currentSourceRGB);
				const uint32_t b = rgb & 0xFF;
				const uint32_t g = (rgb >> 8) & 0xFF;
				const uint32_t r = (rgb >> 16) & 0xFF;
				*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(
														&lutBuffer[(r | (g << 8) | (b << 16)) * 3]);
				dest += 3;
			}
			else
			{
				*(dest++) = currentSourceRGB[2];
				*(dest++) = currentSourceRGB[1];
				*(dest++) = currentSourceRGB[0];
			}
			currentSourceRGB += stride;
		}
	}
}

namespace VECTOR_UYVY {

	template<bool quarterMode>
	static inline void process(
		uint32_t* currentSourceYUV,
		const uint8_t* lutBuffer,
		uint8_t* dest,
		const uint8_t* destEnd
	)
	{
		while (dest < destEnd) {
			const uint32_t uyvy0 = *(currentSourceYUV++);
			const uint32_t uyvy1 = *(currentSourceYUV++);

			const uint32_t u0 = uyvy0 & 0xFF;
			const uint32_t v0 = (uyvy0 >> 16) & 0xFF;
			const uint32_t y00 = (uyvy0 >> 8) & 0xFF;			
			const uint32_t y01 = (uyvy0 >> 24);
			const uint32_t base0 = (u0 << 8) | (v0 << 16);

			const uint32_t u1 = uyvy1 & 0xFF;
			const uint32_t v1 = (uyvy1 >> 16) & 0xFF;
			const uint32_t y10 = (uyvy1 >> 8) & 0xFF;
			const uint32_t y11 = (uyvy1 >> 24);
			const uint32_t base1 = (u1 << 8) | (v1 << 16);

			*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base0 | y00) * 3]); dest += 3;
			if constexpr (!quarterMode)
				{*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base0 | y01) * 3]); dest += 3;}

			*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base1 | y10) * 3]); dest += 3;
			if constexpr (!quarterMode)
				{*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base1 | y11) * 3]); dest += 3;}
		}
	}

}

namespace VECTOR_YUYV{

	template<bool quarterMode>
	static inline void process(
		uint32_t* currentSourceYUV,
		const uint8_t* lutBuffer,
		uint8_t* dest,
		const uint8_t* destEnd
	)
	{
		while (dest < destEnd) {
			const uint32_t yuyv0 = *(currentSourceYUV++);
			const uint32_t yuyv1 = *(currentSourceYUV++);

			const uint32_t u0 = (yuyv0 >> 8) & 0xFF;
			const uint32_t v0 = (yuyv0 >> 24);
			const uint32_t y00 = (yuyv0) & 0xFF;
			const uint32_t y01 = (yuyv0 >> 16) & 0xFF;
			const uint32_t base0 = (u0 << 8) | (v0 << 16);

			const uint32_t u1 = (yuyv1 >> 8) & 0xFF;
			const uint32_t v1 = (yuyv1 >> 24);
			const uint32_t y10 = (yuyv1) & 0xFF;
			const uint32_t y11 = (yuyv1 >> 16) & 0xFF;
			const uint32_t base1 = (u1 << 8) | (v1 << 16);

			*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base0 | y00) * 3]); dest += 3;
			if constexpr (!quarterMode)
				{*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base0 | y01) * 3]); dest += 3;}

			*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base1 | y10) * 3]); dest += 3;
			if constexpr (!quarterMode)
				{*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base1 | y11) * 3]); dest += 3;}
		}
	}
}

namespace VECTOR_I420 {

	template<bool quarterMode>
	static inline void process(
		uint32_t* currentSourceY,
		uint16_t* currentSourceU,
		uint16_t* currentSourceV,
		const uint8_t* lutBuffer,
		uint8_t* dest,
		const uint8_t* destEnd
	)
	{
		while (dest < destEnd) {
			const uint32_t y = *(currentSourceY++);
			const uint16_t u = *(currentSourceU++);
			const uint16_t v = *(currentSourceV++);

			const uint32_t u0 = u & 0xFF;
			const uint32_t v0 = v & 0xFF;
			const uint32_t y00 = y & 0xFF;
			const uint32_t y01 = (y >> 8) & 0xFF;
			const uint32_t base0 = (u0 << 8) | (v0 << 16);
			
			const uint32_t u1 = (u >> 8) & 0xFF;
			const uint32_t v1 = (v >> 8) & 0xFF;
			const uint32_t y10 = (y >> 16) & 0xFF;
			const uint32_t y11 = (y >> 24) ;
			const uint32_t base1 = (u1 << 8) | (v1 << 16);
			
			*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base0 | y00) * 3]); dest += 3;
			if constexpr (!quarterMode)
				{*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base0 | y01) * 3]); dest += 3;}

			*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base1 | y10) * 3]); dest += 3;
			if constexpr (!quarterMode)
				{*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base1 | y11) * 3]); dest += 3;}
		}
	}	
}


namespace VECTOR_NV12 {

	template<bool quarterMode>
	static inline void process(
		uint32_t* currentSourceY,
		uint32_t* currentSourceUV,
		const uint8_t* lutBuffer,
		uint8_t* dest,
		const uint8_t* destEnd
	)
	{
		while (dest < destEnd) {
			const uint32_t y = *(currentSourceY++);
			const uint32_t uv = *(currentSourceUV++);

			const uint32_t y00 = (y & 0xFF);
			const uint32_t y01 = (y >> 8) & 0xFF;
			const uint32_t base0 = (uv & 0x00'00'FF'FF) << 8;

			const uint32_t y10 = (y >> 16) & 0xFF;
			const uint32_t y11 = (y >> 24);
			const uint32_t base1 = (uv & 0xFF'FF'00'00) >> 8;

			*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base0 | y00) * 3]); dest += 3;
			if constexpr (!quarterMode)
				{*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base0 | y01) * 3]); dest += 3;}

			*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base1 | y10) * 3]); dest += 3;
			if constexpr (!quarterMode)
				{*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base1 | y11) * 3]); dest += 3;}
		}	
	}
}

namespace VECTOR_P010 {

	template<bool quarterMode>
	static inline void process(
		uint32_t* currentSourceY,
		uint32_t* currentSourceUV,
		const uint8_t* lutBuffer,
		uint8_t* dest,
		const uint8_t* destEnd
	)
	{
		while (dest < destEnd) {
			const uint32_t y0 = *(currentSourceY++);
			const uint32_t y1 = *(currentSourceY++);
			const uint32_t uv0 = *(currentSourceUV++);
			const uint32_t uv1 = *(currentSourceUV++);

			const uint32_t u0 = (uv0 >> 8) & 0xFF;
			const uint32_t v0 = (uv0 >> 24);
			const uint32_t y00 = (y0 >> 8) & 0xFF;
			const uint32_t y01 = (y0 >> 24);
			const uint32_t base0 = (u0 << 8) | (v0 << 16);

			const uint32_t u1 = (uv1 >> 8) & 0xFF;
			const uint32_t v1 = (uv1 >> 24);
			const uint32_t y10 = (y1 >> 8) & 0xFF;
			const uint32_t y11 = (y1 >> 24);
			const uint32_t base1 = (u1 << 8) | (v1 << 16);

			*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base0 | y00) * 3]); dest += 3;
			if constexpr (!quarterMode)
				{*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base0 | y01) * 3]); dest += 3;}

			*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base1 | y10) * 3]); dest += 3;
			if constexpr (!quarterMode)
				{*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base1 | y11) * 3]); dest += 3;}

		}
	}
	
	template<bool quarterMode>
	static inline void processlWithToneMapping(
		uint64_t* currentSourceY,
		uint64_t* currentSourceUV,
		const uint8_t* lutBuffer,
		uint8_t* dest,
		const uint8_t* destEnd,
		const std::array<uint8_t, 1024>& lutP010_y,
		const std::array<uint8_t, 1024>& lutP010_uv
	)
	{
		while (dest < destEnd)
		{
			const uint64_t y = *(currentSourceY++);
			const uint64_t uv = *(currentSourceUV++);

			const uint32_t u0 = lutP010_uv[(uv & 0xFF'FF) >> 6];
			const uint32_t v0 = lutP010_uv[(uv & 0xFF'FF'00'00) >> (16 + 6)];
			const uint32_t y00 = lutP010_y[(y & 0xFF'FF) >> 6];
			const uint32_t y01 = lutP010_y[(y & 0xFF'FF'00'00) >> (16 + 6)];
			const uint32_t base0 = (u0 << 8) | (v0 << 16);

			const uint32_t u1 = lutP010_uv[(uv & 0xFF'FF'00'00'00'00) >> (6 + 32)];
			const uint32_t v1 = lutP010_uv[uv >> (16 + 6 + 32)];
			const uint32_t y10 = lutP010_y[(y & 0xFF'FF'00'00'00'00) >> (6 + 32)];
			const uint32_t y11 = lutP010_y[y >> (16 + 6 + 32)];
			const uint32_t base1 = (u1 << 8) | (v1 << 16);

			*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base0 | y00) * 3]); dest += 3;
			if constexpr (!quarterMode)
				{*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base0 | y01) * 3]); dest += 3;}

			*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base1 | y10) * 3]); dest += 3;
			if constexpr (!quarterMode)
				{*reinterpret_cast<uint32_t*>(dest) = *reinterpret_cast<const uint32_t*>(&lutBuffer[(base1 | y11) * 3]); dest += 3;}
		}
	}
}


