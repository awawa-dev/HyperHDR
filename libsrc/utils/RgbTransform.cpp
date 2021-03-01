#include <QtCore/qmath.h>
#include <utils/RgbTransform.h>
#include <utils/ColorSys.h>

RgbTransform::RgbTransform():
	_log(Logger::getInstance(QString("RgbTransform")))
{
	init(false,1.0,1.0, 1.0, 1.0, 1.0, 0.0, false, 100, 100);
}

RgbTransform::RgbTransform(
	bool classic_config,	
	double saturationGain,
	double luminanceGain,
	double gammaR, double gammaG, double gammaB, double backlightThreshold, bool backlightColored, uint8_t brightness, uint8_t brightnessCompensation):
	_log(Logger::getInstance(QString("RgbTransform")))
{
	init( classic_config, saturationGain, luminanceGain, 
	gammaR, gammaG, gammaB, backlightThreshold, backlightColored, brightness, brightnessCompensation);
}

void RgbTransform::init(
	bool classic_config,	
	double saturationGain,
	double luminanceGain,
	double gammaR, double gammaG, double gammaB, double backlightThreshold, bool backlightColored, uint8_t brightness, uint8_t brightnessCompensation)
{
	_classic_config = classic_config;
	_saturationGain = saturationGain;
	_luminanceGain = luminanceGain;
	_luminanceMinimum = 0;
	_brightness = brightness;
	_brightnessCompensation = brightnessCompensation;

	Debug(_log, "RGB transform classic_config: %i, saturationGain: %f, luminanceGain: %f", 
			_classic_config, _saturationGain, _luminanceGain);


	_backLightEnabled = true;
	setGamma(gammaR,gammaG,gammaB);
	setBacklightThreshold(backlightThreshold);
	setBacklightColored(backlightColored);
	setBrightness(brightness);
	setBrightnessCompensation(brightnessCompensation);
	initializeMapping();
}

inline uint8_t clamp(int x)
{
	return (x<0) ? 0 : ((x>255) ? 255 : uint8_t(x));
}

double RgbTransform::getGammaR() const
{
	return _gammaR;
}

double RgbTransform::getGammaG() const
{
	return _gammaG;
}

double RgbTransform::getGammaB() const
{
	return _gammaB;
}

void RgbTransform::setGamma(double gammaR, double gammaG, double gammaB)
{
	_gammaR = gammaR;
	_gammaG = (gammaG < 0.0) ? _gammaR : gammaG;
	_gammaB = (gammaB < 0.0) ? _gammaR : gammaB;
	initializeMapping();
}

void RgbTransform::setSaturationGain(double saturationGain)
{
	_saturationGain = saturationGain;
	Debug(_log, "set saturationGain to %f", _saturationGain);
}

void RgbTransform::setLuminanceGain(double luminanceGain)
{
	_luminanceGain = luminanceGain;
	Debug(_log, "set luminanceGain to %f", _luminanceGain);
}

double RgbTransform::getSaturationGain() const
{
	return _saturationGain;
}

double RgbTransform::getLuminanceGain() const
{
	return _luminanceGain;
}

bool RgbTransform::getClassicConfig() const
{
	return _classic_config;
}

void RgbTransform::initializeMapping()
{
	for (int i = 0; i < 256; ++i)
	{
		_mappingR[i] = qMin(qMax((int)(qPow(i / 255.0, _gammaR) * 255), 0), 255);
		_mappingG[i] = qMin(qMax((int)(qPow(i / 255.0, _gammaG) * 255), 0), 255);
		_mappingB[i] = qMin(qMax((int)(qPow(i / 255.0, _gammaB) * 255), 0), 255);
	}
}


int RgbTransform::getBacklightThreshold() const
{
	return _backlightThreshold;
}

void RgbTransform::setBacklightThreshold(int backlightThreshold)
{
	_backlightThreshold = backlightThreshold;
	
	int lowVal = (int) (_backlightThreshold);
	uint8_t rgb = clamp(lowVal), y, u, v;
	ColorSys::rgb2yuv(rgb, rgb, rgb, y, u, v);
	
	_sumBrightnessYLow   = y;
	_sumBrightnessRGBLow = rgb;
}

bool RgbTransform::getBacklightColored() const
{
	return _backlightColored;
}

void RgbTransform::setBacklightColored(bool backlightColored)
{
	_backlightColored = backlightColored;
}

bool RgbTransform::getBackLightEnabled() const
{
	return _backLightEnabled;
}

void RgbTransform::setBackLightEnabled(bool enable)
{
	_backLightEnabled = enable;
}

uint8_t RgbTransform::getBrightness() const
{
	return _brightness;
}

void RgbTransform::setBrightness(uint8_t brightness)
{
	_brightness = brightness;
	updateBrightnessComponents();
}

void RgbTransform::setBrightnessCompensation(uint8_t brightnessCompensation)
{
	_brightnessCompensation = brightnessCompensation;
	updateBrightnessComponents();
}

uint8_t RgbTransform::getBrightnessCompensation() const
{
	return _brightnessCompensation;
}

