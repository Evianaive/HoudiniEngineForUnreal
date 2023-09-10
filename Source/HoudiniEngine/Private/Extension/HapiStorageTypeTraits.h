// Copyright Evianaive, Inc. All Rights Reserved.

#pragma once

#include "HoudiniApi.h"
#define DECLARE_HAPI_STORAGE_TRAITS(Name)\
static inline auto Export = &FHoudiniApi::SetAttribute##Name##Data;\
static inline auto Import = &FHoudiniApi::GetAttribute##Name##Data;\
// auto ExportUnique = &FHoudiniApi::SetAttribute##Name##Data\

namespace HapiStorageTraits
{
	template<int32... Is>
	int32* GetHAPI_StorageTypeSizesInternal(TIntegerSequence<int32,Is...>)
	{
		static int32 HAPI_StorageTypeSizes[HAPI_STORAGETYPE_MAX] = {Is...};
		return HAPI_StorageTypeSizes;
	}
	static int32 GetHAPI_StorageTypeSizesInternal(HAPI_StorageType Type)
	{
		return GetHAPI_StorageTypeSizesInternal(TMakeIntegerSequence<int32,static_cast<int32>(HAPI_STORAGETYPE_MAX)>())[Type];
	}
	constexpr bool IsArrayStorage(HAPI_StorageType StorageType)
	{
		return static_cast<int32>(StorageType) >= static_cast<int32>(HAPI_STORAGETYPE_MAX)/2;
	}
	constexpr HAPI_StorageType GetArrayStorage(HAPI_StorageType StorageType)
	{
		if (IsArrayStorage(StorageType))
			return StorageType;
		return static_cast<HAPI_StorageType>(static_cast<int32>(StorageType) + static_cast<int32>(HAPI_STORAGETYPE_MAX)/2);
	}
	using FHapiStorageTraitsTypeTuple = TTuple<int32,int64,float,double,FString,uint8,int8,int16>;
	template<typename ElementType, bool bArray>
	constexpr HAPI_StorageType GetStorageTypeOfCppType()
	{
		constexpr HAPI_StorageType BaseStorageType = static_cast<HAPI_StorageType>(TTupleIndex<ElementType,FHapiStorageTraitsTypeTuple>::Value);
		return bArray ? GetArrayStorage(BaseStorageType) : BaseStorageType;
	}
}

template<HAPI_StorageType StorageType>
struct FHapiStorageTraits;

template<> struct FHapiStorageTraits<HAPI_STORAGETYPE_INT>{enum{Size=4};using Type = int32;DECLARE_HAPI_STORAGE_TRAITS(Int)};
template<> struct FHapiStorageTraits<HAPI_STORAGETYPE_INT64>{enum{Size=8};using Type = int64;DECLARE_HAPI_STORAGE_TRAITS(Int64)};
template<> struct FHapiStorageTraits<HAPI_STORAGETYPE_FLOAT>{enum{Size=4};using Type = float;DECLARE_HAPI_STORAGE_TRAITS(Float)};
template<> struct FHapiStorageTraits<HAPI_STORAGETYPE_FLOAT64>{enum{Size=8};using Type = double;DECLARE_HAPI_STORAGE_TRAITS(Float64)};
template<> struct FHapiStorageTraits<HAPI_STORAGETYPE_STRING>{enum{Size=24};using Type = FString;DECLARE_HAPI_STORAGE_TRAITS(String)};
template<> struct FHapiStorageTraits<HAPI_STORAGETYPE_UINT8>{enum{Size=1};using Type = uint8;DECLARE_HAPI_STORAGE_TRAITS(UInt8)};
template<> struct FHapiStorageTraits<HAPI_STORAGETYPE_INT8>{enum{Size=1};using Type = int8;DECLARE_HAPI_STORAGE_TRAITS(Int8)};
template<> struct FHapiStorageTraits<HAPI_STORAGETYPE_INT16>{enum{Size=2};using Type = int16;DECLARE_HAPI_STORAGE_TRAITS(Int16)};
template<> struct FHapiStorageTraits<HAPI_STORAGETYPE_INT_ARRAY>{enum{Size=4};using Type = int32;DECLARE_HAPI_STORAGE_TRAITS(IntArray)};
template<> struct FHapiStorageTraits<HAPI_STORAGETYPE_INT64_ARRAY>{enum{Size=8};using Type = int64;DECLARE_HAPI_STORAGE_TRAITS(Int64Array)};
template<> struct FHapiStorageTraits<HAPI_STORAGETYPE_FLOAT_ARRAY>{enum{Size=4};using Type = float;DECLARE_HAPI_STORAGE_TRAITS(FloatArray)};
template<> struct FHapiStorageTraits<HAPI_STORAGETYPE_FLOAT64_ARRAY>{enum{Size=8};using Type = double;DECLARE_HAPI_STORAGE_TRAITS(Float64Array)};
template<> struct FHapiStorageTraits<HAPI_STORAGETYPE_STRING_ARRAY>{enum{Size=24};using Type = FString;DECLARE_HAPI_STORAGE_TRAITS(StringArray)};
template<> struct FHapiStorageTraits<HAPI_STORAGETYPE_UINT8_ARRAY>{enum{Size=1};using Type = uint8;DECLARE_HAPI_STORAGE_TRAITS(UInt8Array)};
template<> struct FHapiStorageTraits<HAPI_STORAGETYPE_INT8_ARRAY>{enum{Size=1};using Type = int8;DECLARE_HAPI_STORAGE_TRAITS(Int8Array)};
template<> struct FHapiStorageTraits<HAPI_STORAGETYPE_INT16_ARRAY>{enum{Size=2};using Type = int16;DECLARE_HAPI_STORAGE_TRAITS(Int16Array)};

