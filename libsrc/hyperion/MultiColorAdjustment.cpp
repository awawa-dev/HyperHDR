// Hyperion includes
#include <utils/Logger.h>
#include <hyperion/MultiColorAdjustment.h>

MultiColorAdjustment::MultiColorAdjustment(int ledCnt)
	: _ledAdjustments(ledCnt, nullptr)
	, _log(Logger::getInstance("ADJUSTMENT"))
{
}

MultiColorAdjustment::~MultiColorAdjustment()
{
	// Clean up all the transforms
	for (ColorAdjustment * adjustment : _adjustment)
	{
		delete adjustment;
		// BUG: Calling pop_back while iterating is invalid
		_adjustment.pop_back();
	}
}

void MultiColorAdjustment::addAdjustment(ColorAdjustment * adjustment)
{
	_adjustmentIds.push_back(adjustment->_id);
	_adjustment.push_back(adjustment);
}

void MultiColorAdjustment::setAdjustmentForLed(const QString& id, int startLed, int endLed)
{
	// abort
	if(startLed > endLed)
	{
		Error(_log,"startLed > endLed -> %d > %d", startLed, endLed);
		return;
	}
	// catch wrong values
	if(endLed > static_cast<int>(_ledAdjustments.size()-1))
	{
		Warning(_log,"The color calibration 'LED index' field has LEDs specified which aren't part of your led layout");
		endLed = static_cast<int>(_ledAdjustments.size()-1);
	}

	// Get the identified adjustment (don't care if is nullptr)
	ColorAdjustment * adjustment = getAdjustment(id);

	//Debug(_log,"ColorAdjustment Profile [%s], startLed[%d], endLed[%d]", QSTRING_CSTR(id), startLed, endLed);
	for (int iLed=startLed; iLed<=endLed; ++iLed)
	{
		//Debug(_log,"_ledAdjustments [%d] -> [%p]", iLed, adjustment);
		_ledAdjustments[iLed] = adjustment;
	}
}

bool MultiColorAdjustment::verifyAdjustments() const
{
	bool ok = true;
	for (unsigned iLed=0; iLed<_ledAdjustments.size(); ++iLed)
	{
		ColorAdjustment * adjustment = _ledAdjustments[iLed];

		if (adjustment == nullptr)
		{
			Warning(_log, "No calibration set for led %d", iLed);
			ok = false;
		}
	}
	return ok;
}

QStringList MultiColorAdjustment::getAdjustmentIds() const
{
	return _adjustmentIds;
}

ColorAdjustment* MultiColorAdjustment::getAdjustment(const QString& id)
{
	// Iterate through the unique adjustments until we find the one with the given id
	for (ColorAdjustment* adjustment : _adjustment)
	{
		if (adjustment->_id == id)
		{
			return adjustment;
		}
	}

	// The ColorAdjustment was not found
	return nullptr;
}

void MultiColorAdjustment::setBacklightEnabled(bool enable)
{
	for (ColorAdjustment* adjustment : _adjustment)
	{
		adjustment->_rgbTransform.setBackLightEnabled(enable);
	}
}

