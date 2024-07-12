/* SQLite.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
 */

#include <db/SQLite.h>
#include <sqlite3.h>
#include <QFileInfo>
#include <QMutexLocker>
#include <QMutex>

#include <sqlite3.h>
#include <algorithm>
#include <utility>

#include <iostream>

SqlDatabase::SqlDatabase(QString databaseName, bool readOnly)
{
	_readOnly = readOnly;
	_handle = nullptr;
	_databaseName = databaseName;
	//std::cout << "SQLite version: " << sqlite3_libversion() << ", DB: " << _databaseName.toStdString() << std::endl;
};

SqlDatabase::~SqlDatabase()
{
	if (_handle != nullptr)
	{
		sqlite3_close(_handle);
	}
};

QString SqlDatabase::databaseName()
{
	return _databaseName;
};

bool SqlDatabase::open()
{
	bool result = false;

	if (_readOnly)
	{
		static QMutex locker;
		QMutexLocker lockThis(&locker);

		const QString sharedDB = "file:hyperhdr?mode=memory&cache=shared";
		const QByteArray& textSharedDB = sharedDB.toUtf8().constData();

		result = sqlite3_open(textSharedDB, &_handle) == SQLITE_OK;

		if (QFileInfo(_databaseName).exists() && isEmpty())
		{
			sqlite3* handleSrc;
			const QByteArray& textSrc = _databaseName.toUtf8().constData();

			if (sqlite3_open(textSrc, &handleSrc) == SQLITE_OK)
			{
				const QByteArray& textQuery = QString("VACUUM INTO '%1';").arg(sharedDB).toUtf8().constData();
				sqlite3_exec(handleSrc, textQuery, NULL, NULL, NULL);
				sqlite3_close(handleSrc);
			}
		}
	}
	else
	{
		const QByteArray& text = _databaseName.toUtf8().constData();
		result = sqlite3_open(text, &_handle) == SQLITE_OK;

	}

	if (result)
	{
		sqlite3_busy_timeout(_handle, 1000);
	}

	return result;
};

QString SqlDatabase::error()
{
	QString retMsg;
	auto message = sqlite3_errmsg(_handle);
	if (message != nullptr)
	{
		retMsg = QString::fromUtf8(message);
	}
	return retMsg;
}

bool SqlDatabase::transaction()
{
	return sqlite3_exec(_handle, "BEGIN TRANSACTION;", NULL, NULL, NULL) == SQLITE_OK;
}

bool SqlDatabase::commit()
{
	return sqlite3_exec(_handle, "END TRANSACTION;", NULL, NULL, NULL) == SQLITE_OK;
}

bool SqlDatabase::rollback()
{
	return sqlite3_exec(_handle, "ROLLBACK TRANSACTION;", NULL, NULL, NULL) == SQLITE_OK;
}

bool SqlDatabase::doesColumnExist(QString table, QString column)
{
	bool hasRow = false;
	QString query = QString("SELECT * FROM pragma_table_info('%1') WHERE name='%2';").arg(table).arg(column);
	const QByteArray& text = query.toUtf8().constData();
	sqlite3_exec(_handle, text, [](void* _hasRow, int argc, char** argv, char** azColName) {
		*(reinterpret_cast<bool*>(_hasRow)) = true;
		return 0;
		}, &hasRow, NULL);
	return hasRow;
}

bool SqlDatabase::doesTableExist(QString table)
{
	bool hasRow = false;
	QString query = QString("SELECT name FROM sqlite_master WHERE type = 'table' AND name = '%1';").arg(table);
	const QByteArray& text = query.toUtf8().constData();
	sqlite3_exec(_handle, text, [](void* _hasRow, int argc, char** argv, char** azColName) {
		*(reinterpret_cast<bool*>(_hasRow)) = true;
		return 0;
		}, &hasRow, NULL);
	return hasRow;
}

