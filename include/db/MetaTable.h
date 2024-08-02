#pragma once

#include <db/DBManager.h>

class MetaTable : public DBManager
{

public:
	MetaTable();

	QString getUUID() const;
};
