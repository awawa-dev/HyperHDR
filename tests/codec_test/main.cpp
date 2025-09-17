#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QSysInfo>
#include <QTextStream>
#include <QStandardPaths>
#include <algorithm>
#include <numeric>
#include <vector>
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <array>
#include <limits>
#include <QString>
#include <iostream>
#include <utils/FrameDecoder.h>
#include <utils/FrameDecoderUtils.h>
#include <base/AutomaticToneMapping.h>
#include <utils/Logger.h>
#include <utils/PixelFormat.h>
#include <utils/LutLoader.h>

namespace FrameDecoder
{
	void processImage(
		int _cropLeft, int _cropRight, int _cropTop, int _cropBottom,
		const uint8_t* data, const uint8_t* dataUV, int width, int height, int lineLength,
		const PixelFormat pixelFormat, const uint8_t* lutBuffer, Image<ColorRgb>& outputImage, bool toneMapping);

	void processQImage(
		const uint8_t* data, const uint8_t* dataUV, int width, int height, int lineLength,
		const PixelFormat pixelFormat, const uint8_t* lutBuffer, Image<ColorRgb>& outputImage, bool toneMapping, AutomaticToneMapping* automaticToneMapping);

}

void AutomaticToneMapping::finilize(){};
AutomaticToneMapping::AutomaticToneMapping() = default;
AutomaticToneMapping* AutomaticToneMapping::prepare() { return nullptr; }
void AutomaticToneMapping::scan_YUYV(int /*width*/, uint8_t* /*currentSourceY*/) {}
void AutomaticToneMapping::scan_Y_UV_8(int /*width*/, uint8_t* /*currentSourceY*/, uint8_t* /*currentSourceUV*/) {}
void AutomaticToneMapping::scan_Y_UV_16(int /*width*/, uint8_t* /*currentSourceY*/, uint8_t* /*currentSourceUV*/) {}


#define INPUT_X 1920
#define INPUT_Y 1080
#define IMAGE_COUNT 4
#define ITERATIONS 200

struct TestFile {
	QString fileName;
	PixelFormat pixelFormat;
	int frameSize;
	int lineLength;
};

int cropLeft = 0, cropRight = 0, cropTop = 0, cropBottom = 0;
LutLoader lut;

void old_func(uint8_t* frameData, const TestFile& testFile, bool quarter, bool toneMapping, Image<ColorRgb>& image)
{	
	if (quarter)
		FrameDecoder::processQImage(
			frameData, nullptr, INPUT_X, INPUT_Y, testFile.lineLength, testFile.pixelFormat, lut._lut.data(), image, toneMapping, nullptr);
	else
		FrameDecoder::processImage(
			cropLeft, cropRight, cropTop, cropBottom,
			frameData, nullptr, INPUT_X, INPUT_Y, testFile.lineLength, testFile.pixelFormat, lut._lut.data(), image, toneMapping);
}

void new_func(uint8_t* frameData, const TestFile& testFile, bool quarter, bool toneMapping, Image<ColorRgb>& image)
{
	FrameDecoder::dispatchProcessImageVector[quarter][toneMapping][false](
		cropLeft, cropRight, cropTop, cropBottom,
		frameData, nullptr, INPUT_X, INPUT_Y, testFile.lineLength, testFile.pixelFormat, lut._lut.data(), image, nullptr);
}

bool savePPM(const QString& filename, const Image<ColorRgb>& img)
{
	QFile file(filename);
	if (!file.open(QIODevice::WriteOnly)) return false;

	QByteArray header = QByteArray("P6\n") +
		QByteArray::number(img.width()) + " " +
		QByteArray::number(img.height()) + "\n255\n";
	file.write(header);

	for (int y = 0; y < static_cast<int>(img.height()); ++y) {
		for (int x = 0; x < static_cast<int>(img.width()); ++x) {
			const auto& c = img(x, y);
			char rgb[3] = { char(c.red), char(c.green), char(c.blue) };
			file.write(rgb, 3);
		}
	}

	return true;
}

// ==== Benchmark ====
struct Stats {
	double avg;
	double median;
};

void benchmark(void (*fn)(uint8_t*, const TestFile&, bool , bool , Image<ColorRgb>&),
	std::vector<double>& times, uint8_t* data, const TestFile& testFile, bool quarter, bool toneMapping,
	std::array<Image<ColorRgb>, IMAGE_COUNT>& outImg) {
	using clock = std::chrono::high_resolution_clock;
	using us = std::chrono::microseconds;

	for(auto& img : outImg)
		img.clear();

	for (int i = 0; i < ITERATIONS / 4; ++i)
	{
		// if we use static result image, the CPU most will cache the output
		outImg[i % IMAGE_COUNT] = Image<ColorRgb>();

		auto start = clock::now();
		fn(data, testFile, quarter, toneMapping, outImg[i % IMAGE_COUNT]);
		auto end = clock::now();
		auto elapsed = std::chrono::duration_cast<us>(end - start).count();
		times.push_back(static_cast<double>(elapsed));

		outImg[i % IMAGE_COUNT].setBufferCacheSize();
	}
}

