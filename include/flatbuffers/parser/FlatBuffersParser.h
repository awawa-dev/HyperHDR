/* FlatBuffersParser.h
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
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
#include <string>

namespace FlatBuffersParser
{
	enum FLATBUFFERS_PACKAGE_TYPE { COLOR = 1, IMAGE, CLEAR, PRIORITY, ERROR };


	void* createFlatbuffersBuilder();
	void releaseFlatbuffersBuilder(void* builder);
	void clearFlatbuffersBuilder(void* builder);
	void encodeImageIntoFlatbuffers(void* builder, const uint8_t* rawImageMem, size_t rawImagesize, int imageWidth, int imageHeight, uint8_t** buffer, size_t* bufferSize);
	void encodeClearPriorityIntoFlatbuffers(void* builder, int priority, uint8_t** buffer, size_t* bufferSize);
	void encodeRegisterPriorityIntoFlatbuffers(void* builder, int priority, const char* name, uint8_t** buffer, size_t* bufferSize);
	void encodeColorIntoFlatbuffers(void* builder, int red, int green, int blue, int priority, int duration, uint8_t** buffer, size_t* bufferSize);
	bool verifyFlatbuffersReplyBuffer(const uint8_t* messageData, size_t messageSize, bool* _sent, bool* _registered, int* _priority);
	int decodeIncomingFlatbuffersFrame(void* builder, const uint8_t* messageData, size_t messageSize,
		uint8_t* red, uint8_t* green, uint8_t* blue,
		int* priority, std::string* clientDescription, int* duration,
		uint8_t** imageData, int* imageWidth, int* imageHeight, size_t* imageSize,
		uint8_t** buffer, size_t* bufferSize);
}
