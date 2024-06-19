#pragma once

#ifndef PCH_ENABLED
	#include <QString>
#endif

namespace hyperhdr
{
	enum Components
	{
		COMP_INVALID,
		COMP_ALL,
		COMP_HDR,
		COMP_SMOOTHING,
		COMP_BLACKBORDER,
		COMP_FORWARDER,
		COMP_VIDEOGRABBER,
		COMP_SYSTEMGRABBER,
		COMP_COLOR,
		COMP_IMAGE,
		COMP_EFFECT,
		COMP_LEDDEVICE,
		COMP_FLATBUFSERVER,
		COMP_RAWUDPSERVER,
		COMP_CEC,
		COMP_PROTOSERVER,
	};

	inline const char* componentToString(Components c)
	{
		switch (c)
		{
		case COMP_ALL:           return "HyperHDR";
		case COMP_HDR:			 return "HDR (global)";
		case COMP_SMOOTHING:     return "Smoothing";
		case COMP_BLACKBORDER:   return "Blackborder detector";
		case COMP_FORWARDER:     return "Json/Proto forwarder";
		case COMP_VIDEOGRABBER:  return "Video capture device";
		case COMP_SYSTEMGRABBER: return "System capture device";
		case COMP_COLOR:         return "Solid color";
		case COMP_EFFECT:        return "Effect";
		case COMP_IMAGE:         return "Image";
		case COMP_LEDDEVICE:     return "LED device";
		case COMP_FLATBUFSERVER: return "Image Receiver";
		case COMP_RAWUDPSERVER:  return "Raw RGB UDP Server";
		case COMP_CEC:           return "CEC";
		case COMP_PROTOSERVER:   return "Proto Server";
		default:                 return "";
		}
	}

	inline const char* componentToIdString(Components c)
	{
		switch (c)
		{
		case COMP_ALL:           return "ALL";
		case COMP_HDR:			 return "HDR";
		case COMP_SMOOTHING:     return "SMOOTHING";
		case COMP_BLACKBORDER:   return "BLACKBORDER";
		case COMP_FORWARDER:     return "FORWARDER";
		case COMP_VIDEOGRABBER:  return "VIDEOGRABBER";
		case COMP_SYSTEMGRABBER: return "SYSTEMGRABBER";
		case COMP_COLOR:         return "COLOR";
		case COMP_EFFECT:        return "EFFECT";
		case COMP_IMAGE:         return "IMAGE";
		case COMP_LEDDEVICE:     return "LEDDEVICE";
		case COMP_FLATBUFSERVER: return "FLATBUFSERVER";
		case COMP_RAWUDPSERVER:  return "RAWUDPSERVER";
		case COMP_CEC:           return "CEC";
		case COMP_PROTOSERVER:   return "PROTOSERVER";
		default:                 return "";
		}
	}

	inline Components stringToComponent(const QString& component)
	{
		const QString cmp = component.toUpper();
		if (cmp == "ALL")           return COMP_ALL;
		if (cmp == "HDR")			return COMP_HDR;
		if (cmp == "SMOOTHING")     return COMP_SMOOTHING;
		if (cmp == "BLACKBORDER")   return COMP_BLACKBORDER;
		if (cmp == "FORWARDER")     return COMP_FORWARDER;
		if (cmp == "VIDEOGRABBER")  return COMP_VIDEOGRABBER;
		if (cmp == "SYSTEMGRABBER") return COMP_SYSTEMGRABBER;
		if (cmp == "COLOR")         return COMP_COLOR;
		if (cmp == "EFFECT")        return COMP_EFFECT;
		if (cmp == "IMAGE")         return COMP_IMAGE;
		if (cmp == "LEDDEVICE")     return COMP_LEDDEVICE;
		if (cmp == "FLATBUFSERVER") return COMP_FLATBUFSERVER;
		if (cmp == "RAWUDPSERVER")  return COMP_RAWUDPSERVER;
		if (cmp == "CEC")           return COMP_CEC;
		if (cmp == "PROTOSERVER")   return COMP_PROTOSERVER;
		return COMP_INVALID;
	}

	enum SystemComponent { SUSPEND, LOCKER, MONITOR };
}