bool SqlDatabase::isEmpty()
{
	bool hasRow = false;
	QString query = QString("SELECT name FROM sqlite_master WHERE type = 'table';");
	const QByteArray& text = query.toUtf8().constData();
	sqlite3_exec(_handle, text, [](void* _hasRow, int argc, char** argv, char** azColName) {
		*(reinterpret_cast<bool*>(_hasRow)) = true;
		return 0;
		}, &hasRow, NULL);
	return !hasRow;
}

QString SqlRecord::fieldName(int index)
{
	return _result[index].first;
}

QVariant SqlRecord::value(int index)
{
	return _result[index].second;
}

size_t SqlRecord::count()
{
	return _result.size();
}

SqlQuery::SqlQuery(SqlDatabase* connection) :
	_connection(connection),
	_prepared(nullptr),
	_paramIndex(1),
	_rowIndex(0),
	_stepResult(SQLITE_ERROR)
{
}

SqlQuery::~SqlQuery()
{
	if (_prepared != nullptr)
	{
		sqlite3_finalize(_prepared);
	}
}

bool SqlQuery::prepare(QString query)
{
	_rowIndex = 0;
	_paramIndex = 1;

	const QByteArray& text = query.toUtf8().constData();
	auto result = sqlite3_prepare_v2(
		_connection->_handle,
		text.data(),
		text.size(),
		&_prepared,
		NULL);

	return result == SQLITE_OK;
}

bool SqlQuery::exec()
{
	_stepResult = sqlite3_step(_prepared);
	return _stepResult == SQLITE_DONE || _stepResult == SQLITE_ROW;
}

bool SqlQuery::exec(QString query)
{
	const QByteArray& text = query.toUtf8().constData();
	auto result = sqlite3_exec(
		_connection->_handle,
		text.data(),
		NULL,
		NULL,
		NULL
	);
	return result == SQLITE_OK;
}


bool SqlQuery::addInt(int x)
{
	auto result = sqlite3_bind_int(_prepared, _paramIndex++, x);
	return result == SQLITE_OK;
}

bool SqlQuery::addDouble(double x)
{
	auto result = sqlite3_bind_double(_prepared, _paramIndex++, x);
	return result == SQLITE_OK;
}

bool SqlQuery::addBlob(QByteArray ar)
{
	auto result = sqlite3_bind_blob(_prepared, _paramIndex++, ar.data(), ar.size(), SQLITE_TRANSIENT);
	return result == SQLITE_OK;
}

bool SqlQuery::addString(QString s)
{
	const QByteArray& ar = s.toUtf8().constData();
	auto result = sqlite3_bind_text(_prepared, _paramIndex++, ar.data(), ar.size(), SQLITE_TRANSIENT);
	return result == SQLITE_OK;
}

SqlRecord SqlQuery::record()
{
	SqlRecord rec;
	int numberOfColumns = sqlite3_data_count(_prepared);

	for (int i = 0; i < numberOfColumns; i++)
	{
		int type = sqlite3_column_type(_prepared, i);
		QVariant value;

		if (type == SQLITE_BLOB)
		{
			auto size = sqlite3_column_bytes(_prepared, i);
			QByteArray arr(size, 0);
			auto data = sqlite3_column_blob(_prepared, i);
			memcpy(arr.data(), data, size);
			value = QVariant(arr);
		}
		else if (type == SQLITE_FLOAT)
		{
			value = QVariant(sqlite3_column_double(_prepared, i));
		}
		else if (type == SQLITE_INTEGER)
		{
			value = QVariant(sqlite3_column_int(_prepared, i));
		}
		else if (type == SQLITE3_TEXT)
		{
			auto size = sqlite3_column_bytes(_prepared, i);
			QByteArray arr(size, 0);
			auto data = sqlite3_column_blob(_prepared, i);
			memcpy(arr.data(), data, size);
			value = QVariant(QString::fromUtf8(arr));
		}

		auto columnName = sqlite3_column_name(_prepared, i);
		std::pair<QString, QVariant> column(QString::fromUtf8(columnName), value);
		rec._result.push_back(column);
	}

	return rec;
}

bool SqlQuery::next()
{
	if (_rowIndex++ > 0)
	{
		_stepResult = sqlite3_step(_prepared);
	}

	return (_stepResult == SQLITE_ROW);
}

