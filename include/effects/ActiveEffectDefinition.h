#pragma once

#ifndef PCH_ENABLED
	#include <QString>
	#include <QJsonObject>
#endif

struct ActiveEffectDefinition
{
	QString		name;
	int			priority = 0;
	int			timeout = 0;
};
