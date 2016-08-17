#include "SQLOperation.h"
#include "DatabaseConnection.h"

SQLOperation::SQLOperation(DatabaseConnection* conn) :
	MySqlConnectionHandle(conn),
	MySqlStatementHandle(nullptr),
	OperationFlag(SqlOperationFlag::Neither),
	ParamCount(0),
	ParamSetMask(0x00000000),
	ParamBindHandle(nullptr),
	ParamDataSerialization(nullptr),
	Storage(StatementStorage::None),
	FieldBindHandle(nullptr),
	RowCount(0),
	FieldCount(0),
	CurrentRowCursor(0),
	RowDataSerialization(nullptr),
	ResultSetDataSerialization(nullptr),
	IsQueryDone(false)
{
}

SQLOperation::~SQLOperation()
{
	ClearParam();
	ClearResult();
}

void SQLOperation::ClearParam()
{
	if (ParamCount)
	{
		delete[] ParamDataSerialization;
		ParamDataSerialization = nullptr;
	}

	if (ParamBindHandle)
	{
		for (uint32 i = 0; i < ParamCount; ++i)
		{
			if (ParamBindHandle[i].buffer_type == MYSQL_TYPE_VAR_STRING ||
				ParamBindHandle[i].buffer_type == MYSQL_TYPE_BLOB)
			{
				delete[] ParamBindHandle[i].buffer;
			}
			delete ParamBindHandle[i].is_null;
			delete ParamBindHandle[i].length;
		}
	}

	delete[] ParamBindHandle;
	ParamBindHandle = nullptr;
}

void SQLOperation::ClearResult()
{
	if (FieldCount)
	{
		std::vector<uint32> extraMemoryItemIndex;

		for (uint32 i = 0; i < FieldCount; ++i)
		{
			// TODO mysql variable length type
			if (FieldBindHandle[i].buffer_type == MYSQL_TYPE_BLOB ||
				FieldBindHandle[i].buffer_type == MYSQL_TYPE_VAR_STRING)
			{
				extraMemoryItemIndex.push_back(i);
				delete[] FieldBindHandle[i].buffer;
			}
			delete FieldBindHandle[i].is_null;
			delete FieldBindHandle[i].length;
		}

		delete[] FieldBindHandle;
		FieldBindHandle = nullptr;

		for (uint64 row = 0; row < RowCount; ++row)
		{
			uint64* newRow = ResultSetDataSerialization + FieldCount*row;
			for (auto& index : extraMemoryItemIndex)
			{
				delete[] reinterpret_cast<void*>(*(newRow + index));
			}
		}

		delete[] ResultSetDataSerialization;
		ResultSetDataSerialization = nullptr;
		delete[] RowDataSerialization;
		RowDataSerialization = nullptr;
	}
}

void SQLOperation::SetConnection(DatabaseConnection* conn)
{
	MySqlConnectionHandle = conn;
}

void SQLOperation::SetStatement(MYSQL_STMT* stmt)
{
	MySqlStatementHandle = stmt;
	ParamCount = mysql_stmt_param_count(stmt);
	ParamBindHandle = new MYSQL_BIND[ParamCount];
	memset(ParamBindHandle, 0, sizeof(MYSQL_BIND)*ParamCount);
	ParamDataSerialization = new uint64[ParamCount];
	memset(ParamDataSerialization, 0, sizeof(uint64)*ParamCount);

	FieldCount = mysql_stmt_field_count(stmt);

	/// "If set to 1, causes mysql_stmt_store_result() to update the metadata MYSQL_FIELD->max_length value."
	my_bool bool_tmp = 1;
	mysql_stmt_attr_set(stmt, STMT_ATTR_UPDATE_MAX_LENGTH, &bool_tmp);
}

void SQLOperation::SetStatement(char* sql)
{
	MYSQL_STMT* stmt = MySqlConnectionHandle->MySqlStatementHandle;

	if (mysql_stmt_prepare(stmt, sql, strlen(sql)))
	{
		//TODO error log
		exit(EXIT_FAILURE);
	}

	SetStatement(stmt);
}

