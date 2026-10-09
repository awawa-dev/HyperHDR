#include <blackborder/BlackBorderDetector.h>
#include <iostream>
#include <string>

namespace
{
	unsigned checks = 0;
	unsigned failures = 0;

	// Paint rectangular subtitles and picture details into synthetic capture frames.
	void paint(Image<ColorRgb>& image, unsigned left, unsigned top, unsigned right, unsigned bottom, const ColorRgb& color)
	{
		for (unsigned y = top; y < bottom; ++y)
			for (unsigned x = left; x < right; ++x)
				image(x, y) = color;
	}

	// Build a frame with independent top/bottom bars, as asymmetric images must
	// retain picture content when represented by the existing symmetric crop.
	Image<ColorRgb> letterbox(unsigned top = 16, unsigned bottom = 16)
	{
		Image<ColorRgb> image(160, 90);
		image.clear();
		paint(image, 0, top, image.width(), image.height() - bottom, {32, 96, 160});
		return image;
	}

	// Use explicit checks so Release builds exercise assertions too.
	void expect(const std::string& name, const hyperhdr::BlackBorder& actual, int horizontal, bool unknown = false)
	{
		++checks;
		if (actual.unknown != unknown || actual.horizontalSize != horizontal || actual.verticalSize != 0)
		{
			++failures;
			std::cerr << name << ": got (" << actual.unknown << ", " << actual.horizontalSize << ", " << actual.verticalSize
				<< "), expected (" << unknown << ", " << horizontal << ", 0)\n";
		}
	}
}

int main()
{
	hyperhdr::BlackBorderDetector detector(0.05);
	auto clean = letterbox();
	expect("clean letterbox", detector.process_subtitle(clean), 16);
	expect("existing letterbox mode", detector.process_letterbox(clean), 16);

	// Reproduce #821: wide captions cross both of Letterbox's 25%/75% scans.
	auto captions = letterbox();
	paint(captions, 30, 79, 130, 82, ColorRgb::WHITE);
	paint(captions, 25, 84, 135, 87, ColorRgb::WHITE);
	expect("legacy scans see captions", detector.process_letterbox(captions), 3);
	expect("wide two-line subtitles", detector.process_subtitle(captions), 16);

	auto inPicture = letterbox();
	paint(inPicture, 30, 68, 130, 73, ColorRgb::WHITE);
	expect("subtitles within the picture", detector.process_subtitle(inPicture), 16);

	// Known limits: text reaching a bottom corner sample, or an overlay in the top bar,
	// is treated as picture and makes the crop smaller.
	auto offCenter = letterbox();
	paint(offCenter, 30, 84, 155, 87, ColorRgb::WHITE);
	expect("caption reaching a bottom corner sample", detector.process_subtitle(offCenter), 3);
	auto topOverlay = letterbox();
	paint(topOverlay, 60, 3, 100, 6, ColorRgb::WHITE);
	expect("overlay in the top bar", detector.process_subtitle(topOverlay), 3);

	expect("full-screen picture", detector.process_subtitle(letterbox(0, 0)), 0);
	expect("top bar is smaller", detector.process_subtitle(letterbox(8, 16)), 8);
	expect("bottom bar is smaller", detector.process_subtitle(letterbox(16, 8)), 8);
	expect("picture resumes after subtitles", detector.process_subtitle(clean), 16);
	expect("aspect ratio changes", detector.process_subtitle(letterbox(6, 6)), 6);
	expect("one-pixel bars", detector.process_subtitle(letterbox(1, 1)), 1);

	auto brightEdge = letterbox();
	paint(brightEdge, 0, 0, brightEdge.width(), 1, ColorRgb::WHITE);
	expect("white picture content is not filtered", detector.process_subtitle(brightEdge), 0);

	Image<ColorRgb> dark(160, 90);
	dark.clear();
	expect("black frame has unknown borders", detector.process_subtitle(dark), -1, true);
	expect("bars outside the search range", detector.process_subtitle(letterbox(31, 31)), -1, true);

	auto greyBars = letterbox();
	paint(greyBars, 0, 0, 160, 16, {12, 12, 12});
	paint(greyBars, 0, 74, 160, 90, {12, 12, 12});
	expect("black threshold handles grey bars", detector.process_subtitle(greyBars), 16);
	paint(greyBars, 0, 0, 160, 1, {13, 0, 0});
	expect("threshold boundary is non-black", detector.process_subtitle(greyBars), 0);

	// Scan positions may collapse onto the same pixel at reduced resolutions.
	for (unsigned width = 0; width < 18; ++width)
		for (unsigned height = 0; height < 10; ++height)
		{
			Image<ColorRgb> small(width, height);
			small.clear();
			paint(small, 0, 0, width, height, ColorRgb::WHITE);
			const bool unknown = width == 0 || height < 3;
			expect("tiny image " + std::to_string(width) + "x" + std::to_string(height), detector.process_subtitle(small), unknown ? -1 : 0, unknown);
		}

	std::cout << checks << " checks, " << failures << " failures\n";
	return failures ? 1 : 0;
}
