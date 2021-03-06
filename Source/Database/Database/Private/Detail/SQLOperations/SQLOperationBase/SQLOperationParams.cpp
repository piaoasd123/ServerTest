#include "Private/Detail/SQLOperations/SQLOperationBase/SQLOperationParams.h"

void SQLOperationParams::ClearParams()
{
	if (ParamCount)
	{
		delete[] ParamData;
		ParamData = nullptr;
	}

	if (ParamBinds)
	{
		for (uint32 i = 0; i < ParamCount; ++i)
		{
			if (ParamBinds[i].buffer_type == MYSQL_TYPE_VAR_STRING ||
				ParamBinds[i].buffer_type == MYSQL_TYPE_BLOB)
			{
				delete[] ParamBinds[i].buffer;
			}
			delete ParamBinds[i].is_null;
			delete ParamBinds[i].length;
		}
	}

	delete[] ParamBinds;
	ParamBinds = nullptr;
}

void SQLOperationParams::SetBool(uint8 Index, const bool& Value)
{
	ParamSetBitMask |= 1 << Index;
	ParamData[Index] = reinterpret_cast<const uint64&>(Value);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_TINY, &ParamData[Index], true);
}

void SQLOperationParams::SetUInt8(uint8 Index, const uint8& Value)
{
	ParamSetBitMask |= 1 << Index;
	ParamData[Index] = reinterpret_cast<const uint64&>(Value);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_TINY, &ParamData[Index], true);
}

void SQLOperationParams::SetUInt16(uint8 Index, const uint16& Value)
{
	ParamSetBitMask |= 1 << Index;
	ParamData[Index] = reinterpret_cast<const uint64&>(Value);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_SHORT, &ParamData[Index], true);
}

void SQLOperationParams::SetUInt32(uint8 Index, const uint32& Value)
{
	ParamSetBitMask |= 1 << Index;
	ParamData[Index] = reinterpret_cast<const uint64&>(Value);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_LONG, &ParamData[Index], true);
}

void SQLOperationParams::SetUInt64(uint8 Index, const uint64& Value)
{
	ParamSetBitMask |= 1 << Index;
	ParamData[Index] = reinterpret_cast<const uint64&>(Value);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_LONGLONG, &ParamData[Index], true);
}

void SQLOperationParams::SetInt8(uint8 Index, const int8& Value)
{
	ParamSetBitMask |= 1 << Index;
	ParamData[Index] = reinterpret_cast<const uint64&>(Value);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_TINY, &ParamData[Index], false);
}

void SQLOperationParams::SetInt16(uint8 Index, const int16& Value)
{
	ParamSetBitMask |= 1 << Index;
	ParamData[Index] = reinterpret_cast<const uint64&>(Value);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_SHORT, &ParamData[Index], false);
}

void SQLOperationParams::SetInt32(uint8 Index, const int32& Value)
{
	ParamSetBitMask |= 1 << Index;
	ParamData[Index] = reinterpret_cast<const uint64&>(Value);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_LONG, &ParamData[Index], false);
}

void SQLOperationParams::SetInt64(uint8 Index, const int64& Value)
{
	ParamSetBitMask |= 1 << Index;
	ParamData[Index] = reinterpret_cast<const uint64&>(Value);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_LONGLONG, &ParamData[Index], false);
}

void SQLOperationParams::SetFloat(uint8 Index, const float& Value)
{
	ParamSetBitMask |= 1 << Index;
	ParamData[Index] = reinterpret_cast<const uint64&>(Value);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_FLOAT, &ParamData[Index], (Value > 0.0f));
}

void SQLOperationParams::SetDouble(uint8 Index, const double& Value)
{
	ParamSetBitMask |= 1 << Index;
	ParamData[Index] = reinterpret_cast<const uint64&>(Value);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_DOUBLE, &ParamData[Index], (Value > 0.0f));
}

void SQLOperationParams::SetString(uint8 Index, const char* Value)
{
	ParamSetBitMask |= 1 << Index;

	size_t len = strlen(Value) + 1;
	char* stringLocation = new char[len];
	memcpy(stringLocation, Value, len);

	ParamData[Index] = reinterpret_cast<uint64&>(stringLocation);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_VAR_STRING, stringLocation, false, len, len - 1);
}

void SQLOperationParams::SetString(uint8 Index, const std::string& Value)
{
	ParamSetBitMask |= 1 << Index;

	size_t len = Value.size() + 1;
	char* stringLocation = new char[len];
	memcpy(stringLocation, Value.c_str(), len);

	ParamData[Index] = reinterpret_cast<uint64&>(stringLocation);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_VAR_STRING, stringLocation, false, len, len - 1);
}

