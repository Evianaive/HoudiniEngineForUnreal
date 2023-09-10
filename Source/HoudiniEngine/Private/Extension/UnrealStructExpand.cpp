// Copyright Evianaive, Inc. All Rights Reserved.

#include "UnrealStructExpand.h"

#include "HoudiniApi.h"


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

template<typename ElementType, bool bUE2Hou>
static void ConvertInternal(UE::Math::TQuat<ElementType>* Quat)
{
	Swap(Quat->Y,Quat->Z);
	Quat->W *= -1;
}

template<typename ElementType, bool bUE2Hou>
static void ConvertInternal(UE::Math::TRotator<ElementType>* Rotator)
{
	if constexpr (!bUE2Hou)
	{
		TRotatorCoordConvert::ReOrder<ElementType,bUE2Hou>(Rotator);
		Rotator->Pitch *= -1;
		Rotator->Roll *= -1;
	}
	auto Quat = Rotator->Quaternion();
	ConvertInternal(&Quat);
	if constexpr (bUE2Hou)
	{
		*Rotator = Quat.Rotator();
		Rotator->Roll *= -1;
		Rotator->Pitch *= -1;
		TRotatorCoordConvert::ReOrder<ElementType,bUE2Hou>(Rotator);
	}
}

template<typename DataType, bool bUE2Hou>
static void Convert(uint8* Data)
{
	ConvertInternal(reinterpret_cast<DataType*>(Data));
}

TMap<FName,FDataGather_ExportInfo::FStorageInfo> FDataGather_Base::PodStructsStorageInfo{
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
	&Convert<FRotator,true>,
	&Convert<FRotator,false>}},
	{NAME_Rotator3d,{3,HAPI_STORAGETYPE_FLOAT64,
	&Convert<FRotator3d,true>,
	&Convert<FRotator3d,false>}},
	{NAME_Rotator3f,{3,HAPI_STORAGETYPE_FLOAT,
	&Convert<FRotator3f,true>,
	&Convert<FRotator3f,false>}},

	{NAME_Quat,{4,HAPI_STORAGETYPE_FLOAT64,
	&Convert<FQuat,true>,
	&Convert<FQuat,false>}},
	{NAME_Quat4d,{4,HAPI_STORAGETYPE_FLOAT64,
	&Convert<FQuat4d,true>,
	&Convert<FQuat4d,false>}},
	{NAME_Quat4f,{4,HAPI_STORAGETYPE_FLOAT,
	&Convert<FQuat4f,true>,
	&Convert<FQuat4f,false>}},
	
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
		Info.tupleSize = Re->ElementTupleCount;
		CoordConvertUE2Hou = Re->CoordConvertUE2Hou;
		CoordConvertHou2Ue = Re->CoordConvertUE2Hou;
	}
}



void FDataGather_ExportInfo::Init(const FDataGather_Struct& InParent)
{
	bInArrayOfStruct = InParent.bArrayOfStruct || InParent.bInArrayOfStruct;
	const FProperty* InputProperty = GetContainerElementRepresentProperty();
	FillHapiAttribInfo(Info,InputProperty);
}

void FDataGather_ExportInfo::FillHapiAttribInfo(HAPI_AttributeInfo& AttributeInfo, const FProperty* InProperty)
{
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	if(const FArrayProperty* InArrayProperty = CastField<const FArrayProperty>(InProperty))
	{
		FillHapiAttribInfo(AttributeInfo,InArrayProperty->Inner);
		
	}
	if(const FNumericProperty* InNumProp = CastField<const FNumericProperty>(InProperty))
	{
		InNumProp->GetClass();
	}
	
}