void SQLOperation::SetOperationFlag(SqlOperationFlag flag)
{
	OperationFlag = flag;
}

void SQLOperation::Execute()
{
	if (ParamCount)
	{
		mysql_stmt_bind_param(MySqlStatementHandle, ParamBindHandle);
	}

	if (mysql_stmt_execute(MySqlStatementHandle))
	{
		//TODO error log
		fprintf(stderr, " mysql_stmt_execute(), failed\n");
		fprintf(stderr, " %s\n", mysql_stmt_error(MySqlStatementHandle));
		exit(EXIT_FAILURE);
	}

	if (FieldCount)
	{
		// Init data serialization
		FieldBindHandle = new MYSQL_BIND[FieldCount];
		memset(FieldBindHandle, 0, sizeof(MYSQL_BIND)*FieldCount);
		RowDataSerialization = new uint64[FieldCount];
		memset(RowDataSerialization, 0, sizeof(uint64)*FieldCount);

		if (mysql_stmt_store_result(MySqlStatementHandle))
		{
			//TODO error log
			fprintf(stderr, " mysql_stmt_execute(), failed\n");
			fprintf(stderr, " %s\n", mysql_stmt_error(MySqlStatementHandle));
			exit(EXIT_FAILURE);
		}

		// get metadata
		MYSQL_RES* resultMetaData = mysql_stmt_result_metadata(MySqlStatementHandle);
		MYSQL_FIELD* resultDataFields = mysql_fetch_fields(resultMetaData);
		RowCount = (uint64)mysql_stmt_num_rows(MySqlStatementHandle);

		// bind result for fetching
		if (MySqlStatementHandle->bind_result_done)
		{
			delete[] MySqlStatementHandle->bind->length;
			delete[] MySqlStatementHandle->bind->is_null;
		}

		for (uint32 i = 0; i < FieldCount; ++i)
		{

			uint32 size = SizeForType(&resultDataFields[i]);

			if (resultDataFields[i].type == MYSQL_TYPE_VAR_STRING ||
				resultDataFields[i].type == MYSQL_TYPE_BLOB)
			{
				char* fieldBuffer = new char[size];
				RowDataSerialization[i] = reinterpret_cast<uint64&>(fieldBuffer);
				SetMySqlBind(&FieldBindHandle[i], resultDataFields[i].type,
					fieldBuffer, false, size, size);
			}
			else
			{
				SetMySqlBind(&FieldBindHandle[i], resultDataFields[i].type,
					&RowDataSerialization[i], resultDataFields[i].flags & UNSIGNED_FLAG);
			}
		}

		mysql_stmt_bind_result(MySqlStatementHandle, FieldBindHandle);
		ResultSetDataSerialization = new uint64[RowCount*FieldCount];

		uint32 rowIndex = 0;
		while (FetchNextRow())
		{
			for (uint32 fieldIndex = 0; fieldIndex < FieldCount; ++fieldIndex)
			{
				if (FieldBindHandle[fieldIndex].buffer_type == MYSQL_TYPE_BLOB ||
					FieldBindHandle[fieldIndex].buffer_type == MYSQL_TYPE_VAR_STRING)
				{
					uint32 fieldBufferSize = FieldBindHandle[fieldIndex].buffer_length;
					char* fieldBuffer = new char[fieldBufferSize];
					memcpy(fieldBuffer, (void*)RowDataSerialization[fieldIndex], fieldBufferSize);
					ResultSetDataSerialization[rowIndex*FieldCount + fieldIndex] = reinterpret_cast<uint64&>(fieldBuffer);
				}
				else
				{
					memcpy(&ResultSetDataSerialization[rowIndex*FieldCount + fieldIndex],
						&RowDataSerialization[fieldIndex], sizeof(uint64));
				}
			}
			++rowIndex;
		}
		mysql_stmt_free_result(MySqlStatementHandle);
	}
}

void SQLOperation::Call()
{
	Execute();
	IsQueryDone = true;
}

void SQLOperation::SetParamBool(uint8 index, bool&& value)
{
	SetParamUInt8(index, uint8(value));
}

