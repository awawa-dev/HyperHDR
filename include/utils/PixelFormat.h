#pragma once

#ifndef PCH_ENABLED
	#include <QString>
#endif

enum class PixelFormat {
	YUYV,
	RGB24,
	XRGB,
	I420,
	NV12,
	MJPEG,
	NO_CHANGE
};

inline PixelFormat parsePixelFormat(const QString& pixelFormat)
{
	QString format = pixelFormat.toLower();

	if (format.compare("yuyv") == 0)
	{
		return PixelFormat::YUYV;
	}
	else if (format.compare("rgb24") == 0)
	{
		return PixelFormat::RGB24;
	}
	else if (format.compare("xrgb") == 0)
	{
		return PixelFormat::XRGB;
	}
	else if (format.compare("i420") == 0)
	{
		return PixelFormat::I420;
	}
	else if (format.compare("nv12") == 0)
	{
		return PixelFormat::NV12;
	}
	else if (format.compare("mjpeg") == 0)
	{
		return PixelFormat::MJPEG;
	}

	return PixelFormat::NO_CHANGE;
}

inline QString pixelFormatToString(const PixelFormat& pixelFormat)
{

	if (pixelFormat == PixelFormat::YUYV)
	{
		return "yuyv";
	}
	else if (pixelFormat == PixelFormat::RGB24)
	{
		return "rgb24";
	}
	else if (pixelFormat == PixelFormat::XRGB)
	{
		return "xrgb";
	}
	else if (pixelFormat == PixelFormat::I420)
	{
		return "i420";
	}
	else if (pixelFormat == PixelFormat::NV12)
	{
		return "nv12";
	}
	else if (pixelFormat == PixelFormat::MJPEG)
	{
		return "mjpeg";
	}

	return "NO_CHANGE";
}
