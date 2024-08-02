#pragma once

/* SQLite.h
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

#include <vector>
#include <QString>
#include <QByteArray>
#include <QVariant>

struct sqlite3;
struct sqlite3_stmt;

class SqlDatabase
{
	friend class SqlQuery;
	bool _readOnly;
	QString _databaseName;
	sqlite3* _handle;

public:
	SqlDatabase(QString databaseName, bool _readOnly);
	~SqlDatabase();

	QString databaseName();
	bool open();
	QString error();
	bool transaction();
	bool commit();
	bool rollback();
	bool doesColumnExist(QString table, QString column);
	bool doesTableExist(QString table);
	bool isEmpty();
};

class SqlRecord
{
	friend class SqlQuery;
	std::vector<std::pair<QString, QVariant>> _result;

public:
	QString fieldName(int index);
	QVariant value(int index);
	size_t count();
};

class SqlQuery
{
	SqlDatabase* _connection;
	sqlite3_stmt* _prepared;
	int _paramIndex;
	int _rowIndex;
	int _stepResult;

public:
	SqlQuery(SqlDatabase* connection);
	~SqlQuery();
	bool prepare(QString query);
	bool exec();
	bool exec(QString query);
	bool addInt(int x);
	bool addDouble(double x);
	bool addBlob(QByteArray ar);
	bool addString(QString s);

	SqlRecord record();
	bool next();
};