void SQLOperation::SetParamUInt8(uint8 index, uint8&& value)
{
	ParamSetMask ^= 0x00000001 << index;
	ParamDataSerialization[index] = reinterpret_cast<uint64&>(value);
	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_TINY, &ParamDataSerialization[index], true);
}

void SQLOperation::SetParamInt8(uint8 index, int8&& value)
{
	ParamSetMask ^= 0x00000001 << index;
	ParamDataSerialization[index] = reinterpret_cast<uint64&>(value);
	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_TINY, &ParamDataSerialization[index], false);
}

void SQLOperation::SetParamUInt16(uint8 index, uint16&& value)
{
	ParamSetMask ^= 0x00000001 << index;
	ParamDataSerialization[index] = reinterpret_cast<uint64&>(value);
	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_SHORT, &ParamDataSerialization[index], true);
}

void SQLOperation::SetParamInt16(uint8 index, int16&& value)
{
	ParamSetMask ^= 0x00000001 << index;
	ParamDataSerialization[index] = reinterpret_cast<uint64&>(value);
	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_SHORT, &ParamDataSerialization[index], false);
}

void SQLOperation::SetParamUInt32(uint8 index, uint32&& value)
{
	ParamSetMask ^= 0x00000001 << index;
	ParamDataSerialization[index] = reinterpret_cast<uint64&>(value);
	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_LONG, &ParamDataSerialization[index], true);
}

void SQLOperation::SetParamInt32(uint8 index, int32&& value)
{
	ParamSetMask ^= 0x00000001 << index;
	ParamDataSerialization[index] = reinterpret_cast<uint64&>(value);
	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_LONG, &ParamDataSerialization[index], false);
}

void SQLOperation::SetParamUInt64(uint8 index, uint64&& value)
{
	ParamSetMask ^= 0x00000001 << index;
	ParamDataSerialization[index] = reinterpret_cast<uint64&>(value);
	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_LONGLONG, &ParamDataSerialization[index], true);
}

void SQLOperation::SetParamInt64(uint8 index, int64&& value)
{
	ParamSetMask ^= 0x00000001 << index;
	ParamDataSerialization[index] = reinterpret_cast<uint64&>(value);
	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_LONGLONG, &ParamDataSerialization[index], false);
}

void SQLOperation::SetParamFloat(uint8 index, float&& value)
{
	ParamSetMask ^= 0x00000001 << index;
	ParamDataSerialization[index] = reinterpret_cast<uint64&>(value);
	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_FLOAT, &ParamDataSerialization[index], (value > 0.0f));
}

void SQLOperation::SetParamDouble(uint8 index, double&& value)
{
	ParamSetMask ^= 0x00000001 << index;
	ParamDataSerialization[index] = reinterpret_cast<uint64&>(value);
	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_DOUBLE, &ParamDataSerialization[index], (value > 0.0f));
}

void SQLOperation::SetParamString(uint8 index, char const* value)
{
	ParamSetMask ^= 0x00000001 << index;

	uint32 len = uint32(strlen(value)) + 1;
	char* stringLocation = new char[len];
	memcpy(stringLocation, value, len);

	ParamDataSerialization[index] = reinterpret_cast<uint64&>(stringLocation);
	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_VAR_STRING, stringLocation, false, len, len - 1);
}

void SQLOperation::MoveParamString(uint8 index, char* value)
{
	ParamSetMask ^= 0x00000001 << index;

	uint32 len = uint32(strlen(value)) + 1;
	char* stringLocation = value;

	ParamDataSerialization[index] = reinterpret_cast<uint64&>(stringLocation);
	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_VAR_STRING, stringLocation, false, len, len - 1);
}

void SQLOperation::SetParamString(uint8 index, std::string& value)
{
	ParamSetMask ^= 0x00000001 << index;

	uint32 len = value.size() + 1;
	char* stringLocation = new char[len];
	memcpy(stringLocation, value.c_str(), len);

	ParamDataSerialization[index] = reinterpret_cast<uint64&>(stringLocation);
	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_VAR_STRING, stringLocation, false, len, len - 1);
}