void MultiColorAdjustment::applyAdjustment(std::vector<ColorRgb>& ledColors)
{
	const size_t itCnt = qMin(_ledAdjustments.size(), ledColors.size());
	for (size_t i=0; i<itCnt; ++i)
	{
		ColorAdjustment* adjustment = _ledAdjustments[i];
		if (adjustment == nullptr)
		{
			//std::cout << "MultiColorAdjustment::applyAdjustment() - No transform set for this led : " << i << std::endl;
			// No transform set for this led (do nothing)
			continue;
		}
		ColorRgb& color = ledColors[i];

		if (adjustment->_rgbTransform._classic_config)
		{
			uint8_t ored   = color.red;
			uint8_t ogreen = color.green;
			uint8_t oblue  = color.blue;
			
			adjustment->_rgbTransform.transformSatLum(ored,ogreen,oblue);
			adjustment->_rgbTransform.transform(ored,ogreen,oblue);
			
			color.red   = ored;
			color.green = ogreen;
			color.blue = oblue;
			
			int RR = adjustment->_rgbRedAdjustment.adjustmentR(color.red);
			int RG = color.red > color.green ? adjustment->_rgbRedAdjustment.adjustmentG(color.red-color.green) : 0;
			int RB = color.red > color.blue ? adjustment->_rgbRedAdjustment.adjustmentB(color.red-color.blue) : 0;
			
			int GR = color.green > color.red ? adjustment->_rgbGreenAdjustment.adjustmentR(color.green-color.red) : 0;
			int GG = adjustment->_rgbGreenAdjustment.adjustmentG(color.green);
			int GB = color.green > color.blue ? adjustment->_rgbGreenAdjustment.adjustmentB(color.green-color.blue) : 0;
			
			int BR = color.blue > color.red ? adjustment->_rgbBlueAdjustment.adjustmentR(color.blue-color.red) : 0;
			int BG = color.blue > color.green ? adjustment->_rgbBlueAdjustment.adjustmentG(color.blue-color.green) : 0;
			int BB = adjustment->_rgbBlueAdjustment.adjustmentB(color.blue);
					
			int ledR = RR + GR + BR;
			int maxR = (int)adjustment->_rgbRedAdjustment.getAdjustmentR();
			int ledG = RG + GG + BG;
			int maxG = (int)adjustment->_rgbGreenAdjustment.getAdjustmentG();
			int ledB = RB + GB + BB;
			int maxB = (int)adjustment->_rgbBlueAdjustment.getAdjustmentB();
			
			if (ledR > maxR)
				color.red = (uint8_t)maxR;
			else
				color.red = (uint8_t)ledR;
			
			if (ledG > maxG)
				color.green = (uint8_t)maxG;
			else
				color.green = (uint8_t)ledG;
			
			if (ledB > maxB)
				color.blue = (uint8_t)maxB;
			else
				color.blue = (uint8_t)ledB;
				
			// temperature
			color.red   = adjustment->_rgbRedAdjustment.correction(color.red);
			color.green = adjustment->_rgbGreenAdjustment.correction(color.green);
			color.blue = adjustment->_rgbBlueAdjustment.correction(color.blue);
		}
		else
		{
			uint8_t ored   = color.red;
			uint8_t ogreen = color.green;
			uint8_t oblue  = color.blue;
			uint8_t B_RGB = 0, B_CMY = 0, B_W = 0;

			adjustment->_rgbTransform.transform(ored,ogreen,oblue);
			adjustment->_rgbTransform.getBrightnessComponents(B_RGB, B_CMY, B_W);

			uint32_t nrng = (uint32_t) (255-ored)*(255-ogreen);
			uint32_t rng  = (uint32_t) (ored)    *(255-ogreen);
			uint32_t nrg  = (uint32_t) (255-ored)*(ogreen);
			uint32_t rg   = (uint32_t) (ored)    *(ogreen);

			uint8_t black   = nrng*(255-oblue)/65025;
			uint8_t red     = rng *(255-oblue)/65025;
			uint8_t green   = nrg *(255-oblue)/65025;
			uint8_t blue    = nrng*(oblue)    /65025;
			uint8_t cyan    = nrg *(oblue)    /65025;
			uint8_t magenta = rng *(oblue)    /65025;
			uint8_t yellow  = rg  *(255-oblue)/65025;
			uint8_t white   = rg  *(oblue)    /65025;

			uint8_t OR, OG, OB, RR, RG, RB, GR, GG, GB, BR, BG, BB;
			uint8_t CR, CG, CB, MR, MG, MB, YR, YG, YB, WR, WG, WB;

			adjustment->_rgbBlackAdjustment.apply  (black  , 255  , OR, OG, OB);
			adjustment->_rgbRedAdjustment.apply    (red    , B_RGB, RR, RG, RB);
			adjustment->_rgbGreenAdjustment.apply  (green  , B_RGB, GR, GG, GB);
			adjustment->_rgbBlueAdjustment.apply   (blue   , B_RGB, BR, BG, BB);
			adjustment->_rgbCyanAdjustment.apply   (cyan   , B_CMY, CR, CG, CB);
			adjustment->_rgbMagentaAdjustment.apply(magenta, B_CMY, MR, MG, MB);
			adjustment->_rgbYellowAdjustment.apply (yellow , B_CMY, YR, YG, YB);
			adjustment->_rgbWhiteAdjustment.apply  (white  , B_W  , WR, WG, WB);

			color.red   = OR + RR + GR + BR + CR + MR + YR + WR;
			color.green = OG + RG + GG + BG + CG + MG + YG + WG;
			color.blue  = OB + RB + GB + BB + CB + MB + YB + WB;
		}
	}
}