Stats getStats(std::vector<double>& times, bool isNewEntryNotSummary = false)
{
	double median = 0;

	std::sort(times.begin(), times.end());

	if (isNewEntryNotSummary)
	{
		median = times[times.size() / 2];
		if (times.size() % 2 == 0)
		{
			median = (times[times.size() / 2 - 1] + median) / 2.0;
		}
	}

	double avg = std::accumulate(times.begin(), times.end(), 0.0) / times.size();

	return { avg, median };
}

QString cpuName();

#ifdef _WIN32
	#include <windows.h>
#endif

int main(int argc, char* argv[]) {
	QCoreApplication app(argc, argv);
	QTextStream out(stdout);

	auto fmtCell = [](const QString& text, int width) {
		return text.leftJustified(width, ' ');
		};

	#if defined(_WIN32)
		SetConsoleOutputCP(CP_UTF8);
		SetConsoleCP(CP_UTF8);
	#endif

	std::vector<TestFile> testFiles = {
			{"test_image_nv12.bin", PixelFormat::NV12    , (6 * INPUT_X * INPUT_Y) / 4, INPUT_X },
			{"test_image_p010.bin", PixelFormat::P010    , (6 * INPUT_X * INPUT_Y) / 2, INPUT_X * 2 },
			{"test_image_yuyv.bin", PixelFormat::YUYV    , INPUT_X * INPUT_Y * 2      , INPUT_X * 2},
			{ "test_image_uyvy.bin", PixelFormat::UYVY    , INPUT_X* INPUT_Y * 2      , INPUT_X * 2 },
			{"test_image_yuv420.bin", PixelFormat::I420  , (6 * INPUT_X * INPUT_Y) / 4, INPUT_X},
			{"test_image_rgb24.bin", PixelFormat::RGB24  , INPUT_X * INPUT_Y * 3      , INPUT_X * 3},			
			{"test_image_xrgb32.bin", PixelFormat::XRGB  , INPUT_X * INPUT_Y * 4      , INPUT_X * 4}
	};


	out << "> [!NOTE]\n";
	out << "> ### Offline benchmark: Old HyperHDR v21 vs New vectorized codecs HyperHDR v22\n";
	out << "> **Platform:** " << QSysInfo::prettyProductName() << " (" << cpuName() << ")\n";
	out << "> **System:** " << QSysInfo::kernelType() << " " << QSysInfo::kernelVersion() << "\n\n";

	out << "| File                  | Output size | ToneMap | Old avg [us] | Old median | New avg [us] | New median | Gain [%] |\n";
	out << "|-----------------------|-------------|---------|--------------|------------|--------------|------------|----------|\n";

	QString resultsDir = QCoreApplication::applicationDirPath() + "/results";
	QDir dir;
	dir.mkpath(resultsDir);

	std::vector<double> totalGains;
	for (const auto& testFile : testFiles)
	{
		std::vector<double> formatGains;
		const auto& fileName = testFile.fileName;
		for(int quarter = 0; quarter <= 1; quarter++)
			for (int toneMapping = 0; toneMapping <= 1; toneMapping++)
			{
				std::vector<double> gains;
				QString appDir = QCoreApplication::applicationDirPath();
				QString userFolder = QString("%1%2").arg(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)).arg("/.hyperhdr/lut_lin_tables.3d");
				QString filePath = appDir + "/" + fileName;
				QFile file(filePath);

				lut._hdrToneMappingEnabled = toneMapping;
				if (QFileInfo::exists(userFolder))
				{
					lut.loadLutFile(nullptr, (testFile.pixelFormat == PixelFormat::RGB24 || testFile.pixelFormat == PixelFormat::XRGB) ? PixelFormat::RGB24 : PixelFormat::YUYV, { userFolder });
				}
				else
				{
					out << "ERROR: cannot open LUT in the user home folder: " << userFolder << "\n";
				}


				if (toneMapping && (lut._lut.data() == nullptr || !lut._lutBufferInit))
				{
					out << "ERROR: cannot initialized LUT from the user home folder: " << userFolder << "\n";
					return 1;
				}

				if (!file.open(QIODevice::ReadOnly)) {
					out << "| " << fileName << " | ERROR: cannot open\n";
					continue;
				}
				if (file.size() != testFile.frameSize) {
					out << "| " << fileName << " | ERROR: incorrect size\n";
					continue;
				}
				QByteArray data = file.readAll();
				file.close();

				QThread::msleep(3000);

				uint8_t* buffer = reinterpret_cast<uint8_t*>(data.data());

				std::array<Image<ColorRgb>, IMAGE_COUNT> imgOld;
				std::array<Image<ColorRgb>, IMAGE_COUNT> imgNew;
				std::vector<double> timesNew, timesOld;
				timesNew.reserve(ITERATIONS);
				timesOld.reserve(ITERATIONS);

				benchmark(old_func, timesOld, buffer, testFile, quarter, toneMapping, imgOld);
				benchmark(new_func, timesNew, buffer, testFile, quarter, toneMapping, imgNew);
				benchmark(old_func, timesOld, buffer, testFile, quarter, toneMapping, imgOld);
				benchmark(new_func, timesNew, buffer, testFile, quarter, toneMapping, imgNew);
				

				int resCompare  = 0;
				if (imgNew.front().width() != imgOld.front().width() || imgNew.front().height() != imgOld.front().height() ||
					(resCompare = std::memcmp(imgNew.front().rawMem(), imgOld.front().rawMem(), imgNew.front().width() * imgNew.front().height() * 3)) != 0)
				{
					out << "| "
						<< fmtCell((quarter == 0 && toneMapping == 0) ? fileName : "", 21)
						<< " | " << fmtCell(QString::number(imgOld.front().width()), 4) << "x" << fmtCell(QString::number(imgOld.front().height()), 4) << ((quarter) ? " (q)" : " (f)")
						<< " | " << fmtCell(((toneMapping) ? "ON" : "OFF"), 7)
						<< "| ERROR: data verification failed, code= "<< resCompare <<"\n";
						//continue;
				}

				Stats oldStats = getStats(timesOld, true);
				Stats newStats = getStats(timesNew, true);


				double speedup = (newStats.avg > 1 && oldStats.avg > 1) ? (1.0 - (newStats.avg / oldStats.avg)) * 100.0 : std::numeric_limits<double>::quiet_NaN();

				if (!std::isnan(speedup))
				{
					formatGains.push_back(std::log(oldStats.avg / newStats.avg));
				}

				out << "| "
					<< fmtCell((quarter ==0 && toneMapping == 0) ? fileName : "", 21)
					<< " | " << fmtCell(QString::number(imgNew.front().width()) + "x" + QString::number(imgNew.front().height()) + ((quarter) ? " Q" : "F"), 11)
					<< " | " << fmtCell(((toneMapping) ? "ON" : "OFF"), 7)
					<< " | " << fmtCell(QString::number(oldStats.avg), 12)
					<< " | " << fmtCell(QString::number(oldStats.median), 10)
					<< " | " << fmtCell(QString::number(newStats.avg), 12)
					<< " | " << fmtCell(QString::number(newStats.median), 10)
					<< " | " << fmtCell((std::isnan(speedup)) ? "-" : QString::number(speedup, 'f', 2) +"%", 8)
					<< " |\n";					

				// zapisujemy wynik new_func jako PPM
				QString baseName = QFileInfo(fileName).baseName();
				QString outPath = QString("%1/%2%3%4.ppm").arg(resultsDir).arg(baseName).arg(quarter?"_quarter":"_full").arg(toneMapping ? "_tonemapping" : "");
				savePPM(outPath, imgNew.front());
				out.flush();
			}


		Stats formatStats = getStats(formatGains);
		double formatGeoMean = std::exp(formatStats.avg);
		double formatSpeedup = (1.0 - (1.0 / formatGeoMean)) * 100.0;
		out << "|                       |             |         |              |            |              |   GeoMean: | " << fmtCell(QString::number(formatSpeedup, 'f', 2) + "%", 8) << " | \n";
		out << "| --------------------- | ----------- | ------- | ------------ | ---------- | ------------ | ---------- | -------- |\n";
		out.flush();

		totalGains.insert(totalGains.end(),
			std::make_move_iterator(formatGains.begin()),
			std::make_move_iterator(formatGains.end()));
		formatGains.clear();
	}


	Stats totalStats = getStats(totalGains);
	double totalGeoMean = std::exp(totalStats.avg);
	double totalSpeedup = (1.0 - (1.0 / totalGeoMean)) * 100.0;
	out << "|                       |             |         |              |            |              | TotalMean: | " << fmtCell(QString::number(totalSpeedup, 'f', 2) + "%", 8) << " | \n";
	out.flush();

	return 0;
}


