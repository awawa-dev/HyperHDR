#include <effectengine/Animation_KnightRider.h>

Animation_KnightRider::Animation_KnightRider() :
	AnimationBase(ANIM_KNIGHT_RIDER)
{
	speed = 0;
	fadeFactor = 0;
	color = { 0, 0, 0 };	
	increment = 0;
	position = 0;
	direction = 0;

	for (int j = 0; j < KNIGHT_WIDTH; j++) {
		imageData[3 * j] = 0;
		imageData[3 * j + 1] = 0;
		imageData[3 * j + 2] = 0;
	}
};

EffectDefinition Animation_KnightRider::getDefinition()
{
	EffectDefinition ed;
	ed.name = ANIM_KNIGHT_RIDER;
	ed.args = GetArgs();
	return ed;
}

void Animation_KnightRider::Init(
	QImage& hyperImage,
	int hyperLatchTime	
)
{
	
	hyperImage = hyperImage.scaled(KNIGHT_WIDTH, KNIGHT_HEIGHT);

	speed = 1.0;
	fadeFactor = 0.7;
	color = { 255, 0, 0 };

	imageData[0] = color.x;
	imageData[1] = color.y;
	imageData[2] = color.z;
	
	increment = 1;
	double sleepTime = 1.0 / (speed * KNIGHT_WIDTH);
	while (sleepTime < 0.05)
	{
		increment *= 2;
		sleepTime *= 2;
	}

	SetSleepTime(int(round(sleepTime*1000.0)));

	position = 0;
	direction = 1;	
}

bool Animation_KnightRider::Play(QPainter* painter)
{
	bool ret = true;

	for (int i = 0; i < increment; i++) 
	{
		position += direction;
		if (position == -1)
		{
			position = 1;
			direction = 1;
		}
		else if (position == KNIGHT_WIDTH)
		{
			position = KNIGHT_WIDTH - 2;
			direction = -1;
		}
		
		for (int j = 0; j < KNIGHT_WIDTH; j++) {
			imageData[3*j    ] = uint8_t(clamp(int(fadeFactor * imageData[3 * j]), 0, 255));
			imageData[3*j + 1] = uint8_t(clamp(int(fadeFactor * imageData[3 * j + 1]), 0, 255));
			imageData[3*j + 2] = uint8_t(clamp(int(fadeFactor * imageData[3 * j + 2]), 0, 255));

			painter->setPen(qRgb(imageData[3*j], imageData[3*j+1], imageData[3*j+2]));
			painter->drawLine(j, 0, j, KNIGHT_HEIGHT-1);
			
		}

		imageData[3 * position] = color.x;
		imageData[3 * position + 1] = color.y;
		imageData[3 * position + 2] = color.z;

		//QImage img(imageData, KNIGHT_WIDTH, KNIGHT_HEIGHT, KNIGHT_WIDTH, QImage::Format_RGB888);
		painter->setPen(qRgb(color.x, color.y, color.z));
		painter->drawLine(position, 0, position, KNIGHT_HEIGHT-1);
	}
	return ret;
}

QJsonObject Animation_KnightRider::GetArgs() {
	QJsonObject doc;
	doc["smoothing-custom-settings"] = false;
	return doc;
}





