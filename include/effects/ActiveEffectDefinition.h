#pragma once

#ifndef PCH_ENABLED
	#include <QString>
	#include <QJsonObject>
#endif

struct ActiveEffectDefinition
{
	QString		name;
	int			priority;
	int			timeout;
};
