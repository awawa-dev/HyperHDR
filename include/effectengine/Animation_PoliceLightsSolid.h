#pragma once

#include <effectengine/Animation_Police.h>

#define ANIM_POLICELIGHTSSOLID "Police Lights Solid"

class Animation_PoliceLightsSolid : public Animation_Police
{
	Q_OBJECT

private:

	static QJsonObject GetArgs();

public:

	Animation_PoliceLightsSolid(QString name = ANIM_POLICELIGHTSSOLID);

	static EffectDefinition getDefinition();
};