// --- Windows ---
#ifdef _WIN32
#include <intrin.h>
#endif

// --- Linux ---
#ifdef __linux__
#include <fstream>
#include <string>
#endif

// --- macOS ---
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

QString cpuName()
{
#ifdef _WIN32
	int cpuInfo[4] = { -1 };
	char name[0x40];
	__cpuid(cpuInfo, 0x80000000);
	unsigned int nExIds = cpuInfo[0];
	memset(name, 0, sizeof(name));
	if (nExIds >= 0x80000004) {
		__cpuid(cpuInfo, 0x80000002);
		memcpy(name, cpuInfo, sizeof(cpuInfo));
		__cpuid(cpuInfo, 0x80000003);
		memcpy(name + 16, cpuInfo, sizeof(cpuInfo));
		__cpuid(cpuInfo, 0x80000004);
		memcpy(name + 32, cpuInfo, sizeof(cpuInfo));
	}
	return QString::fromLatin1(name).trimmed();

#elif defined(__linux__)
	std::ifstream f("/proc/cpuinfo");
	std::string line;

	while (std::getline(f, line)) {
		if (line.find("model name") != std::string::npos) {
			auto pos = line.find(':');
			if (pos != std::string::npos) {
				return QString::fromStdString(line.substr(pos + 2));
			}
		}

		if (line.find("Model") != std::string::npos) {
			auto pos = line.find(':');
			if (pos != std::string::npos) {
				return QString::fromStdString(line.substr(pos + 2));
			}
		}

		if (line.find("Hardware") != std::string::npos) {
			auto pos = line.find(':');
			if (pos != std::string::npos) {
				return QString::fromStdString(line.substr(pos + 2));
			}
		}
	}

	return "Unknown CPU";


#elif defined(__APPLE__)
	char buffer[256];
	size_t bufferlen = sizeof(buffer);
	if (sysctlbyname("machdep.cpu.brand_string", &buffer, &bufferlen, NULL, 0) == 0) {
		return QString::fromLatin1(buffer);
	}
	return "Unknown CPU";

#else
	return "Unknown CPU";
#endif
}