void SQLOperationParams::SetBinary(uint8 Index, const void* Value, uint32 dataSize)
{
	ParamSetBitMask |= 1 << Index;

	char* binaryLocation = new char[dataSize];
	memcpy(binaryLocation, Value, dataSize);

	ParamData[Index] = reinterpret_cast<uint64&>(binaryLocation);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_BLOB, binaryLocation, false, dataSize, dataSize);
}

void SQLOperationParams::SetBinary(uint8 Index, const std::vector<uint8>& Value)
{
	ParamSetBitMask |= 1 << Index;

	size_t dataSize = Value.size();
	char* binaryLocation = new char[dataSize];
	memcpy(binaryLocation, Value.data(), dataSize);

	ParamData[Index] = reinterpret_cast<uint64&>(binaryLocation);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_BLOB, binaryLocation, false, dataSize, dataSize);
}

void SQLOperationParams::MoveString(uint8 Index, char*& Value)
{
	ParamSetBitMask |= 1 << Index;
	size_t len = strlen(Value) + 1;
	ParamData[Index] = reinterpret_cast<uint64&>(Value);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_VAR_STRING, Value, false, len, len - 1);
	Value = nullptr;
}

void SQLOperationParams::MoveString(uint8 Index, std::string&& Value)
{
	ParamSetBitMask |= 1 << Index;
	size_t Length = Value.length() + 1; // include null terminator
	char* CStringPointer = const_cast<char*>(reinterpret_cast<const char*>(Value.c_str()));
	char* StringPointer = reinterpret_cast<char*>(&Value);

	char* MemBlock = nullptr;
	{
		if (CStringPointer > StringPointer && CStringPointer < (StringPointer + sizeof(std::string)))
		{
			// <= 15 chars, in-struct storage. alloc new char array.
			MemBlock = new char[Length];
			memcpy(MemBlock, Value.c_str(), Length);
		}
		else
		{
			// > 15 chars. take the memory block out from input string
			char Temp[sizeof(std::string)] = { 0 };
			std::string* TempString = reinterpret_cast<std::string*>(&Temp);
			(*TempString).swap(Value);
			MemBlock = const_cast<char*>(TempString->c_str());
		}
	}

	//Value = "abcdefghijklmnop";
	//Value = "abcd" "efgh" "ijkl" "mno";
	//Value.shrink_to_fit();

	ParamData[Index] = reinterpret_cast<uint64&>(MemBlock);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_VAR_STRING, const_cast<void*>(reinterpret_cast<const void*>(MemBlock)), false, Length, Length - 1);
}

void SQLOperationParams::MoveBinary(uint8 Index, void*& Value, uint32 dataSize)
{
	ParamSetBitMask |= 1 << Index;
	ParamData[Index] = reinterpret_cast<uint64&>(Value);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_BLOB, (void*)Value, false, dataSize, dataSize);
	Value = nullptr;
}

void SQLOperationParams::MoveBinary(uint8 Index, std::vector<uint8>&& Value)
{
	ParamSetBitMask |= 1 << Index;

	size_t dataSize = Value.size();
	void* binaryLocation = Value.data();

	ParamData[Index] = reinterpret_cast<uint64&>(binaryLocation);
	SetBind(&ParamBinds[Index], MYSQL_TYPE_BLOB, binaryLocation, false, dataSize, dataSize);
}

void SQLOperationParams::SetNull(uint8 Index)
{
	ParamSetBitMask |= 1 << Index;

	SetBind(&ParamBinds[Index], MYSQL_TYPE_NULL, NULL, false, 0, 0, true);
}

void SQLOperationParams::SetBind(MYSQL_BIND* mySqlBind, enum_field_types bufferType, void* bufferLocation, bool isUnsigned, size_t bufferLength /*= 0*/, size_t dataSize /*= 0*/, bool isNull /*= 0*/)
{
	mySqlBind->buffer_type = bufferType;
	mySqlBind->buffer = bufferLocation;
	mySqlBind->buffer_length = uint32(bufferLength);
	mySqlBind->is_null_value = isNull;
	if (dataSize > 0)
	{
		if (mySqlBind->length)
		{
			*(mySqlBind->length) = uint32(dataSize);
		}
		else
		{
			// todo: create out own bind class to wrap length instead of malloc
			// edit: actually, check if length_value works instead
			mySqlBind->length_value = uint32(dataSize);
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