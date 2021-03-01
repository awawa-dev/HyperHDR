#pragma once
#include <QString>
#include <QJsonDocument>

///
/// @brief Provide util methods to work with SettingsManager class
///
namespace settings {
	// all available settings sections
	enum class type  {
		SNDEFFECT,
		BGEFFECT,
		FGEFFECT,
		BLACKBORDER,
		BOBLSERVER,
		COLOR,
		DEVICE,
		EFFECTS,
		NETFORWARD,
		SYSTEMCAPTURE,
		GENERAL,
		V4L2,
		JSONSERVER,
		LEDCONFIG,
		LEDS,
		LOGGER,
		SMOOTHING,
		WEBSERVER,
		INSTCAPTURE,
		NETWORK,
		FLATBUFSERVER,
		PROTOSERVER,
		INVALID
	};

	///
	/// @brief Convert settings::type to string representation
	/// @param  type  The settings::type from enum
	/// @return       The settings type as string
	///
	inline QString typeToString(type type)
	{
		switch (type)
		{
			case type::SNDEFFECT:     return "soundEffect";
			case type::BGEFFECT:      return "backgroundEffect";
			case type::FGEFFECT:      return "foregroundEffect";
			case type::BLACKBORDER:   return "blackborderdetector";
			case type::BOBLSERVER:    return "boblightServer";
			case type::COLOR:         return "color";
			case type::DEVICE:        return "device";
			case type::EFFECTS:       return "effects";
			case type::NETFORWARD:    return "forwarder";
			case type::SYSTEMCAPTURE: return "framegrabber";
			case type::GENERAL:       return "general";
			case type::V4L2:          return "grabberV4L2";
			case type::JSONSERVER:    return "jsonServer";
			case type::LEDCONFIG:     return "ledConfig";
			case type::LEDS:          return "leds";
			case type::LOGGER:        return "logger";
			case type::SMOOTHING:     return "smoothing";
			case type::WEBSERVER:     return "webConfig";
			case type::INSTCAPTURE:   return "instCapture";
			case type::NETWORK:       return "network";
			case type::FLATBUFSERVER: return "flatbufServer";
			case type::PROTOSERVER:   return "protoServer";
			default:            return "invalid";
		}
	}

	///
	/// @brief Convert string to settings::type representation
	/// @param  type  The string to convert
	/// @return       The settings type from enum
	///
	inline type stringToType(const QString& type)
	{
		if      (type == "soundEffect")          return type::SNDEFFECT;
		else if (type == "backgroundEffect")     return type::BGEFFECT;
		else if (type == "foregroundEffect")     return type::FGEFFECT;
		else if (type == "blackborderdetector")  return type::BLACKBORDER;
		else if (type == "boblightServer")       return type::BOBLSERVER;
		else if (type == "color")                return type::COLOR;
		else if (type == "device")               return type::DEVICE;
		else if (type == "effects")              return type::EFFECTS;
		else if (type == "forwarder")            return type::NETFORWARD;
		else if (type == "framegrabber")         return type::SYSTEMCAPTURE;
		else if (type == "general")              return type::GENERAL;
		else if (type == "grabberV4L2")          return type::V4L2;
		else if (type == "jsonServer")           return type::JSONSERVER;
		else if (type == "ledConfig")            return type::LEDCONFIG;
		else if (type == "leds")                 return type::LEDS;
		else if (type == "logger")               return type::LOGGER;
		else if (type == "smoothing")            return type::SMOOTHING;
		else if (type == "webConfig")            return type::WEBSERVER;
		else if (type == "instCapture")          return type::INSTCAPTURE;
		else if (type == "network")              return type::NETWORK;
		else if (type == "flatbufServer")        return type::FLATBUFSERVER;
		else if (type == "protoServer")          return type::PROTOSERVER;
		else                                     return type::INVALID;
	}
}