void RgbTransform::updateBrightnessComponents()
{
	double Fw   = _brightnessCompensation*2.0/100.0+1.0;
	double Fcmy = _brightnessCompensation/100.0+1.0;

	double B_in= 0;
	_brightness_rgb = 0;
	_brightness_cmy = 0;
	_brightness_w   = 0;

	if (_brightness > 0)
	{
		B_in = (_brightness<50)? -0.09*_brightness+7.5 : -0.04*_brightness+5.0;

		_brightness_rgb = std::ceil(qMin(255.0,255.0/B_in));
		_brightness_cmy = std::ceil(qMin(255.0,255.0/(B_in*Fcmy)));
		_brightness_w   = std::ceil(qMin(255.0,255.0/(B_in*Fw)));
	}
}

void RgbTransform::getBrightnessComponents(uint8_t & rgb, uint8_t & cmy, uint8_t & w) const
{
	rgb = _brightness_rgb;
	cmy = _brightness_cmy;
	w   = _brightness_w;
}

void RgbTransform::transform(uint8_t & red, uint8_t & green, uint8_t & blue)
{
	// apply gamma
	red   = _mappingR[red];
	green = _mappingG[green];
	blue  = _mappingB[blue];

	// apply brightnesss
	uint8_t y, u, v;
	ColorSys::rgb2yuv(red, green, blue, y, u, v);

	if ( _backLightEnabled && _sumBrightnessYLow>0 && y < _sumBrightnessYLow)
	{
		if (_backlightColored)
		{
			y = _sumBrightnessYLow;
			ColorSys::yuv2rgb(y, u, v, red, green, blue);
		}
		else
		{
			red   = _sumBrightnessRGBLow;
			green = red;
			blue  = red;
		}
	}
}

void RgbTransform::transformSatLum(uint8_t & red, uint8_t & green, uint8_t & blue) 
{
	if (_saturationGain != 1.0 || _luminanceGain != 1.0 || _luminanceMinimum != 0.0)
	{
		uint16_t hue;
		float saturation, luminance;
		rgb2hsl(red, green, blue, hue, saturation, luminance);
		
		float s = saturation * _saturationGain;
		if (s > 1.0f)
			saturation = 1.0f;
		else
			saturation = s;

		float l = luminance * _luminanceGain;
		if (l < _luminanceMinimum)
		{
			saturation = 0;
			l = _luminanceMinimum;
		}
		if (l > 1.0f)
			luminance = 1.0f;
		else 
			luminance = l;
				
		hsl2rgb(hue, saturation, luminance, red, green, blue);
	}
}

void RgbTransform::rgb2hsl(uint8_t red, uint8_t green, uint8_t blue, uint16_t & hue, float & saturation, float & luminance)
{
	float r = red / 255.0f;
	float g = green / 255.0f;
	float b = blue / 255.0f;	
	
	float rgbMin = r < g ? (r < b ? r : b) : (g < b ? g : b);
	float rgbMax = r > g ? (r > b ? r : b) : (g > b ? g : b);
	float diff = rgbMax - rgbMin;
		
	//luminance
	luminance = (rgbMin + rgbMax) / 2.0f;
	
	if (diff ==  0.0f) {
		saturation = 0.0f;
		hue        = 0;		
		return;
	}	
	
	//saturation
	if (luminance < 0.5f)
		saturation = diff / (rgbMin + rgbMax);
	else 
		saturation = diff / (2.0f - rgbMin - rgbMax);
	
	if (rgbMax == r)
	{
		// start from 360 to be sure that we won't assign a negative number to the unsigned hue value
		hue = 360 + 60 * (g - b) / (rgbMax - rgbMin);

		if (hue > 359)
			hue -= 360;
	}
	else if (rgbMax == g)
	{
		hue = 120 + 60 * (b - r) / (rgbMax - rgbMin);
	}
	else
	{
		hue = 240 + 60 * (r - g) / (rgbMax - rgbMin);
	}
		
}

void RgbTransform::hsl2rgb(uint16_t hue, float saturation, float luminance, uint8_t & red, uint8_t & green, uint8_t & blue) 
{
	if (saturation == 0.0f){
		red = (uint8_t)(luminance * 255.0f);
		green = (uint8_t)(luminance * 255.0f);
		blue = (uint8_t)(luminance * 255.0f);
		return;
	}
	
	float q;
	
	if (luminance < 0.5f)
		q = luminance * (1.0f + saturation);
	else
		q = (luminance + saturation) - (luminance * saturation);
	
	float p = (2.0f * luminance) - q;	
	float h = hue / 360.0f;
	
	float t[3];
	
	t[0] = h + (1.0f / 3.0f);
	t[1] = h;
	t[2] = h - (1.0f / 3.0f);
	
	for (int i = 0; i < 3; i++) {
		if (t[i] < 0.0f)
			t[i] += 1.0f;
		if (t[i] > 1.0f)
			t[i] -= 1.0f;
	}
	
	float out[3];
	
	for (int i = 0; i < 3; i++) {
		if (t[i] * 6.0f < 1.0f)
			out[i] = p + (q - p) * 6.0f * t[i];
		else if (t[i] * 2.0f < 1.0f)
			out[i] = q;
		else if (t[i] * 3.0f < 2.0f)
			out[i] = p + (q - p) * ((2.0f / 3.0f) - t[i]) * 6.0f;
		else out[i] = p;
	}
	
	//convert back to 0...255 range
	red = (uint8_t)(out[0] * 255.0f);
	green = (uint8_t)(out[1] * 255.0f);
	blue = (uint8_t)(out[2] * 255.0f);

}
