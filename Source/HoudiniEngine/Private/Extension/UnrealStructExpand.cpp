// Copyright Evianaive, Inc. All Rights Reserved.

#include "UnrealStructExpand.h"

#include "HapiStorageTypeTraits.h"
#include "HoudiniApi.h"
#include "HoudiniEngineUtilsExtenstion.h"


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
	//Todo use this map directly map to corresponding container. 
	{NAME_NameProperty,{1,HAPI_STORAGETYPE_STRING}},
	{NAME_TextProperty,{1,HAPI_STORAGETYPE_STRING}},
	{NAME_StrProperty,{1,HAPI_STORAGETYPE_STRING}},
	// POD Struct Property is already handled by PodStructsStorageInfo in ctor of FDataGather_ExportInfo
	// for other struct property that is not POD but export whole struct together by FDataGather_ExportInfo
	// can only be string, but actually this will not be hit in FDataGather_PODExport::Init
	{NAME_StructProperty,{1,HAPI_STORAGETYPE_STRING}},
	{NAME_BoolProperty,{1,HAPI_STORAGETYPE_INT8}},
	{NAME_EnumProperty,{1,HAPI_STORAGETYPE_INT}},
	
	{NAME_DoubleProperty,{1,HAPI_STORAGETYPE_FLOAT64}},
	{NAME_FloatProperty,{1,HAPI_STORAGETYPE_FLOAT}},
	//Todo Int16 is not supported!!
	{NAME_Int8Property,{1,HAPI_STORAGETYPE_INT8}},
	{NAME_IntProperty,{1,HAPI_STORAGETYPE_INT}},
	{NAME_Int64Property,{1,HAPI_STORAGETYPE_INT64}},
	//Todo UInt16 is not supported!!
	{NAME_ByteProperty,{1,HAPI_STORAGETYPE_UINT8}},
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

template <typename Derived>
void FDataGather_Base::InitByParent(const FDataGather_Struct& InParent)
{
	ParentStruct = const_cast<FDataGather_Struct*>(&InParent);
	bInArrayOfStruct = InParent.bArrayOfStruct || InParent.bInArrayOfStruct;
	if constexpr (std::is_same_v<FDataGather_Struct,Derived>)
	{
		/*Todo 并且为每个类都配一个init用于初始化 HAPI_AttributeInfo*/
		reinterpret_cast<FDataGather_Struct*>(this)->MakeChildren();
	}
	else
	{
		const FProperty* InputProperty = GetInputProperty();
		Derived::FillHapiAttribInfo(reinterpret_cast<Derived*>(this)->Info,InputProperty,bInArrayOfStruct);	
	}
}

template<bool bUE2Hou>
void FDataGather_Base::PerformStructConversion(FInstancedStruct& Struct, const uint8* InPtr)
{
	// auto* DestStruct = GetDestStruct<true>();
	// if(!DestStruct)
	// 	return;

	auto* ConversionMethod = ConvertSpecialization.GetConversionMethod<bUE2Hou>();
	if(!ConversionMethod)
		return;
	Struct = ConversionMethod(InPtr);
}

template<bool bUE2Hou>
void FDataGather_Base::PerformCoordinateConversion(const uint8* InPtr)
{
	
}

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

template<bool bAddInfoCount>
void FDataGather_PODExport::PropToContainer(const uint8* Ptr)
{
	// Info.tupleSize;
	uint8* NewElem = ContainerHelper.GetRawPtr(ContainerHelper.AddValue());
	if(ConvertSpecialization.ToStruct)
	{
		const auto ConvertResult = ConvertSpecialization.ConvertTo(Ptr);
		FMemory::Memcpy(NewElem,ConvertResult.GetMemory(),Property->ElementSize);
	}
	else
	{
		FMemory::Memcpy(NewElem,Ptr,Property->ElementSize);
	}
	if constexpr (bAddInfoCount)
		Info.count++;
}

void FDataGather_PODExport::ArrayPropToContainer(const uint8* Ptr)
{
	if(!ArrayProperty)
		return;
	FScriptArrayHelper TempHelper(ArrayProperty,Ptr);
	const int32 Num = TempHelper.Num();
	const int32 AddStart = ContainerHelper.AddValues(Num);
	
	if(ConvertSpecialization.ToStruct)
	{
		for(int32 i = 0; i<Num; i++)
		{
			uint8* NewElem = ContainerHelper.GetRawPtr(AddStart+i);
			Ptr = TempHelper.GetRawPtr(i);
			const auto ConvertResult = ConvertSpecialization.ConvertTo(Ptr);
			FMemory::Memcpy(NewElem,ConvertResult.GetMemory(),Property->ElementSize);
		}
	}
	else
	{
		uint8* NewElem = ContainerHelper.GetRawPtr(AddStart);
		FMemory::Memcpy(NewElem,TempHelper.GetRawPtr(0),Property->ElementSize);
	}
	Info.count+=1;
	Info.totalArrayElements+=Num;
}

void FDataGather_PODExport::UnpackArray()
{
	ContainerHelper.UnPackElement(Info.tupleSize);
}

void FDataGather_PODExport::PackArray()
{
	ContainerHelper.PackElement(ContainerHelper.GetStorePackSize());
}
#pragma region StringExport

