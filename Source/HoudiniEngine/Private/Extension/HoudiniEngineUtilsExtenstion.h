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
		TArray<int>* SizesFixedArray = nullptr;
		bool bAttemptRunLengthEncoding = true;
	};
	// Dispatch Set Different Type of Data
	template<int32 Is>
	struct FHapiSetAttribDataDispatch
	{
		using Traits = FHapiStorageTraits<static_cast<HAPI_StorageType>(Is)>;
		static HAPI_Result Dispatch(
			const void* Array,
			const HAPI_Session * InSessionId,
			const HAPI_NodeId& InNodeId,
			const HAPI_PartId& InPartId,
			const FString& InAttributeName,
			const HAPI_AttributeInfo& InAttributeInfo,
			const FLastParam& LastParam
		);
	};
	// Dispatch Get Different Type of Data
	template<int32 Is>
	struct FHapiGetAttribDataDispatch
	{
		using Traits = FHapiStorageTraits<static_cast<HAPI_StorageType>(Is)>;
		static HAPI_Result Dispatch(
			void* Array,
			const HAPI_Session * InSessionId,
			const HAPI_NodeId& InNodeId,
			const HAPI_PartId& InPartId,
			const FString& InAttributeName,
			HAPI_AttributeInfo& InAttributeInfo,
			TArray<int>* SizesFixedArray
		);
	};
	// Common Set Data Function. Jump table dispatch internal
	static HAPI_Result HapiSetAttribData(
		const void* Array,
		const HAPI_Session* InSessionId,
		const HAPI_NodeId& InNodeId,
		const HAPI_PartId& InPartId,
		const FString& InAttributeName,
		const HAPI_AttributeInfo& InAttributeInfo,
		const FLastParam& LastParam
		)
	{
		static TJumpTable<
			FHapiSetAttribDataDispatch,
			TMakeIntegerSequence<int32,static_cast<int32>(HAPI_STORAGETYPE_MAX)>			
		> JumpTable;
		return JumpTable.Func[InAttributeInfo.storage](Array,InSessionId,InNodeId,InPartId,InAttributeName,InAttributeInfo,LastParam);
	}
	
	// Todo Common Get Data Function. Jump table dispatch internal
	static HAPI_Result HapiGetAttribData(
		void* Array,
		const HAPI_Session * InSessionId,
		const HAPI_NodeId& InNodeId,
		const HAPI_PartId& InPartId,
		const FString& InAttributeName,
		HAPI_AttributeInfo& InAttributeInfo,
		TArray<int>* SizesFixedArray
		)
	{
		static TJumpTable<
			FHapiGetAttribDataDispatch,
			TMakeIntegerSequence<int32,static_cast<int32>(HAPI_STORAGETYPE_MAX)>			
		> JumpTable;
		return JumpTable.Func[InAttributeInfo.storage](Array,InSessionId,InNodeId,InPartId,InAttributeName,InAttributeInfo,SizesFixedArray);
	}

	template <typename DataType>
	static TArray<int> RunLengthEncode(const DataType* Data, int TupleSize, int Count, const float MaxCompressionRatio = 0.25f)
	{
		// Run length encode the data.
		// If this function returns an empty array it means the desired compression ratio could not be met.

		auto CompareTuple = [TupleSize] (const DataType* StartA, const DataType* StartB)
		{
			for (int Index = 0; Index < TupleSize; Index++)
			{
				if (StartA[Index] != StartB[Index])
					return false;
			}
			return true;
		};

		TArray<int> EncodedData;
		if (Count == 0)
			return EncodedData;

		// Guess of size needed.
		EncodedData.Reserve(static_cast<int>(MaxCompressionRatio * Count));

		// The first run always begins on element zero.
		int Start = 0;
		EncodedData.Add(Start);

		// Created a run length encoded array based off the input data. eg.
		// [ 0, 0, 0, 1, 1, 2, 3 ] will return [ 0, 3, 5, 6]

		for(int Index = 0; Index < Count * TupleSize; Index += TupleSize)
		{
			if (!CompareTuple(&Data[Start], &Data[Index]))
			{
				// The value changed, so start a new run
				Start = Index;
				EncodedData.Add(Start / TupleSize);
			}
		}

		// Check we've made a decent compression ratio. If not return an empty array.
		float Ratio = float(EncodedData.Num() / float(Count));
		if (Ratio > MaxCompressionRatio)
			EncodedData.SetNum(0);

		return EncodedData;
	}
	
	// Set Attrib Data
	template<typename ElementType>
	static HAPI_Result HapiSetAttribData(
		const TArray<ElementType>* Array,
		const HAPI_Session * InSessionId,
		const HAPI_NodeId& InNodeId,
		const HAPI_PartId& InPartId,
		const FString& InAttributeName,
		const HAPI_AttributeInfo& InAttributeInfo,
		const bool bAttemptRunLengthEncoding 
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
			if(bAttemptRunLengthEncoding)
			{
				TArray<int> RunLengths = RunLengthEncode(Array->GetData(), InAttributeInfo.tupleSize, InAttributeInfo.count);
				if (RunLengths.Num() != 0)
				{
					for(int Index = 0; Index < RunLengths.Num(); Index++)
					{
						int StartIndex = RunLengths[Index];
						int EndIndex = InAttributeInfo.count;
						if (Index != RunLengths.Num() - 1)
							EndIndex = RunLengths[Index + 1];

						const ElementType* TupleValues = &Array->GetData()[StartIndex * InAttributeInfo.tupleSize];
						Result = (*Traits::UniqueExport)(
							InSessionId,
							InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
							&InAttributeInfo, TupleValues, InAttributeInfo.tupleSize,
							StartIndex, EndIndex - StartIndex);

						if (Result != HAPI_RESULT_SUCCESS)
							return  Result;
					}
					return HAPI_RESULT_SUCCESS;
				}
			}
			
			int32 ChunkSize = THRIFT_MAX_CHUNKSIZE / InAttributeInfo.tupleSize;
			if (InAttributeInfo.count > ChunkSize)
			{
				// Send the attribte in chunks
				for (int32 ChunkStart = 0; ChunkStart < InAttributeInfo.count; ChunkStart += ChunkSize)
				{
					int32 CurCount = InAttributeInfo.count - ChunkStart > ChunkSize ? ChunkSize : InAttributeInfo.count - ChunkStart;

					Result = (*Traits::Export)(
						InSessionId,
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
				Result = (*Traits::Export)(
					InSessionId,
					InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
					&InAttributeInfo, Array->GetData(),
					0, InAttributeInfo.count);
			}

			return Result;	
		}
	}
	// Set Array Attrib Data
	template<typename ElementType>
	static HAPI_Result HapiSetAttribArrayData(
		const TArray<ElementType>* Array,
		const HAPI_Session * InSessionId,
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

					Result = (*Traits::Export)(
						InSessionId,
						InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
						&InAttributeInfo, Array->GetData() + StringStart, NumSent,
						SizesFixedArray.GetData() + ChunkStart * InAttributeInfo.tupleSize, ChunkStart, CurCount);

					if (Result != HAPI_RESULT_SUCCESS)
						break;

					StringStart += NumSent;
				}
			}
			else if(InAttributeInfo.count > 0)
			{
				// Set all the attribute values once
				Result = (*Traits::Export)(
					InSessionId,
					InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
					&InAttributeInfo, Array->GetData(), Array->Num(),
					SizesFixedArray.GetData(),0, SizesFixedArray.Num());
			}
			else
			{
				int64 NotNull = 1;
				Result = (*Traits::Export)(
					InSessionId,
					InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
					&InAttributeInfo, reinterpret_cast<ElementType*>(&NotNull), 0,
					SizesFixedArray.GetData(),0, 0);
			}
			return Result;
		}
	}

	// Todo Get Attrib Data
	template<typename ElementType>
	static HAPI_Result HapiGetAttribData(
		TArray<ElementType>* Array,
		const HAPI_Session * InSessionId,
		const HAPI_NodeId& InNodeId,
		const HAPI_PartId& InPartId,
		const FString& InAttributeName,
		HAPI_AttributeInfo& InAttributeInfo
		)
	{
		if constexpr (std::is_same_v<ElementType,FString>)
		{
			bool Result = FHoudiniEngineUtils::HapiGetAttributeDataAsStringFromInfo(InNodeId,InPartId,TCHAR_TO_ANSI(*InAttributeName),InAttributeInfo,*Array);
			return Result?HAPI_RESULT_SUCCESS:HAPI_RESULT_FAILURE;
		}
		else
		{
			using Traits = FHapiStorageTraits<HapiStorageTraits::GetStorageTypeOfCppType<ElementType,false>()>;
			int32 CountResize = InAttributeInfo.count;
			Array->Reserve(CountResize*InAttributeInfo.tupleSize*2);
			Array->SetNumZeroed(CountResize*InAttributeInfo.tupleSize);
			
			///////////
			if (InAttributeInfo.count <= 0 || InAttributeInfo.tupleSize < 1)
				return HAPI_RESULT_INVALID_ARGUMENT;

			HAPI_Result Result = HAPI_RESULT_FAILURE;
			
			// Get all the attribute values once
			Result = (*Traits::Import)(
				InSessionId,
				InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
				&InAttributeInfo, -1, Array->GetData(),
				0, InAttributeInfo.count);
			
			return Result;	
		}
	}
	// Todo Get Array Attrib Data
	template<typename ElementType>
	static HAPI_Result HapiGetAttribArrayData(
		TArray<ElementType>* Array,
		const HAPI_Session * InSessionId,
		const HAPI_NodeId& InNodeId,
		const HAPI_PartId& InPartId,
		const FString& InAttributeName,
		HAPI_AttributeInfo& InAttributeInfo,
		TArray<int>& SizesFixedArray
		)
	{
		if constexpr (std::is_same_v<ElementType,FString>)
		{
			return HapiGetAttributeStringArrayData(*Array,InSessionId,InNodeId,InPartId,InAttributeName,InAttributeInfo,SizesFixedArray);
		}
		else
		{
			using Traits = FHapiStorageTraits<HapiStorageTraits::GetStorageTypeOfCppType<ElementType,true>()>;

			// Send strings in smaller chunks due to their potential size

			HAPI_Result Result = HAPI_RESULT_FAILURE;
			if(InAttributeInfo.totalArrayElements)
				Array->SetNumUninitialized(1);
			Array->Reserve(InAttributeInfo.totalArrayElements);
			Array->SetNumZeroed(InAttributeInfo.totalArrayElements*2);
			
			// Set all the attribute values once
			Result = (*Traits::Import)(
				InSessionId,
				InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
				&InAttributeInfo, Array->GetData(), InAttributeInfo.totalArrayElements,
				SizesFixedArray.GetData(),0, InAttributeInfo.count);
			
			return Result;
		}
	}

	static HAPI_Result HapiGetAttributeStringArrayData(
		TArray<FString>& Array,
		const HAPI_Session * InSessionId,
		const HAPI_NodeId& InNodeId,
		const HAPI_PartId& InPartId,
		const FString& InAttributeName,
		HAPI_AttributeInfo& InAttributeInfo,
		TArray<int>& SizesFixedArray
		)
	{
		HAPI_Result Result = HAPI_RESULT_FAILURE;
		if(!InAttributeInfo.exists)
			return Result;
		TArray<HAPI_StringHandle> StringHandles;
		StringHandles.SetNumUninitialized(InAttributeInfo.totalArrayElements);
		for(int32 i = 0, n = StringHandles.Num(); i<n;i++)
			StringHandles[i] = -1;
		

		// Set the data size
		Array.SetNum(StringHandles.Num());
		SizesFixedArray.SetNum(InAttributeInfo.count);

		Result = FHoudiniApi::GetAttributeStringArrayData(
			InSessionId,
			InNodeId,InPartId,TCHAR_TO_ANSI(*InAttributeName),
			&InAttributeInfo, StringHandles.GetData(),
			InAttributeInfo.totalArrayElements,
			SizesFixedArray.GetData(),
			0,InAttributeInfo.count);

		// Convert the StringHandles to FString.
		// using a map to minimize the number of HAPI calls
		FHoudiniEngineString::SHArrayToFStringArray(StringHandles, Array);
		return Result;
	}
};
// Impl Dispatch
template <int32 Is>
HAPI_Result FHoudiniEngineUtilsExtenstion::FHapiSetAttribDataDispatch<Is>::Dispatch(
	const void* Array,
	const HAPI_Session * InSessionId,
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId, 
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo,
	const FLastParam& LastParam)
{
	// FHapiStorageTraits<static_cast<HAPI_StorageType>(Is)>::Export;
	if constexpr (HapiStorageTraits::IsArrayStorage(static_cast<HAPI_StorageType>(Is)))	
		return FHoudiniEngineUtilsExtenstion::HapiSetAttribArrayData(static_cast<const TArray<typename Traits::Type>*>(Array),InSessionId,InNodeId,InPartId,InAttributeName,InAttributeInfo,*LastParam.SizesFixedArray);
	else	
		return FHoudiniEngineUtilsExtenstion::HapiSetAttribData(static_cast<const TArray<typename Traits::Type>*>(Array),InSessionId,InNodeId,InPartId,InAttributeName,InAttributeInfo,LastParam.bAttemptRunLengthEncoding);
}
// Todo Impl Dispatch
template <int32 Is>
HAPI_Result FHoudiniEngineUtilsExtenstion::FHapiGetAttribDataDispatch<Is>::Dispatch(
	void* Array,
	const HAPI_Session * InSessionId,
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId,
	const FString& InAttributeName,
	HAPI_AttributeInfo& InAttributeInfo,
	TArray<int>* SizesFixedArray)
{
	// FHapiStorageTraits<static_cast<HAPI_StorageType>(Is)>::Export;
	if constexpr (HapiStorageTraits::IsArrayStorage(static_cast<HAPI_StorageType>(Is)))	
		return FHoudiniEngineUtilsExtenstion::HapiGetAttribArrayData(static_cast<TArray<typename Traits::Type>*>(Array),InSessionId,InNodeId,InPartId,InAttributeName,InAttributeInfo,*SizesFixedArray);
	else	
		return FHoudiniEngineUtilsExtenstion::HapiGetAttribData(static_cast<TArray<typename Traits::Type>*>(Array),InSessionId,InNodeId,InPartId,InAttributeName,InAttributeInfo);
}
