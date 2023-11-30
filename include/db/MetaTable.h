#pragma once

#include <db/DBManager.h>

class MetaTable : public DBManager
{

public:
	MetaTable(bool readonlyMode = false);

	QString getUUID() const;
};
