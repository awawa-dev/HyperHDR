/* FlatBuffersParser.cpp
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

#include <flatbuffers/flatbuffers.h>
#include <flatbuffers/parser/FlatBuffersParser.h>
#include <flatbuffers/parser/hyperhdr_reply_generated.h>
#include <flatbuffers/parser/hyperhdr_request_generated.h>

void* FlatBuffersParser::createFlatbuffersBuilder()
{
	auto builder = new flatbuffers::FlatBufferBuilder();
	return builder;
}

void FlatBuffersParser::releaseFlatbuffersBuilder(void* builder)
{
	delete reinterpret_cast<flatbuffers::FlatBufferBuilder*>(builder);
}

void FlatBuffersParser::clearFlatbuffersBuilder(void* builder)
{
	reinterpret_cast<flatbuffers::FlatBufferBuilder*>(builder)->Clear();
}

void FlatBuffersParser::encodeImageIntoFlatbuffers(void* builder, const uint8_t* rawImageMem, size_t rawImagesize, int imageWidth, int imageHeight, uint8_t** buffer, size_t* bufferSize)
{
	auto _builder = reinterpret_cast<flatbuffers::FlatBufferBuilder*>(builder);
	auto imgData = _builder->CreateVector(rawImageMem, rawImagesize);
	auto rawImg = hyperhdrnet::CreateRawImage(*_builder, imgData, imageWidth, imageHeight);
	auto imageReq = hyperhdrnet::CreateImage(*_builder, hyperhdrnet::ImageType_RawImage, rawImg.Union(), -1);
	auto req = hyperhdrnet::CreateRequest(*_builder, hyperhdrnet::Command_Image, imageReq.Union());

	_builder->Finish(req);

	*buffer = _builder->GetBufferPointer();
	*bufferSize = _builder->GetSize();
}

void FlatBuffersParser::encodeClearPriorityIntoFlatbuffers(void* builder, int priority, uint8_t** buffer, size_t* bufferSize)
{
	auto _builder = reinterpret_cast<flatbuffers::FlatBufferBuilder*>(builder);
	auto clearReq = hyperhdrnet::CreateClear(*_builder, priority);
	auto req = hyperhdrnet::CreateRequest(*_builder, hyperhdrnet::Command_Clear, clearReq.Union());
	_builder->Finish(req);

	*buffer = _builder->GetBufferPointer();
	*bufferSize = _builder->GetSize();
}

void FlatBuffersParser::encodeRegisterPriorityIntoFlatbuffers(void* builder, int priority, const char* name, uint8_t** buffer, size_t* bufferSize)
{
	auto _builder = reinterpret_cast<flatbuffers::FlatBufferBuilder*>(builder);
	auto registerReq = hyperhdrnet::CreateRegister(*_builder, _builder->CreateString(name), priority);
	auto req = hyperhdrnet::CreateRequest(*_builder, hyperhdrnet::Command_Register, registerReq.Union());
	_builder->Finish(req);

	*buffer = _builder->GetBufferPointer();
	*bufferSize = _builder->GetSize();
}

void FlatBuffersParser::encodeColorIntoFlatbuffers(void* builder, int red, int green, int blue, int priority, int duration, uint8_t** buffer, size_t* bufferSize)
{
	auto _builder = reinterpret_cast<flatbuffers::FlatBufferBuilder*>(builder);
	auto colorReq = hyperhdrnet::CreateColor(*_builder, (red << 16) | (green << 8) | blue, duration);
	auto req = hyperhdrnet::CreateRequest(*_builder, hyperhdrnet::Command_Color, colorReq.Union());
	_builder->Finish(req);

	*buffer = _builder->GetBufferPointer();
	*bufferSize = _builder->GetSize();
}

bool FlatBuffersParser::verifyFlatbuffersReplyBuffer(const uint8_t* messageData, size_t messageSize, bool* _sent, bool* _registered, int* _priority)
{	
	flatbuffers::Verifier verifier(messageData, messageSize);

	if (hyperhdrnet::VerifyReplyBuffer(verifier))
	{
		auto reply = hyperhdrnet::GetReply(messageData);

		*_sent = false;

		if (!reply->error())
		{

			const auto registered = reply->registered();

			if (registered == -1 || registered != (*_priority))
				*_registered = false;
			else
				*_registered = true;

			return true;
		}
	}
	return false;
}

namespace
{
	const char* NO_IMAGE_DATA = "Do not have frame data";
	const char* INVALID_PACKAGE = "Received invalid packet";
	const char* CANT_PARSE = "Unable to parse message";
}

static void sendSuccessReply(flatbuffers::FlatBufferBuilder* _builder)
{
	auto reply = hyperhdrnet::CreateReplyDirect(*_builder);
	_builder->Finish(reply);
}

static void sendErrorReply(flatbuffers::FlatBufferBuilder* _builder, const char* error)
{
	auto reply = hyperhdrnet::CreateReplyDirect(*_builder, error);
	_builder->Finish(reply);
}

int FlatBuffersParser::decodeIncomingFlatbuffersFrame(void* builder, const uint8_t* messageData, size_t messageSize,									
									int* priority, std::string* clientDescription, int* duration,
									FlatbuffersTransientImage& image,
									FlatbuffersColor& color,
									uint8_t** buffer, size_t* bufferSize)
{
	int retType = FLATBUFFERS_PACKAGE_TYPE::ERROR;
	auto _builder = reinterpret_cast<flatbuffers::FlatBufferBuilder*>(builder);
	flatbuffers::Verifier verifier(messageData, messageSize);

	if (hyperhdrnet::VerifyRequestBuffer(verifier))
	{
		auto req = hyperhdrnet::GetRequest(messageData);
		const void* reqPtr;

		if ((reqPtr = req->command_as_Color()) != nullptr)
		{
			auto colorReq = static_cast<const hyperhdrnet::Color*>(reqPtr);
			const int32_t rgbData = colorReq->data();

			color.red = uint8_t((rgbData >> 16) & 0xff);
			color.green = uint8_t((rgbData >> 8) & 0xff);
			color.blue = uint8_t(rgbData & 0xff);
			*duration = colorReq->duration();

			retType = FLATBUFFERS_PACKAGE_TYPE::COLOR;
			sendSuccessReply(_builder);
		}
		else if ((reqPtr = req->command_as_Image()) != nullptr)
		{
			auto flatImage = static_cast<const hyperhdrnet::Image*>(reqPtr);
			*duration = flatImage->duration();

			const void* rawFlatImage;
			if ((rawFlatImage = flatImage->data_as_RawImage()) != nullptr)
			{
				const auto* img = static_cast<const hyperhdrnet::RawImage*>(rawFlatImage);
				const auto& imgD = img->data();

				image.format = FLATBUFFERS_IMAGE_FORMAT::RGB;
				image.firstPlane.data = const_cast<uint8_t*>(imgD->data());
				image.firstPlane.size = static_cast<int>(imgD->size());
				image.size = imgD->size();
				image.width = img->width();
				image.height = img->height();

				retType = FLATBUFFERS_PACKAGE_TYPE::IMAGE;
				auto reply = hyperhdrnet::CreateReplyDirect(*_builder, nullptr, -1, *priority);
				_builder->Finish(reply);
			}
			else if ((rawFlatImage = flatImage->data_as_NV12Image()) != nullptr)
			{
				const auto* img = static_cast<const hyperhdrnet::NV12Image*>(rawFlatImage);
				const auto& imgD = img->data_y();
				const auto& imgUvD = img->data_uv();

				image.format = FLATBUFFERS_IMAGE_FORMAT::NV12;
				image.firstPlane.data = const_cast<uint8_t*>(imgD->data());
				image.firstPlane.size = static_cast<int>(imgD->size());
				image.firstPlane.stride = img->stride_y();
				image.secondPlane.data = const_cast<uint8_t*>(imgUvD->data());
				image.secondPlane.size = static_cast<int>(imgUvD->size());
				image.secondPlane.stride = img->stride_uv();
				image.size = imgD->size() + imgUvD->size();
				image.width = img->width();
				image.height = img->height();

				retType = FLATBUFFERS_PACKAGE_TYPE::IMAGE;
				auto reply = hyperhdrnet::CreateReplyDirect(*_builder, nullptr, -1, *priority);
				_builder->Finish(reply);
			}
			else
				sendErrorReply(_builder, NO_IMAGE_DATA);
		}
		else if ((reqPtr = req->command_as_Clear()) != nullptr)
		{
			auto clear = static_cast<const hyperhdrnet::Clear*>(reqPtr);
			*priority = clear->priority();

			sendSuccessReply(_builder);
			retType = FLATBUFFERS_PACKAGE_TYPE::CLEAR;
		}
		else if ((reqPtr = req->command_as_Register()) != nullptr)
		{
			auto regPrio = static_cast<const hyperhdrnet::Register*>(reqPtr);
			*priority = regPrio->priority();
			*clientDescription = regPrio->origin()->c_str();

			auto reply = hyperhdrnet::CreateReplyDirect(*_builder, nullptr, -1, *priority);
			_builder->Finish(reply);

			retType = FLATBUFFERS_PACKAGE_TYPE::PRIORITY;
		}
		else
		{
			sendErrorReply(_builder, INVALID_PACKAGE);
			retType = FLATBUFFERS_PACKAGE_TYPE::ERROR;
		}
	}
	else
	{
		sendErrorReply(_builder, CANT_PARSE);
		retType = FLATBUFFERS_PACKAGE_TYPE::ERROR;
	}

	*buffer = _builder->GetBufferPointer();
	*bufferSize = _builder->GetSize();
	return retType;
}