void SQLOperation::MoveParamString(uint8 index, std::string&& value)
{
	ParamSetMask ^= 0x00000001 << index;

	uint32 len = value.size() + 1;
	const char* stringLocation = value.c_str();

	ParamDataSerialization[index] = reinterpret_cast<uint64&>(stringLocation);
	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_VAR_STRING, (void*)stringLocation, false, len, len - 1);
}

void SQLOperation::SetParamBinary(uint8 index, const void* value, uint32 dataSize)
{
	ParamSetMask ^= 0x00000001 << index;

	char* binaryLocation = new char[dataSize];
	memcpy(binaryLocation, value, dataSize);

	ParamDataSerialization[index] = reinterpret_cast<uint64&>(binaryLocation);
	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_BLOB, binaryLocation, false, dataSize, dataSize);
}

void SQLOperation::MoveParamBinary(uint8 index, const void* value, uint32 dataSize)
{
	ParamSetMask ^= 0x00000001 << index;

	const void* binaryLocation = value;

	ParamDataSerialization[index] = reinterpret_cast<uint64&>(binaryLocation);
	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_BLOB, (void*)binaryLocation, false, dataSize, dataSize);
}

void SQLOperation::SetParamBinary(uint8 index, std::vector<uint8>&& value)
{
	ParamSetMask ^= 0x00000001 << index;

	uint32 dataSize = value.size();
	char* binaryLocation = new char[dataSize];
	memcpy(binaryLocation, value.data(), dataSize);

	ParamDataSerialization[index] = reinterpret_cast<uint64&>(binaryLocation);
	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_BLOB, binaryLocation, false, dataSize, dataSize);
}

void SQLOperation::MoveParamBinary(uint8 index, std::vector<uint8>&& value)
{
	ParamSetMask ^= 0x00000001 << index;

	uint32 dataSize = value.size();
	void* binaryLocation = value.data();

	ParamDataSerialization[index] = reinterpret_cast<uint64&>(binaryLocation);
	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_BLOB, binaryLocation, false, dataSize, dataSize);
}

void SQLOperation::SetParamNull(uint8 index)
{
	ParamSetMask ^= 0x00000001 << index;

	SetMySqlBind(&ParamBindHandle[index], MYSQL_TYPE_NULL, NULL, false, 0, 0, true);
}

bool SQLOperation::GetNextRowOfResultSet()
{
	if (FieldCount && CurrentRowCursor < RowCount)
	{
		CurrentRowCursor++;
		return true;
	}
	else
	{
		return false;
	}
}

bool SQLOperation::GetBool(uint8 index)
{
	return bool(ResultSetDataSerialization[(CurrentRowCursor - 1)*FieldCount + index]);
}

uint8 SQLOperation::GetUInt8(uint8 index)
{
	return uint8(ResultSetDataSerialization[(CurrentRowCursor - 1)*FieldCount + index]);
}

int8 SQLOperation::GetInt8(uint8 index)
{
	return int8(ResultSetDataSerialization[(CurrentRowCursor - 1)*FieldCount + index]);
}

uint16 SQLOperation::GetUInt16(uint8 index)
{
	return uint16(ResultSetDataSerialization[(CurrentRowCursor - 1)*FieldCount + index]);
}

int16 SQLOperation::GetInt16(uint8 index)
{
	return int16(ResultSetDataSerialization[(CurrentRowCursor - 1)*FieldCount + index]);
}

uint32 SQLOperation::GetUInt32(uint8 index)
{
	return uint32(ResultSetDataSerialization[(CurrentRowCursor - 1)*FieldCount + index]);
}

int32 SQLOperation::GetInt32(uint8 index)
{
	return int32(ResultSetDataSerialization[(CurrentRowCursor - 1)*FieldCount + index]);
}

uint64 SQLOperation::GetUInt64(uint8 index)
{
	return uint64(ResultSetDataSerialization[(CurrentRowCursor - 1)*FieldCount + index]);
}

int64 SQLOperation::GetInt64(uint8 index)
{
	return int64(ResultSetDataSerialization[(CurrentRowCursor - 1)*FieldCount + index]);
}

