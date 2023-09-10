// Copyright Evianaive, Inc. All Rights Reserved.

#pragma once

#include "HoudiniEngineUtils.h"
#include "HapiStorageTypeTraits.h"
#include "HoudiniEngine.h"
#include "JumpTable.h"

#ifdef THRIFT_MAX_CHUNKSIZE
#else
// Maximum size of the data that can be sent via thrift
//#define THRIFT_MAX_CHUNKSIZE			100 * 1024 * 1024 // This is supposedly the current limit in thrift, but still seems to be too large
#define THRIFT_MAX_CHUNKSIZE			10 * 1024 * 1024
//#define THRIFT_MAX_CHUNKSIZE			2048 * 2048
//#define THRIFT_MAX_CHUNKSIZE_STRING		256 * 256
#endif

struct FHoudiniEngineUtilsExtenstion : public FHoudiniEngineUtils
{
	struct FLastParam
	{
		TArray<int> SizesFixedArray;
	};
	template<int32 Is>
	struct FHapiSetAttribDataDispatch
	{
		using Traits = FHapiStorageTraits<static_cast<HAPI_StorageType>(Is)>;
		static HAPI_Result Dispatch(
			const void* Array,
			const HAPI_NodeId& InNodeId,
			const HAPI_PartId& InPartId,
			const FString& InAttributeName,
			const HAPI_AttributeInfo& InAttributeInfo,
			const FLastParam& LastParam
		);
	};
	static HAPI_Result HapiSetAttribData(
		const void* Array,
		const HAPI_NodeId& InNodeId,
		const HAPI_PartId& InPartId,
		const FString& InAttributeName,
		const HAPI_AttributeInfo& InAttributeInfo,
		const FLastParam& LastParam
		)
	{
		static TJumpTable<
			FHapiSetAttribDataDispatch,
			TIntegerSequence<int32,static_cast<int32>(HAPI_STORAGETYPE_MAX)>
		> JumpTable;
		JumpTable.Func[InAttributeInfo.storage](Array,InNodeId,InPartId,InAttributeName,InAttributeInfo,LastParam);
	}

	template<typename ElementType>
	static HAPI_Result HapiSetAttribData(
		const TArray<ElementType>* Array,
		const HAPI_NodeId& InNodeId,
		const HAPI_PartId& InPartId,
		const FString& InAttributeName,
		const HAPI_AttributeInfo& InAttributeInfo
		)
	{
		if constexpr (std::is_same_v<ElementType,FString>)
		{
			return HapiSetAttributeStringData(*Array,InNodeId,InPartId,InAttributeName,InAttributeInfo);
		}
		else
		{
			using Traits = FHapiStorageTraits<HapiStorageTraits::GetStorageTypeOfCppType<ElementType,false>()>;
			if (InAttributeInfo.count <= 0 || InAttributeInfo.tupleSize < 1)
				return HAPI_RESULT_INVALID_ARGUMENT;

			HAPI_Result Result = HAPI_RESULT_FAILURE;
			int32 ChunkSize = THRIFT_MAX_CHUNKSIZE / InAttributeInfo.tupleSize;
			if (InAttributeInfo.count > ChunkSize)
			{
				// Send the attribte in chunks
				for (int32 ChunkStart = 0; ChunkStart < InAttributeInfo.count; ChunkStart += ChunkSize)
				{
					int32 CurCount = InAttributeInfo.count - ChunkStart > ChunkSize ? ChunkSize : InAttributeInfo.count - ChunkStart;

					Result = Traits::Export(
						FHoudiniEngine::Get().GetSession(),
						InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
						&InAttributeInfo, Array->GetData() + ChunkStart * InAttributeInfo.tupleSize,
						ChunkStart, CurCount);

					if (Result != HAPI_RESULT_SUCCESS)
						break;
				}
			}
			else
			{
				// Send all the attribute values once
				Result = Traits::Export(
					FHoudiniEngine::Get().GetSession(),
					InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
					&InAttributeInfo, Array->GetData(),
					0, InAttributeInfo.count);
			}

			return Result;	
		}
	}
	template<typename ElementType>
	static HAPI_Result HapiSetAttribArrayData(
		const TArray<ElementType>* Array,
		const HAPI_NodeId& InNodeId,
		const HAPI_PartId& InPartId,
		const FString& InAttributeName,
		const HAPI_AttributeInfo& InAttributeInfo,
		const TArray<int>& SizesFixedArray
		)
	{
		if constexpr (std::is_same_v<ElementType,FString>)
		{
			return HapiSetAttributeStringArrayData(*Array,InNodeId,InPartId,InAttributeName,InAttributeInfo,SizesFixedArray);
		}
		else
		{
			using Traits = FHapiStorageTraits<HapiStorageTraits::GetStorageTypeOfCppType<ElementType,true>()>;

			// Send strings in smaller chunks due to their potential size
			int32 ChunkSize = (THRIFT_MAX_CHUNKSIZE / 100) / InAttributeInfo.tupleSize;

			HAPI_Result Result = HAPI_RESULT_FAILURE;
			if (InAttributeInfo.count > ChunkSize)
			{
				// Set the attributes in chunks
				int32 StringStart = 0;
				for (int32 ChunkStart = 0; ChunkStart < InAttributeInfo.count; ChunkStart += ChunkSize)
				{
					int32 CurCount = SizesFixedArray.Num() - ChunkStart > ChunkSize ? ChunkSize : SizesFixedArray.Num() - ChunkStart;
					int32 NumSent = 0;
					for (int32 Idx = 0; Idx < CurCount; ++Idx)
					{
						NumSent += SizesFixedArray[Idx + ChunkStart * InAttributeInfo.tupleSize];
					}

					Result = FHoudiniApi::SetAttributeStringArrayData(
						FHoudiniEngine::Get().GetSession(),
						InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
						&InAttributeInfo, Array->GetData() + StringStart, NumSent,
						SizesFixedArray.GetData() + ChunkStart * InAttributeInfo.tupleSize, ChunkStart, CurCount);

					if (Result != HAPI_RESULT_SUCCESS)
						break;

					StringStart += NumSent;
				}
			}
			else
			{
				// Set all the attribute values once
				Result = FHoudiniApi::SetAttributeStringArrayData(
					FHoudiniEngine::Get().GetSession(),
					InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
					&InAttributeInfo, Array->GetData(), Array->Num(),
					SizesFixedArray.GetData(),0, SizesFixedArray.Num());
			}
			return Result;
		}
	}
};

template <int32 Is>
HAPI_Result FHoudiniEngineUtilsExtenstion::FHapiSetAttribDataDispatch<Is>::Dispatch(
	const void* Array,
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId, 
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo,
	const FLastParam& LastParam)
{
	// FHapiStorageTraits<static_cast<HAPI_StorageType>(Is)>::Export;
	if constexpr (HapiStorageTraits::IsArrayStorage(static_cast<HAPI_StorageType>(Is)))
		return HapiSetAttribData(static_cast<const TArray<typename Traits::Type>*>(Array),InNodeId,InPartId,InAttributeName,InAttributeInfo);
	else
		return HapiSetAttribArrayData(static_cast<const TArray<typename Traits::Type>*>(Array),InNodeId,InPartId,InAttributeName,InAttributeInfo,LastParam.SizesFixedArray);
}