namespace Private
{
	void ExportString(FDataGather_StringExport& StringExport, const uint8* Ptr, FString& Value)
	{
		if(StringExport.ConvertSpecialization.ToStruct)
		{
			const auto ConvertResult = StringExport.ConvertSpecialization.ConvertTo(Ptr);
			ConvertResult.ExportTextItem(Value,ConvertResult,nullptr,PPF_None,nullptr);
		}
		else
		{
			StringExport.Property->ExportTextItem_Direct(Value,Ptr,Ptr,nullptr,PPF_None,nullptr); 
		}
	}
}
template<bool bAddInfoCount>
void FDataGather_StringExport::PropToContainer(const uint8* Ptr)
{
	if(Property->IsA(FStrProperty::StaticClass()))
	{
		Container.Add(*(reinterpret_cast<const FString*>(Ptr)));
	}
	else
	{
		Private::ExportString(*this,Ptr,Container.AddDefaulted_GetRef());
	}
	if constexpr (bAddInfoCount)
		Info.count++;
}

void FDataGather_StringExport::FillHapiAttribInfo(HAPI_AttributeInfo& AttributeInfo, const FProperty* InProperty, bool InbInArrayOfStruct)
{
	auto* InArrayProperty = CastField<FArrayProperty>(InProperty);
	// Export an array property to string is not supported if the array property is not in array of struct.
	// So it must be exported as string array
	if(InbInArrayOfStruct || InArrayProperty)
	{
		AttributeInfo.storage = HAPI_STORAGETYPE_STRING_ARRAY;	
	}
	else
	{
		AttributeInfo.storage = HAPI_STORAGETYPE_STRING;	
	}
	AttributeInfo.tupleSize = 1;
}

void FDataGather_StringExport::ArrayPropToContainer(const uint8* Ptr)
{
	int32 Num = 0;
	if(Property->IsA(FStrProperty::StaticClass()))
	{
		Container.Append(*(reinterpret_cast<const TArray<FString>*>(Ptr)));
		Num = reinterpret_cast<const TArray<FString>*>(Ptr)->Num();
	}
	else
	{
		FScriptArrayHelper TempHelper(ArrayProperty,Ptr);
		Num = TempHelper.Num();

		const int32 AddStart = Container.AddZeroed(Num);
		for(int32 i=0; i<Num; i++)
		{
			Private::ExportString(*this,TempHelper.GetRawPtr(i),Container[AddStart+i]);
		}		
	}
	Info.count+=1;
	Info.totalArrayElements+=Num;
}

FGatherDataVisitor::FGatherDataVisitor(const uint8* StructPtr)
{
	Reset(StructPtr);
}
void FGatherDataVisitor::Reset()
{
	Struct = nullptr;
	bInArrayOfStruct = false;
	AddElementCount = Normal;
}
void FGatherDataVisitor::Reset(const uint8* StructPtr)
{
	Ptr = StructPtr;
	Reset();
}

#pragma endregion StringExport

void FDataGather_PODExport::FillHapiAttribInfo(HAPI_AttributeInfo& AttributeInfo, const FProperty* InProperty, bool InbInArrayOfStruct)
{
	const FArrayProperty* InArrayProperty = CastField<const FArrayProperty>(InProperty);
	if(InArrayProperty || InbInArrayOfStruct)
	{
		if(InArrayProperty && InbInArrayOfStruct)
		{
			// This condition should never be fitted
			AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
		}
		else if(InArrayProperty)
		{
			FillHapiAttribInfo(AttributeInfo,InArrayProperty->Inner,InbInArrayOfStruct);		
		}
		else
		{
			FillHapiAttribInfo(AttributeInfo,InProperty,false);
		}
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
		AttributeInfo.tupleSize = re->ElementTupleCount;
	}
	// Todo add some key word detect to scale property value by CoordConvertUE2Hou and CoordConvertHou2UE
}

template <typename T>
void FExportDataVisitor::operator()(T& Gather)
{
	// auto& Gather = reinterpret_cast<FDataGather_PODExport&>(Gather);
	const FString String = Gather.GetInputProperty()->GetAuthoredName();
	Gather.Info.owner = Owner;
	// Gather.Info.exists = true;
	// Gather.Info.originalOwner = Owner;
	
	Gather.UnpackArray();

	// We must add attribute before we set! 
	FHoudiniApi::AddAttribute(SessionId,NodeId,PartId,TCHAR_TO_ANSI(*String),&Gather.Info);
	FHoudiniEngineUtilsExtenstion::HapiSetAttribData(
		Gather.GetContainer(),
		SessionId,NodeId,PartId
		,String,Gather.Info,
		{&Gather.SizeFixedArray,bTryEncode});
}
template void FExportDataVisitor::operator()(FDataGather_PODExport& PODExport);
template void FExportDataVisitor::operator()(FDataGather_StringExport& PODExport);

template <typename T>
void FImportDataVisitor::operator()(T& Gather)
{
	// auto& PODExport = reinterpret_cast<FDataGather_PODExport&>(PODExport);
	const FString String = Gather.GetInputProperty()->GetAuthoredName();
	
	HAPI_AttributeInfo TempInfo;
	FHoudiniApi::AttributeInfo_Init(&TempInfo);
	FHoudiniApi::GetAttributeInfo(SessionId,NodeId,PartId,TCHAR_TO_ANSI(*String),Owner,&TempInfo);

	//Todo Compare TempInfo with Gather.Info
	
	FHoudiniEngineUtilsExtenstion::HapiGetAttribData(
		Gather.GetContainer(),
		SessionId,NodeId,PartId
		,String,Gather.Info,
		&Gather.SizeFixedArray);
	
	Gather.PackArray();
}
template void FExportDataVisitor::operator()(FDataGather_PODExport& PODExport);
template void FExportDataVisitor::operator()(FDataGather_StringExport& PODExport);