float SQLOperation::GetFloat(uint8 index)
{
	return reinterpret_cast<float&>(ResultSetDataSerialization[(CurrentRowCursor - 1)*FieldCount + index]);
}

double SQLOperation::GetDouble(uint8 index)
{
	return reinterpret_cast<double&>(ResultSetDataSerialization[(CurrentRowCursor - 1)*FieldCount + index]);
}

char const* SQLOperation::GetCString(uint8 index)
{
	return reinterpret_cast<char const*>(ResultSetDataSerialization[(CurrentRowCursor - 1)*FieldCount + index]);
}

std::string SQLOperation::GetString(uint8 index)
{
	return std::string(reinterpret_cast<char const*>(ResultSetDataSerialization[(CurrentRowCursor - 1)*FieldCount + index]));
}

std::vector<uint8> SQLOperation::GetBinary(uint8 index, std::size_t size)
{
	std::vector<uint8> bytes(size, 0);
	void* binaryBuffer = reinterpret_cast<void*>(ResultSetDataSerialization[(CurrentRowCursor - 1)*FieldCount + index]);
	int i = 0;
	for (auto& byte : bytes)
	{
		byte = ((uint8*)(binaryBuffer))[i++];
	}
	return bytes;
}

void SQLOperation::SetMySqlBind(MYSQL_BIND* mySqlBind, enum_field_types bufferType, void* bufferLocation, bool isUnsigned, uint32 bufferLength /*= 0*/, uint32 dataSize /*= 0*/, bool isNull /*= 0*/)
{
	mySqlBind->buffer_type = bufferType;
	mySqlBind->buffer = bufferLocation;
	mySqlBind->buffer_length = bufferLength;
	mySqlBind->is_null_value = isNull;
	if (dataSize > 0)
	{
		if (mySqlBind->length)
		{
			*(mySqlBind->length) = dataSize;
		}
		else
		{
			mySqlBind->length = new unsigned long(dataSize);
		}
	}
	/*
	else
	{
		delete mySqlBind->length;
		mySqlBind->length = nullptr;
	}
	*/
	mySqlBind->is_unsigned = isUnsigned;
}

uint32 SQLOperation::SizeForType(MYSQL_FIELD* field)
{
	switch (field->type)
	{
	case MYSQL_TYPE_NULL:
		return 0;
	case MYSQL_TYPE_TINY:
		return 1;
	case MYSQL_TYPE_YEAR:
	case MYSQL_TYPE_SHORT:
		return 2;
	case MYSQL_TYPE_INT24:
	case MYSQL_TYPE_LONG:
	case MYSQL_TYPE_FLOAT:
		return 4;
	case MYSQL_TYPE_DOUBLE:
	case MYSQL_TYPE_LONGLONG:
	case MYSQL_TYPE_BIT:
		return 8;

	case MYSQL_TYPE_TIMESTAMP:
	case MYSQL_TYPE_DATE:
	case MYSQL_TYPE_TIME:
	case MYSQL_TYPE_DATETIME:
		return sizeof(MYSQL_TIME);

	case MYSQL_TYPE_TINY_BLOB:
	case MYSQL_TYPE_MEDIUM_BLOB:
	case MYSQL_TYPE_LONG_BLOB:
	case MYSQL_TYPE_BLOB:
	case MYSQL_TYPE_STRING:
	case MYSQL_TYPE_VAR_STRING:
		return field->max_length + 1;

	case MYSQL_TYPE_DECIMAL:
	case MYSQL_TYPE_NEWDECIMAL:
		return 64;

	case MYSQL_TYPE_GEOMETRY:
		/*
		Following types are not sent over the wire:
		MYSQL_TYPE_ENUM:
		MYSQL_TYPE_SET:
		*/
	default:
		//TODO error log
		return 0;
	}
}

bool SQLOperation::FetchNextRow()
{
	int retval = mysql_stmt_fetch(MySqlStatementHandle);
	return retval == 0 || retval == MYSQL_DATA_TRUNCATED;
}