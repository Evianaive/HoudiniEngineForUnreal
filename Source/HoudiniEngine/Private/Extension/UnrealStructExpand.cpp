// Copyright Evianaive, Inc. All Rights Reserved.

#include "UnrealStructExpand.h"

#include "HapiStorageTypeTraits.h"
#include "HoudiniApi.h"


TMap<UScriptStruct*,FStructConvertSpecialization> FStructConvertSpecialization::RegisteredSpecializations;

#pragma region Template

template<typename TIntSeq>
struct TIntSeqHelper;
template<int32... Is>
struct TIntSeqHelper<TIntegerSequence<int32,Is...>>
{
	static constexpr int32 Find(int32 i)
	{
		bool bFound = false;
		return (... + (bFound |= i==Is,(bFound?0:1)));
	}
	static constexpr bool Contains(int32 i)
	{
		return (... || (i==Is));
	}
};

template<typename TReOrderSeq>
struct TLocalReOrder;
template<int32... Is>
struct TLocalReOrder<TIntegerSequence<int32,Is...>> : public TIntSeqHelper<TIntegerSequence<int32,Is...>>
{
	using TIntSeqHelper<TIntegerSequence<int32,Is...>>::Find;
	template<typename ElementType, bool bUE2Hou>
	static void ReOrder(uint8* Data)
	{
		constexpr int32 ArraySize = sizeof...(Is);
		static int32 Order[ArraySize] = {(bUE2Hou?Is:Find(Find(Is)))...};
		ElementType* TypedData = reinterpret_cast<ElementType*>(Data);

		int32 ReOrderRingStart = 0;
		int32 ReOrderFrom = ReOrderRingStart;
		bool bHasOrdered[ArraySize] = {(Is,false)...};
		while (true)
		{
			if((Order[ReOrderFrom] == ReOrderRingStart))
			{
				ReOrderRingStart++;
				if(bHasOrdered[ReOrderRingStart])
					continue;
				if(ReOrderRingStart == ArraySize)
					break;
				ReOrderFrom = ReOrderRingStart;
			}
			else
			{
				FMemory::Memswap(TypedData+ReOrderFrom,TypedData+Order[ReOrderFrom],sizeof(ElementType));
				ReOrderFrom = Order[ReOrderFrom];
			}
		}
	}
};

template<typename TReOrderSeq>
struct TLocalScale;
template<int32... Is>
struct TLocalScale<TIntegerSequence<int32,Is...>> : public TIntSeqHelper<TIntegerSequence<int32,Is...>>
{
	using TIntSeqHelper<TIntegerSequence<int32,Is...>>::Contains;
	template<typename ElementType, bool bUE2Hou>
	static void Scale(uint8* Data)
	{
		constexpr int32 ArraySize = sizeof...(Is);
		if constexpr (!ArraySize)
		{
		}
		else
		{
			static int32 ScaleMask[ArraySize] = {Is...};
			ElementType* TypedData = reinterpret_cast<ElementType*>(Data);

			for(int32 i : ScaleMask)
			{
				if constexpr (bUE2Hou)
				{
					TypedData[i] /=  HAPI_UNREAL_SCALE_FACTOR_POSITION;	
				}
				else
				{
					TypedData[i] *=  HAPI_UNREAL_SCALE_FACTOR_POSITION;
				}
			}
		}
	}
};

template<typename TReOrderSeq, typename TScaleSeq = TReOrderSeq>
struct TLocalReOrderAndScale
{
	template<typename ElementType, bool bUE2Hou>
	static void ReOrderAndScale(uint8* Data)
	{
		TLocalReOrder<TReOrderSeq>::template ReOrder<ElementType,bUE2Hou>(Data);
		TLocalScale<TReOrderSeq>::template Scale<ElementType,bUE2Hou>(Data);
	}
};

using TRotatorCoordConvert = TLocalReOrder<TIntegerSequence<int32,2,0,1>>;
using TVectorCoordConvert = TLocalReOrderAndScale<TIntegerSequence<int32,0,2,1>,TMakeIntegerSequence<int32,3>>;
using TColorCoordConvert = TLocalReOrderAndScale<TIntegerSequence<int32,2,0,1,3>,TIntegerSequence<int32>>;
using TMatrixCoordConvert = TLocalReOrderAndScale<TIntegerSequence<int32,0,2,1,3,8,10,9,11,4,6,5,7,12,14,13,15>,TIntegerSequence<int32,12,13,14>>;

template<bool bUE2Hou, typename ElementType>
static void ConvertInternal(UE::Math::TQuat<ElementType>* Quat)
{
	Swap(Quat->Y,Quat->Z);
	Quat->W *= -1;
}

