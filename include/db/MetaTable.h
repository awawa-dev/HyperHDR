#pragma once

#include <db/DBManager.h>

///
/// @brief meta table specific database interface
///
class MetaTable : public DBManager
{

public:
	/// construct wrapper with plugins table and columns
	MetaTable(QObject* parent = nullptr, bool readonlyMode = false);

	///
	/// @brief Get the uuid, if the uuid is not set it will be created
	/// @return The uuid
	///
	QString getUUID() const;
};
