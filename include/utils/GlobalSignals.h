#pragma once

// util
#include <utils/Image.h>
#include <utils/ColorRgb.h>
#include <utils/Components.h>

// qt
#include <QObject>

///
/// Singleton instance for simple signal sharing across threads, should be never used with Qt:DirectConnection!
///
class GlobalSignals : public QObject
{
	Q_OBJECT
public:
	static GlobalSignals* getInstance()
	{
		static GlobalSignals instance;
		return &instance;
	}

private:
	GlobalSignals() = default;

public:
	GlobalSignals(GlobalSignals const&) = delete;
	void operator=(GlobalSignals const&) = delete;

signals:
	///////////////////////////////////////
	////////////////// TO /////////////////
	///////////////////////////////////////

	///
	/// @brief PIPE SystemCapture images from GrabberWrapper to HyperHDR class
	/// @param name   The name of the platform capture that is currently active
	/// @param image  The prepared image
	///
	void setSystemImage(const QString& name, const Image<ColorRgb>& image);

	///
	/// @brief PIPE video images from video grabber over HyperHDRDaemon to HyperHDR class
	/// @param name   The name of the grabber capture (path) that is currently active
	/// @param image  The prepared image
	///
	void setVideoImage(const QString& name, const Image<ColorRgb>& image);

	///
	/// @brief PIPE the register command for a new global input over HyperHDRDaemon to HyperHDR class
	/// @param[in] priority    The priority of the channel
	/// @param[in] component   The component of the channel
	/// @param[in] origin      Who set the channel (CustomString@IP)
	/// @param[in] owner       Specific owner string, might be empty
	/// @param[in] smooth_cfg  The smooth id to use
	///
	void registerGlobalInput(int priority, hyperhdr::Components component, const QString& origin = "External", const QString& owner = "", unsigned smooth_cfg = 0);

	///
	/// @brief PIPE the clear command for the global priority channel over HyperHDRDaemon to HyperHDR class
	/// @param[in] priority       The priority channel (-1 to clear all possible priorities)
	/// @param[in] forceclearAll  Force the clear
	///
	void clearGlobalInput(int priority, bool forceClearAll = false);

	///
	/// @brief PIPE external images over HyperHDRDaemon to HyperHDR class
	/// @param[in] priority    The priority of the channel
	/// @param     image       The prepared image
	/// @param[in] timeout_ms  The timeout in milliseconds
	/// @param     clearEffect Should be true when NOT called from an effect
	///
	void setGlobalImage(int priority, const Image<ColorRgb>& image, int timeout_ms, bool clearEffect = true);

	///
	/// @brief PIPE external color message over HyperHDRDaemon to HyperHDR class
	/// @param[in] priority    The priority of the channel
	/// @param     image       The prepared color
	/// @param[in] timeout_ms  The timeout in milliseconds
	/// @param[in] origin      The setter
	/// @param     clearEffect Should be true when NOT called from an effect
	///
	void setGlobalColor(int priority, const std::vector<ColorRgb>& ledColor, int timeout_ms, const QString& origin = "External", bool clearEffects = true);

	///////////////////////////////////////
	///////////////// FROM ////////////////
	///////////////////////////////////////

	///
	/// @brief PIPE a registration request from the HyperHDR class to the priority channel
	/// @param[in] priority    The priority channel
	///
	void globalRegRequired(int priority);

	///
	/// @brief Tell v4l2/screen capture the listener state
	/// @param component  The component to handle
	/// @param hyperhdrInd The HyperHDR instance index as identifier
	/// @param listen  True when listening, else false
	///
	void requestSource(hyperhdr::Components component, int hyperHdrInd, bool listen);

};
