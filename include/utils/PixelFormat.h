#pragma once

#include <QString>

/**
 * Enumeration of the possible pixel formats the grabber can be set to
 */
enum class PixelFormat {
	YUYV,
	RGB24,
	XRGB,
	I420,
	NV12,
#ifdef HAVE_JPEG_DECODER
	MJPEG,
#endif
	NO_CHANGE
};

inline PixelFormat parsePixelFormat(const QString& pixelFormat)
{
	// convert to lower case
	QString format = pixelFormat.toLower();

	if (format.compare("yuyv") == 0)
	{
		return PixelFormat::YUYV;
	}	
	else if (format.compare("rgb24")  == 0)
	{
		return PixelFormat::RGB24;
	}
	else if (format.compare("xrgb")  == 0)
	{
		return PixelFormat::XRGB;
	}
	else if (format.compare("i420")  == 0)
	{
		return PixelFormat::I420;
	}
	else if (format.compare("nv12") == 0)
	{
		return PixelFormat::NV12;
	}
#ifdef HAVE_JPEG_DECODER
	else if (format.compare("mjpeg")  == 0)
	{
		return PixelFormat::MJPEG;
	}
#endif

	// return the default NO_CHANGE
	return PixelFormat::NO_CHANGE;
}

inline QString pixelFormatToString(const PixelFormat& pixelFormat)
{	

	if ( pixelFormat == PixelFormat::YUYV)
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
#ifdef HAVE_JPEG_DECODER
	else if (pixelFormat == PixelFormat::MJPEG)
	{
		return "mjpeg";
	}
#endif

	// return the default NO_CHANGE
	return "NO_CHANGE";
}
