#pragma once

#ifndef PCH_ENABLED
	#include <QTextStream>
	#include <QString>

	#include <cstdint>
	#include <iostream>
#endif

struct ColorRgb
{
	uint8_t red;
	uint8_t green;
	uint8_t blue;

	static const ColorRgb BLACK;
	static const ColorRgb RED;
	static const ColorRgb GREEN;
	static const ColorRgb BLUE;
	static const ColorRgb YELLOW;
	static const ColorRgb WHITE;

	ColorRgb() = default;

	ColorRgb(uint8_t _red, uint8_t _green, uint8_t _blue) :
		red(_red),
		green(_green),
		blue(_blue)
	{

	}

	int Red()
	{
		return red;
	}

	int Green()
	{
		return green;
	}

	int Blue()
	{
		return blue;
	}

	void getRGB(int& r, int& g, int& b)
	{
		r = red;
		g = green;
		b = blue;
	}

	void setRed(uint8_t r)
	{
		red = r;
	}

	void setGreen(uint8_t g)
	{
		green = g;
	}

	void setBlue(uint8_t b)
	{
		blue = b;
	}

	ColorRgb operator-(const ColorRgb& b) const
	{
		ColorRgb a(*this);
		a.red -= b.red;
		a.green -= b.green;
		a.blue -= b.blue;
		return a;
	}

	inline bool hasColor() const
	{
		return red || green || blue;
	}

	QString toQString()
	{
		return QString("(%1,%2,%3)").arg(red).arg(green).arg(blue);
	}
};

static_assert(sizeof(ColorRgb) == 3, "Incorrect size of ColorRgb");

inline std::ostream& operator<<(std::ostream& os, const ColorRgb& color)
{
	os << "{"
		<< static_cast<unsigned>(color.red) << ","
		<< static_cast<unsigned>(color.green) << ","
		<< static_cast<unsigned>(color.blue)
		<< "}";

	return os;
}

inline QTextStream& operator<<(QTextStream& os, const ColorRgb& color)
{
	os << "{"
		<< static_cast<unsigned>(color.red) << ","
		<< static_cast<unsigned>(color.green) << ","
		<< static_cast<unsigned>(color.blue)
		<< "}";

	return os;
}

inline bool operator==(const ColorRgb& lhs, const ColorRgb& rhs)
{
	return	lhs.red == rhs.red &&
		lhs.green == rhs.green &&
		lhs.blue == rhs.blue;
}

inline bool operator<(const ColorRgb& lhs, const ColorRgb& rhs)
{
	return	lhs.red < rhs.red&&
		lhs.green < rhs.green&&
		lhs.blue < rhs.blue;
}

inline bool operator!=(const ColorRgb& lhs, const ColorRgb& rhs)
{
	return !(lhs == rhs);
}

inline bool operator<=(const ColorRgb& lhs, const ColorRgb& rhs)
{
	return	lhs.red <= rhs.red &&
		lhs.green <= rhs.green &&
		lhs.blue <= rhs.blue;
}

inline bool operator>(const ColorRgb& lhs, const ColorRgb& rhs)
{
	return	lhs.red > rhs.red &&
		lhs.green > rhs.green &&
		lhs.blue > rhs.blue;
}

inline bool operator>=(const ColorRgb& lhs, const ColorRgb& rhs)
{
	return	lhs.red >= rhs.red &&
		lhs.green >= rhs.green &&
		lhs.blue >= rhs.blue;
}