template<bool bUE2Hou, typename ElementType>
static void ConvertInternal(UE::Math::TRotator<ElementType>* Rotator)
{
	if constexpr (!bUE2Hou)
	{
		TRotatorCoordConvert::ReOrder<ElementType,bUE2Hou>(reinterpret_cast<uint8*>(Rotator));
		Rotator->Pitch *= -1;
		Rotator->Roll *= -1;
	}
	auto Quat = Rotator->Quaternion();
	ConvertInternal<bUE2Hou>(&Quat);
	if constexpr (bUE2Hou)
	{
		*Rotator = Quat.Rotator();
		Rotator->Roll *= -1;
		Rotator->Pitch *= -1;
		TRotatorCoordConvert::ReOrder<ElementType,bUE2Hou>(reinterpret_cast<uint8*>(Rotator));
	}
}

template<bool bUE2Hou, typename DataType>
static void Convert(uint8* Data)
{
	ConvertInternal<bUE2Hou>(reinterpret_cast<DataType*>(Data));
}

TMap<FName,FDataGather_ExportInfo::FStorageInfo> FDataGather_Base::PodStructsStorageInfo
{
	{NAME_Vector2D,{2,HAPI_STORAGETYPE_FLOAT64}},
	{NAME_Vector2d,{2,HAPI_STORAGETYPE_FLOAT64}},
	{NAME_Vector2f,{2,HAPI_STORAGETYPE_FLOAT}},

	{NAME_Vector,{3,HAPI_STORAGETYPE_FLOAT64,
	&TVectorCoordConvert::ReOrderAndScale<double,true>,
	&TVectorCoordConvert::ReOrderAndScale<double,false>}},
	{NAME_Vector3d,{3,HAPI_STORAGETYPE_FLOAT64,
	&TVectorCoordConvert::ReOrderAndScale<double,true>,
	&TVectorCoordConvert::ReOrderAndScale<double,false>}},
	{NAME_Vector3f,{3,HAPI_STORAGETYPE_FLOAT,
	&TVectorCoordConvert::ReOrderAndScale<float,true>,
	&TVectorCoordConvert::ReOrderAndScale<float,false>}},

	{NAME_Vector4,{4,HAPI_STORAGETYPE_FLOAT64}},
	{NAME_Vector4d,{4,HAPI_STORAGETYPE_FLOAT64}},
	{NAME_Vector4f,{4,HAPI_STORAGETYPE_FLOAT}},

	{NAME_Rotator,{3,HAPI_STORAGETYPE_FLOAT64,
	&Convert<true,FRotator>,
	&Convert<false,FRotator>}},
	{NAME_Rotator3d,{3,HAPI_STORAGETYPE_FLOAT64,
	&Convert<true,FRotator3d>,
	&Convert<false,FRotator3d>}},
	{NAME_Rotator3f,{3,HAPI_STORAGETYPE_FLOAT,
	&Convert<true,FRotator3f>,
	&Convert<false,FRotator3f>}},

	{NAME_Quat,{4,HAPI_STORAGETYPE_FLOAT64,
	&Convert<true,FQuat>,
	&Convert<false,FQuat>}},
	{NAME_Quat4d,{4,HAPI_STORAGETYPE_FLOAT64,
	&Convert<true,FQuat4d>,
	&Convert<false,FQuat4d>}},
	{NAME_Quat4f,{4,HAPI_STORAGETYPE_FLOAT,
	&Convert<true,FQuat4f>,
	&Convert<false,FQuat4f>}},
	
	{NAME_Color,{4,HAPI_STORAGETYPE_UINT8,
	&TColorCoordConvert::ReOrderAndScale<uint8,true>,
	&TColorCoordConvert::ReOrderAndScale<uint8,false>}},
	{NAME_LinearColor,{4,HAPI_STORAGETYPE_FLOAT}},
	
	/*Transform will convert to Matrix*/
	{NAME_Transform,{16,HAPI_STORAGETYPE_FLOAT64}},
	{NAME_Transform3d,{16,HAPI_STORAGETYPE_FLOAT64}},
	{NAME_Transform3f,{16,HAPI_STORAGETYPE_FLOAT}},

	{NAME_Matrix,{16,HAPI_STORAGETYPE_FLOAT64,
	&TMatrixCoordConvert::ReOrderAndScale<double,true>,
	&TMatrixCoordConvert::ReOrderAndScale<double,false>}},
	{NAME_Matrix44d,{16,HAPI_STORAGETYPE_FLOAT64,
	&TMatrixCoordConvert::ReOrderAndScale<double,true>,
	&TMatrixCoordConvert::ReOrderAndScale<double,false>}},
	{NAME_Matrix44f,{16,HAPI_STORAGETYPE_FLOAT,
	&TMatrixCoordConvert::ReOrderAndScale<float,true>,
	&TMatrixCoordConvert::ReOrderAndScale<float,false>}},
};