////////////////////////////////////////////
/// Old decoders
////////////////////////////////////////////


namespace FrameDecoder
{
	void processImage(
		int _cropLeft, int _cropRight, int _cropTop, int _cropBottom,
		const uint8_t* data, const uint8_t* dataUV, int width, int height, int lineLength,
		const PixelFormat pixelFormat, const uint8_t* lutBuffer, Image<ColorRgb>& outputImage, bool toneMapping)
	{
		uint32_t ind_lutd, ind_lutd2;
		uint8_t  buffer[8];

		// validate format
		if (pixelFormat != PixelFormat::YUYV && pixelFormat != PixelFormat::UYVY &&
			pixelFormat != PixelFormat::XRGB && pixelFormat != PixelFormat::RGB24 &&
			pixelFormat != PixelFormat::I420 && pixelFormat != PixelFormat::NV12 && pixelFormat != PixelFormat::P010 && pixelFormat != PixelFormat::MJPEG)
		{
			Error("FrameDecoder", "Invalid pixel format given");
			return;
		}

		// validate format LUT
		if ((pixelFormat == PixelFormat::YUYV || pixelFormat == PixelFormat::UYVY || pixelFormat == PixelFormat::I420 || pixelFormat == PixelFormat::MJPEG ||
			pixelFormat == PixelFormat::NV12 || pixelFormat == PixelFormat::P010 || toneMapping) && lutBuffer == NULL)
		{
			Error("FrameDecoder", "Missing LUT table for YUV colorspace or tone mapping");
			return;
		}

		// sanity check, odd values doesnt work for yuv either way
		_cropLeft = (_cropLeft >> 1) << 1;
		_cropRight = (_cropRight >> 1) << 1;

		// calculate the output size
		int outputWidth = (width - _cropLeft - _cropRight);
		int outputHeight = (height - _cropTop - _cropBottom);

		outputImage.resize(outputWidth, outputHeight);
		outputImage.setOriginFormat(pixelFormat);

		uint8_t* destMemory = outputImage.rawMem();
		int 		destLineSize = outputImage.width() * 3;


		if (pixelFormat == PixelFormat::YUYV)
		{
			for (int yDest = 0, ySource = _cropTop; yDest < outputHeight; ++ySource, ++yDest)
			{
				uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
				uint8_t* endDest = currentDest + destLineSize;
				uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource) + (((uint64_t)_cropLeft) << 1));

				while (currentDest < endDest)
				{
					*((uint32_t*)&buffer) = *((uint32_t*)currentSource);

					ind_lutd = LUT_INDEX(buffer[0], buffer[1], buffer[3]);
					ind_lutd2 = LUT_INDEX(buffer[2], buffer[1], buffer[3]);

					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
					currentDest += 3;
					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd2]));
					currentDest += 3;
					currentSource += 4;
				}
			}

			return;
		}

		if (pixelFormat == PixelFormat::UYVY)
		{
			for (int yDest = 0, ySource = _cropTop; yDest < outputHeight; ++ySource, ++yDest)
			{
				uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
				uint8_t* endDest = currentDest + destLineSize;
				uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource) + (((uint64_t)_cropLeft) << 1));

				while (currentDest < endDest)
				{
					*((uint32_t*)&buffer) = *((uint32_t*)currentSource);

					ind_lutd = LUT_INDEX(buffer[1], buffer[0], buffer[2]);
					ind_lutd2 = LUT_INDEX(buffer[3], buffer[0], buffer[2]);

					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
					currentDest += 3;
					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd2]));
					currentDest += 3;
					currentSource += 4;
				}
			}
			return;
		}

		if (pixelFormat == PixelFormat::RGB24)
		{
			for (int yDest = 0, ySource = height - _cropBottom - 1; yDest < outputHeight; --ySource, ++yDest)
			{
				uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
				uint8_t* endDest = currentDest + destLineSize;
				uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource) + (((uint64_t)_cropLeft) * 3));

				if (!toneMapping)
					currentSource += 2;

				while (currentDest < endDest)
				{
					if (toneMapping)
					{
						*((uint32_t*)&buffer) = *((uint32_t*)currentSource);
						currentSource += 3;
						ind_lutd = LUT_INDEX(buffer[2], buffer[1], buffer[0]);
						*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
						currentDest += 3;
					}
					else
					{
						*currentDest++ = *currentSource--;
						*currentDest++ = *currentSource--;
						*currentDest++ = *currentSource;
						currentSource += 5;
					}
				}
			}
			return;
		}

		if (pixelFormat == PixelFormat::XRGB)
		{
			for (int yDest = 0, ySource = height - _cropBottom - 1; yDest < outputHeight; --ySource, ++yDest)
			{
				uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
				uint8_t* endDest = currentDest + destLineSize;
				uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource) + (((uint64_t)_cropLeft) * 4));

				if (!toneMapping)
					currentSource += 2;

				while (currentDest < endDest)
				{
					if (toneMapping)
					{
						*((uint32_t*)&buffer) = *((uint32_t*)currentSource);
						currentSource += 4;
						ind_lutd = LUT_INDEX(buffer[2], buffer[1], buffer[0]);
						*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
						currentDest += 3;
					}
					else
					{
						*currentDest++ = *currentSource--;
						*currentDest++ = *currentSource--;
						*currentDest++ = *currentSource;
						currentSource += 6;
					}
				}
			}
			return;
		}

		if (pixelFormat == PixelFormat::I420)
		{
			int deltaU = lineLength * height;
			int deltaV = lineLength * height * 5 / 4;
			for (int yDest = 0, ySource = _cropTop; yDest < outputHeight; ++ySource, ++yDest)
			{
				uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
				uint8_t* endDest = currentDest + destLineSize;
				uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource) + ((uint64_t)_cropLeft));
				uint8_t* currentSourceU = (uint8_t*)data + deltaU + ((((uint64_t)ySource / 2) * lineLength) + ((uint64_t)_cropLeft)) / 2;
				uint8_t* currentSourceV = (uint8_t*)data + deltaV + ((((uint64_t)ySource / 2) * lineLength) + ((uint64_t)_cropLeft)) / 2;

				while (currentDest < endDest)
				{
					*((uint16_t*)&buffer) = *((uint16_t*)currentSource);
					currentSource += 2;
					buffer[2] = *(currentSourceU++);
					buffer[3] = *(currentSourceV++);

					ind_lutd = LUT_INDEX(buffer[0], buffer[2], buffer[3]);
					ind_lutd2 = LUT_INDEX(buffer[1], buffer[2], buffer[3]);

					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
					currentDest += 3;
					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd2]));
					currentDest += 3;
				}
			}
			return;
		}

		if (pixelFormat == PixelFormat::MJPEG)
		{
			int deltaU = lineLength * height;
			int deltaV = lineLength * height * 6 / 4;
			for (int yDest = 0, ySource = _cropTop; yDest < outputHeight; ++ySource, ++yDest)
			{
				uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
				uint8_t* endDest = currentDest + destLineSize;
				uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource) + ((uint64_t)_cropLeft));
				uint8_t* currentSourceU = (uint8_t*)data + deltaU + ((((uint64_t)ySource) * lineLength) + ((uint64_t)_cropLeft)) / 2;
				uint8_t* currentSourceV = (uint8_t*)data + deltaV + ((((uint64_t)ySource) * lineLength) + ((uint64_t)_cropLeft)) / 2;

				while (currentDest < endDest)
				{
					*((uint16_t*)&buffer) = *((uint16_t*)currentSource);
					currentSource += 2;
					buffer[2] = *(currentSourceU++);
					buffer[3] = *(currentSourceV++);

					ind_lutd = LUT_INDEX(buffer[0], buffer[2], buffer[3]);
					ind_lutd2 = LUT_INDEX(buffer[1], buffer[2], buffer[3]);

					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
					currentDest += 3;
					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd2]));
					currentDest += 3;
				}
			}
			return;
		}

		if (pixelFormat == PixelFormat::P010)
		{
			uint16_t p010[2] = {};

			FrameDecoderUtils& instance = FrameDecoderUtils::instance();
			auto lutP010_y = instance.getLutP010_y();
			auto lutP010_uv = instance.getlutP010_uv();

			auto deltaUV = (dataUV != nullptr) ? (uint8_t*)dataUV : (uint8_t*)data + lineLength * height;
			for (int yDest = 0, ySource = _cropTop; yDest < outputHeight; ++ySource, ++yDest)
			{
				uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
				uint8_t* endDest = currentDest + destLineSize;
				uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource) + ((uint64_t)_cropLeft));
				uint8_t* currentSourceUV = deltaUV + (((uint64_t)ySource / 2) * lineLength) + ((uint64_t)_cropLeft);

				while (currentDest < endDest)
				{
					memcpy(((uint32_t*)&p010), ((uint32_t*)currentSource), 4);
					if (toneMapping)
					{
						buffer[0] = lutP010_y[p010[0] >> 6];
						buffer[1] = lutP010_y[p010[1] >> 6];
					}
					else
					{
						buffer[0] = p010[0] >> 8;
						buffer[1] = p010[1] >> 8;
					}

					currentSource += 4;
					memcpy(((uint32_t*)&p010), ((uint32_t*)currentSourceUV), 4);
					if (toneMapping)
					{
						buffer[2] = lutP010_uv[p010[0] >> 6];
						buffer[3] = lutP010_uv[p010[1] >> 6];
					}
					else
					{
						buffer[2] = p010[0] >> 8;
						buffer[3] = p010[1] >> 8;
					}

					currentSourceUV += 4;

					ind_lutd = LUT_INDEX(buffer[0], buffer[2], buffer[3]);
					ind_lutd2 = LUT_INDEX(buffer[1], buffer[2], buffer[3]);

					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
					currentDest += 3;
					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd2]));
					currentDest += 3;
				}
			}
			return;
		}

		if (pixelFormat == PixelFormat::NV12)
		{
			auto deltaUV = (dataUV != nullptr) ? (uint8_t*)dataUV : (uint8_t*)data + lineLength * height;
			for (int yDest = 0, ySource = _cropTop; yDest < outputHeight; ++ySource, ++yDest)
			{
				uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
				uint8_t* endDest = currentDest + destLineSize;
				uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource) + ((uint64_t)_cropLeft));
				uint8_t* currentSourceUV = deltaUV + (((uint64_t)ySource / 2) * lineLength) + ((uint64_t)_cropLeft);

				while (currentDest < endDest)
				{
					*((uint16_t*)&buffer) = *((uint16_t*)currentSource);
					currentSource += 2;
					*((uint16_t*)&(buffer[2])) = *((uint16_t*)currentSourceUV);
					currentSourceUV += 2;

					ind_lutd = LUT_INDEX(buffer[0], buffer[2], buffer[3]);
					ind_lutd2 = LUT_INDEX(buffer[1], buffer[2], buffer[3]);

					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
					currentDest += 3;
					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd2]));
					currentDest += 3;
				}
			}

			return;
		}
	}

	void processQImage(
		const uint8_t* data, const uint8_t* dataUV, int width, int height, int lineLength,
		const PixelFormat pixelFormat, const uint8_t* lutBuffer, Image<ColorRgb>& outputImage, bool toneMapping, AutomaticToneMapping* automaticToneMapping)
	{
		uint32_t ind_lutd;
		uint8_t  buffer[8];

		// validate format
		if (pixelFormat != PixelFormat::YUYV && pixelFormat != PixelFormat::UYVY &&
			pixelFormat != PixelFormat::XRGB && pixelFormat != PixelFormat::RGB24 &&
			pixelFormat != PixelFormat::I420 && pixelFormat != PixelFormat::NV12 && pixelFormat != PixelFormat::P010)
		{
			Error("FrameDecoder", "Invalid pixel format given");
			return;
		}

		// validate format LUT
		if ((pixelFormat == PixelFormat::YUYV || pixelFormat == PixelFormat::UYVY || pixelFormat == PixelFormat::I420 ||
			pixelFormat == PixelFormat::NV12 || pixelFormat == PixelFormat::P010 || toneMapping) && lutBuffer == NULL)
		{
			Error("FrameDecoder", "Missing LUT table for YUV colorspace or tone mapping");
			return;
		}

		// calculate the output size
		int outputWidth = (width) >> 1;
		int outputHeight = (height) >> 1;

		outputImage.resize(outputWidth, outputHeight);

		uint8_t* destMemory = outputImage.rawMem();
		int 		destLineSize = outputImage.width() * 3;


		if (pixelFormat == PixelFormat::YUYV && automaticToneMapping == nullptr)
		{
			for (int yDest = 0, ySource = 0; yDest < outputHeight; ySource += 2, ++yDest)
			{
				uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
				uint8_t* endDest = currentDest + destLineSize;
				uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));

				while (currentDest < endDest)
				{
					*((uint32_t*)&buffer) = *((uint32_t*)currentSource);

					ind_lutd = LUT_INDEX(buffer[0], buffer[1], buffer[3]);

					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
					currentDest += 3;
					currentSource += 4;
				}
			}
			return;
		}
		if (pixelFormat == PixelFormat::YUYV && automaticToneMapping != nullptr)
		{
			for (int yDest = 0, ySource = 0; yDest < outputHeight; ySource += 2, ++yDest)
			{
				uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
				uint8_t* endDest = currentDest + destLineSize;
				uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));

				while (currentDest < endDest)
				{
					*((uint32_t*)&buffer) = *((uint32_t*)currentSource);

					ind_lutd = LUT_INDEX((automaticToneMapping->checkY(buffer[0])), (automaticToneMapping->checkU(buffer[1])), (automaticToneMapping->checkV(buffer[3])));

					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
					currentDest += 3;
					currentSource += 4;
				}
			}
			automaticToneMapping->finilize();
			return;
		}

		if (pixelFormat == PixelFormat::UYVY)
		{
			for (int yDest = 0, ySource = 0; yDest < outputHeight; ySource += 2, ++yDest)
			{
				uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
				uint8_t* endDest = currentDest + destLineSize;
				uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));

				while (currentDest < endDest)
				{
					*((uint32_t*)&buffer) = *((uint32_t*)currentSource);

					ind_lutd = LUT_INDEX(buffer[1], buffer[0], buffer[2]);

					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
					currentDest += 3;
					currentSource += 4;
				}
			}
			return;
		}

		if (pixelFormat == PixelFormat::RGB24)
		{
			for (int yDest = 0, ySource = height - 1; yDest < outputHeight; ySource -= 2, ++yDest)
			{
				uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
				uint8_t* endDest = currentDest + destLineSize;
				uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));

				if (!toneMapping)
					currentSource += 2;

				while (currentDest < endDest)
				{
					if (toneMapping)
					{
						*((uint32_t*)&buffer) = *((uint32_t*)currentSource);
						currentSource += 6;
						ind_lutd = LUT_INDEX(buffer[2], buffer[1], buffer[0]);
						*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
						currentDest += 3;
					}
					else
					{
						*currentDest++ = *currentSource--;
						*currentDest++ = *currentSource--;
						*currentDest++ = *currentSource;
						currentSource += 8;
					}
				}
			}
			return;
		}

		if (pixelFormat == PixelFormat::XRGB)
		{
			for (int yDest = 0, ySource = height - 1; yDest < outputHeight; ySource -= 2, ++yDest)
			{
				uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
				uint8_t* endDest = currentDest + destLineSize;
				uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));

				if (!toneMapping)
					currentSource += 2;

				while (currentDest < endDest)
				{
					if (toneMapping)
					{
						*((uint32_t*)&buffer) = *((uint32_t*)currentSource);
						currentSource += 8;
						ind_lutd = LUT_INDEX(buffer[2], buffer[1], buffer[0]);
						*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
						currentDest += 3;
					}
					else
					{
						*currentDest++ = *currentSource--;
						*currentDest++ = *currentSource--;
						*currentDest++ = *currentSource;
						currentSource += 10;
					}
				}
			}
			return;
		}

		if (pixelFormat == PixelFormat::I420)
		{
			int deltaU = lineLength * height;
			int deltaV = lineLength * height * 5 / 4;
			for (int yDest = 0, ySource = 0; yDest < outputHeight; ySource += 2, ++yDest)
			{
				uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
				uint8_t* endDest = currentDest + destLineSize;
				uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));
				uint8_t* currentSourceU = (uint8_t*)data + deltaU + (ySource * lineLength / 2) / 2;
				uint8_t* currentSourceV = (uint8_t*)data + deltaV + (ySource * lineLength / 2) / 2;

				while (currentDest < endDest)
				{
					*((uint8_t*)&buffer) = *((uint8_t*)currentSource);
					currentSource += 2;
					buffer[2] = *(currentSourceU++);
					buffer[3] = *(currentSourceV++);

					ind_lutd = LUT_INDEX(buffer[0], buffer[2], buffer[3]);

					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
					currentDest += 3;
				}
			}
			return;
		}

		if (pixelFormat == PixelFormat::P010 && automaticToneMapping == nullptr)
		{
			uint16_t p010[2] = {};

			FrameDecoderUtils& instance = FrameDecoderUtils::instance();
			auto lutP010_y = instance.getLutP010_y();
			auto lutP010_uv = instance.getlutP010_uv();

			uint8_t* deltaUV = (dataUV != nullptr) ? (uint8_t*)dataUV : (uint8_t*)data + lineLength * height;
			for (int yDest = 0, ySource = 0; yDest < outputHeight; ySource += 2, ++yDest)
			{
				uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
				uint8_t* endDest = currentDest + destLineSize;
				uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));
				uint8_t* currentSourceU = deltaUV + (((uint64_t)ySource / 2) * lineLength);

				while (currentDest < endDest)
				{
					memcpy(((uint16_t*)&p010), ((uint16_t*)currentSource), 2);
					if (toneMapping)
					{
						buffer[0] = lutP010_y[p010[0] >> 6];
					}
					else
					{
						buffer[0] = p010[0] >> 8;
					}
					currentSource += 4;
					memcpy(((uint32_t*)&p010), ((uint32_t*)currentSourceU), 4);
					if (toneMapping)
					{
						buffer[2] = lutP010_uv[p010[0] >> 6];
						buffer[3] = lutP010_uv[p010[1] >> 6];
					}
					else
					{
						buffer[2] = p010[0] >> 8;
						buffer[3] = p010[1] >> 8;
					}

					currentSourceU += 4;

					ind_lutd = LUT_INDEX(buffer[0], buffer[2], buffer[3]);

					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
					currentDest += 3;
				}
			}
			return;
		}
		if (pixelFormat == PixelFormat::P010 && automaticToneMapping != nullptr)
		{
			uint16_t p010[2] = {};

			FrameDecoderUtils& instance = FrameDecoderUtils::instance();
			auto lutP010_y = instance.getLutP010_y();
			auto lutP010_uv = instance.getlutP010_uv();

			uint8_t* deltaUV = (dataUV != nullptr) ? (uint8_t*)dataUV : (uint8_t*)data + lineLength * height;
			for (int yDest = 0, ySource = 0; yDest < outputHeight; ySource += 2, ++yDest)
			{
				uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
				uint8_t* endDest = currentDest + destLineSize;
				uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));
				uint8_t* currentSourceU = deltaUV + (((uint64_t)ySource / 2) * lineLength);

				while (currentDest < endDest)
				{
					memcpy(((uint16_t*)&p010), ((uint16_t*)currentSource), 2);
					if (toneMapping)
					{
						automaticToneMapping->checkY(p010[0] >> 8);

						buffer[0] = lutP010_y[p010[0] >> 6];
					}
					else
					{
						buffer[0] = automaticToneMapping->checkY(p010[0] >> 8);
					}
					currentSource += 4;
					memcpy(((uint32_t*)&p010), ((uint32_t*)currentSourceU), 4);
					if (toneMapping)
					{
						automaticToneMapping->checkU(p010[0] >> 8);
						automaticToneMapping->checkV(p010[1] >> 8);

						buffer[2] = lutP010_uv[p010[0] >> 6];
						buffer[3] = lutP010_uv[p010[1] >> 6];
					}
					else
					{
						buffer[2] = automaticToneMapping->checkU(p010[0] >> 8);
						buffer[3] = automaticToneMapping->checkV(p010[1] >> 8);
					}

					currentSourceU += 4;

					ind_lutd = LUT_INDEX(buffer[0], buffer[2], buffer[3]);

					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
					currentDest += 3;
				}
			}
			automaticToneMapping->finilize();
			return;
		}

		if (pixelFormat == PixelFormat::NV12 && automaticToneMapping == nullptr)
		{
			uint8_t* deltaUV = (dataUV != nullptr) ? (uint8_t*)dataUV : (uint8_t*)data + lineLength * height;
			for (int yDest = 0, ySource = 0; yDest < outputHeight; ySource += 2, ++yDest)
			{
				uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
				uint8_t* endDest = currentDest + destLineSize;
				uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));
				uint8_t* currentSourceU = deltaUV + (((uint64_t)ySource / 2) * lineLength);

				while (currentDest < endDest)
				{
					*((uint8_t*)&buffer) = *((uint8_t*)currentSource);
					currentSource += 2;
					*((uint16_t*)&(buffer[2])) = *((uint16_t*)currentSourceU);
					currentSourceU += 2;

					ind_lutd = LUT_INDEX(buffer[0], buffer[2], buffer[3]);

					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
					currentDest += 3;
				}
			}
			return;
		}
		if (pixelFormat == PixelFormat::NV12 && automaticToneMapping != nullptr)
		{
			uint8_t* deltaUV = (dataUV != nullptr) ? (uint8_t*)dataUV : (uint8_t*)data + lineLength * height;
			for (int yDest = 0, ySource = 0; yDest < outputHeight; ySource += 2, ++yDest)
			{
				uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
				uint8_t* endDest = currentDest + destLineSize;
				uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));
				uint8_t* currentSourceU = deltaUV + (((uint64_t)ySource / 2) * lineLength);

				while (currentDest < endDest)
				{
					*((uint8_t*)&buffer) = *((uint8_t*)currentSource);
					currentSource += 2;
					*((uint16_t*)&(buffer[2])) = *((uint16_t*)currentSourceU);
					currentSourceU += 2;

					ind_lutd = LUT_INDEX((automaticToneMapping->checkY(buffer[0])), (automaticToneMapping->checkU(buffer[2])), (automaticToneMapping->checkV(buffer[3])));

					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
					currentDest += 3;
				}
			}
			automaticToneMapping->finilize();
			return;
		}
	}
};