TMap<FName,FDataGather_ExportInfo::FStorageInfo> FDataGather_Base::PropertyStorageInfo = 
{
	{NAME_NameProperty,{1,HAPI_STORAGETYPE_STRING}},
	{NAME_TextProperty,{1,HAPI_STORAGETYPE_STRING}},
	{NAME_StrProperty,{1,HAPI_STORAGETYPE_STRING}},
	// POD Struct Property is already handled by PodStructsStorageInfo in ctor of FDataGather_ExportInfo
	// for other struct property that is not POD but export whole struct together by FDataGather_ExportInfo
	// can only be string, but actually this will not be hit in FDataGather_PODExport::Init
	{NAME_StructProperty,{1,HAPI_STORAGETYPE_STRING}},
	{NAME_IntProperty,{1,HAPI_STORAGETYPE_INT}},
	{NAME_BoolProperty,{1,HAPI_STORAGETYPE_INT8}},
	{NAME_EnumProperty,{1,HAPI_STORAGETYPE_INT}},
	{NAME_ByteProperty,{1,HAPI_STORAGETYPE_UINT8}},
	{NAME_DoubleProperty,{1,HAPI_STORAGETYPE_FLOAT64}},
	{NAME_FloatProperty,{1,HAPI_STORAGETYPE_FLOAT}},
	{NAME_Int64Property,{1,HAPI_STORAGETYPE_INT64}},
	{NAME_UInt32Property,{1,HAPI_STORAGETYPE_INT}},
	{NAME_UInt64Property,{1,HAPI_STORAGETYPE_INT64}}
	
	// ,{"ObjectPropertyBase",{1,HAPI_STORAGETYPE_INT64}}
	,{NAME_ObjectProperty,{1,HAPI_STORAGETYPE_INT64}}
	// ,{"ObjectPtrProperty",{1,HAPI_STORAGETYPE_INT64}}
	// ,{NAME_InterfaceProperty,{1,HAPI_STORAGETYPE_INT64}}
	,{"WeakObjectProperty",{1,HAPI_STORAGETYPE_INT64}}
	// ,{NAME_MulticastDelegateProperty,{1,HAPI_STORAGETYPE_INT64}}
	// ,{"MulticastSparseDelegateProperty",{1,HAPI_STORAGETYPE_INT64}}
	// ,{"MulticastInlineDelegateProperty",{1,HAPI_STORAGETYPE_INT64}}
	,{NAME_SoftObjectProperty,{1,HAPI_STORAGETYPE_INT64}}
	,{"SoftClassProperty",{1,HAPI_STORAGETYPE_STRING}}
	// ,{NAME_DelegateProperty,{1,HAPI_STORAGETYPE_INT64}}
	,{"FieldPathProperty",{1,HAPI_STORAGETYPE_STRING}}
	,{"ClassProperty",{1,HAPI_STORAGETYPE_INT64}}
};

#pragma endregion Template

FDataGather_ExportInfo::FDataGather_ExportInfo(const FProperty* Property)
:FDataGather_Base(Property)
{
	/*Todo HAPI_Storage Change Switch*/
	FHoudiniApi::AttributeInfo_Init(&Info);
	if(!FromStruct)
		return;
	const UScriptStruct* ConvertTo = FromStruct;
	if(ConvertSpecialization.ToStruct)
		ConvertTo = ConvertSpecialization.ToStruct;
	if(const auto* Re = PodStructsStorageInfo.Find(ConvertTo->GetFName()))
	{
		Info.storage = Re->StorageType;
		Info.tupleSize = Re->ElementTupleCount;
		CoordConvertUE2Hou = Re->CoordConvertUE2Hou;
		CoordConvertHou2Ue = Re->CoordConvertUE2Hou;
	}
	// Export String will Override POD Default Export Method because it create a StringExport
}

void FDataGather_PODExport::Init(const FDataGather_Struct& InParent)
{
	bInArrayOfStruct = InParent.bArrayOfStruct || InParent.bInArrayOfStruct;
	const FProperty* InputProperty = GetContainerElementRepresentProperty();
	FillHapiAttribInfo(Info,InputProperty,bInArrayOfStruct);
}

void FDataGather_ExportInfo::FillHapiAttribInfo(HAPI_AttributeInfo& AttributeInfo, const FProperty* InProperty, bool InbInArrayOfStruct)
{
	if(const FArrayProperty* InArrayProperty = CastField<const FArrayProperty>(InProperty))
	{
		// if(InbInArrayOfStruct)
		// {
		// 	AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
		// }
		FillHapiAttribInfo(AttributeInfo,InArrayProperty->Inner,InbInArrayOfStruct);
		AttributeInfo.storage = HapiStorageTraits::GetArrayStorage(AttributeInfo.storage);
		return;
	}
	const FName& FieldName = InProperty->GetClass()->GetFName();
	// Check AttributeInfo_Init set storage to HAPI_STORAGETYPE_INVALID
	// Otherwise the export storage is already check
	if(AttributeInfo.storage!=HAPI_STORAGETYPE_INVALID)
		return;
	if(const auto re = PropertyStorageInfo.Find(FieldName))
	{
		AttributeInfo.storage = re->StorageType;
		AttributeInfo.tupleSize = re->StorageType;
	}
	// Todo add some key word detect to scale property value by CoordConvertUE2Hou and CoordConvertHou2UE
}
