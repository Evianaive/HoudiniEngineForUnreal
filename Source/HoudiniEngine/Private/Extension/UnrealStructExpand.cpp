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

TMap<FName,FDataExchange_Info::FStorageInfo> FDataExchange_Base::PodStructsStorageInfo
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

TMap<FName,FDataExchange_Info::FStorageInfo> FDataExchange_Base::PropertyStorageInfo = 
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
	
	{NAME_Int8Property,{1,HAPI_STORAGETYPE_INT8}},
	{NAME_Int16Property,{1,HAPI_STORAGETYPE_INT16}},
	{NAME_IntProperty,{1,HAPI_STORAGETYPE_INT}},
	{NAME_Int64Property,{1,HAPI_STORAGETYPE_INT64}},
	
	{NAME_ByteProperty,{1,HAPI_STORAGETYPE_UINT8}},
	{NAME_UInt16Property,{1,HAPI_STORAGETYPE_INT16}},
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
void FDataExchange_Base::InitByParent(const FDataExchange_Struct& InParent)
{
	ParentStruct = const_cast<FDataExchange_Struct*>(&InParent);
	bInArrayOfStruct = InParent.bArrayOfStruct || InParent.bInArrayOfStruct;
	if constexpr (std::is_same_v<FDataExchange_Struct,Derived>)
	{
		/*Todo 并且为每个类都配一个init用于初始化 HAPI_AttributeInfo*/
		reinterpret_cast<FDataExchange_Struct*>(this)->MakeChildren();
	}
	else
	{
		const FProperty* InputProperty = GetInputProperty();
		Derived::FillHapiAttribInfo(reinterpret_cast<Derived*>(this)->Info,InputProperty,bInArrayOfStruct);	
	}
}

template<bool bUE2Hou>
void FDataExchange_Base::PerformStructConversion(FInstancedStruct& Struct, const uint8* InPtr)
{
	// auto* DestStruct = GetDestStruct<true>();
	// if(!DestStruct)
	// 	return;

	auto* ConversionMethod = ConvertSpecialization.GetConversionMethodRaw<bUE2Hou>();
	if(!ConversionMethod)
		return;
	ConversionMethod(InPtr,Struct.GetMutableMemory());
}

template<bool bUE2Hou>
void FDataExchange_Base::PerformCoordinateConversion(const uint8* InPtr)
{
	
}

FDataExchange_Info::FDataExchange_Info(const FProperty* Property)
:FDataExchange_Base(Property)
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
void FDataExchange_POD::PropToContainer(const uint8* Ptr)
{
	// Info.tupleSize;
	uint8* NewElem = ContainerHelper.GetRawPtr(ContainerHelper.AddValue());
	if(ConvertSpecialization.ToStruct)
	{
		const auto ConvertResult = ConvertSpecialization.PerformConvert<true>(Ptr);
		FMemory::Memcpy(NewElem,ConvertResult.GetMemory(),Property->ElementSize);
	}
	else
	{
		FMemory::Memcpy(NewElem,Ptr,Property->ElementSize);
	}
	if constexpr (bAddInfoCount)
		Info.count++;
}

void FDataExchange_POD::ArrayPropToContainer(const uint8* Ptr)
{
	if(!ArrayProperty)
		return;
	FScriptArrayHelper TempHelper(ArrayProperty,Ptr);
	const int32 Num = TempHelper.Num();
	const int32 AddStart = ContainerHelper.AddValues(Num);
	if(Num!=0)
	{
		if(ConvertSpecialization.ToStruct)
		{
			for(int32 i = 0; i<Num; i++)
			{
				uint8* NewElem = ContainerHelper.GetRawPtr(AddStart+i);
				Ptr = TempHelper.GetRawPtr(i);
				const auto ConvertResult = ConvertSpecialization.PerformConvert<true>(Ptr);
				FMemory::Memcpy(NewElem,ConvertResult.GetMemory(),Property->ElementSize);
			}
		}
		else
		{
			uint8* NewElem = ContainerHelper.GetRawPtr(AddStart);
			FMemory::Memcpy(NewElem,TempHelper.GetRawPtr(0),Num * Property->ElementSize);
		}
	}
	SizeFixedArray.Add(Num);
	Info.count+=1;
	Info.totalArrayElements+=Num;
}

template <bool bMinInfoCount>
void FDataExchange_POD::ContainerToProp(uint8* Ptr)
{
	const int32 LastElemNum = ContainerHelper.Num()-1;
	const uint8* NewElem = ContainerHelper.GetRawPtr(LastElemNum);
	if(ConvertSpecialization.ToStruct)
	{
		const auto ConvertResult = ConvertSpecialization.PerformConvert<false>(NewElem);
		FMemory::Memcpy(Ptr,ConvertResult.GetMemory(),Property->ElementSize);
	}
	else
	{
		FMemory::Memcpy(Ptr,NewElem,Property->ElementSize);
	}
	ContainerHelper.RemoveValues(LastElemNum);
	if constexpr (bMinInfoCount)
		Info.count--;
}

void FDataExchange_POD::ContainerToArrayProp(uint8* Ptr)
{

	/*Todo*/	
	// ContainerHelper.RemoveValues(LastElemNum);
}

void FDataExchange_POD::UnpackArray()
{
	ContainerHelper.UnPackElement(Info.tupleSize);
}

void FDataExchange_POD::PackArray()
{
	ContainerHelper.PackElement(Info.tupleSize);
}
#pragma region StringExport

namespace Private
{
	void ExportString(FDataExchange_String& StringExport, const uint8* Ptr, FString& Value)
	{
		if(StringExport.ConvertSpecialization.ToStruct)
		{
			const auto ConvertResult = StringExport.ConvertSpecialization.PerformConvert<true>(Ptr);
			ConvertResult.ExportTextItem(Value,ConvertResult,nullptr,PPF_None,nullptr);
		}
		else
		{
			StringExport.Property->ExportTextItem_Direct(Value,Ptr,Ptr,nullptr,PPF_None,nullptr); 
		}
	}
	void ImportString(FDataExchange_String& StringExport, uint8* Ptr, const FString& Value)
	{
		auto ValueChar = *Value;
		if(StringExport.ConvertSpecialization.ToStruct)
		{
			FInstancedStruct ConvertResult{StringExport.ConvertSpecialization.ToStruct};
			ConvertResult.ImportTextItem(ValueChar,PPF_None,nullptr,nullptr);
			StringExport.ConvertSpecialization.GetConversionMethodRaw<false>()(ConvertResult.GetMemory(),Ptr);
		}
		else
		{
			StringExport.Property->ImportText_Direct(ValueChar,Ptr,nullptr,PPF_None,nullptr); 
		}
	}
}

void FDataExchange_String::FillHapiAttribInfo(HAPI_AttributeInfo& AttributeInfo, const FProperty* InProperty, bool InbInArrayOfStruct)
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

template<bool bAddInfoCount>
void FDataExchange_String::PropToContainer(const uint8* Ptr)
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

void FDataExchange_String::ArrayPropToContainer(const uint8* Ptr)
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
	SizeFixedArray.Add(Num);
	Info.count+=1;
	Info.totalArrayElements+=Num;
}

template <bool bMinInfoCount>
void FDataExchange_String::ContainerToProp(uint8* Ptr)
{
	if(Property->IsA(FStrProperty::StaticClass()))
	{
		*(reinterpret_cast<FString*>(Ptr)) = Container.Pop();
	}
	else
	{
		Private::ImportString(*this,Ptr,Container.Pop());
	}
	if constexpr (bMinInfoCount)
		Info.count--;
}

void FDataExchange_String::ContainerToArrayProp(uint8* Ptr)
{
	int32 Num = SizeFixedArray.Pop();;
	if(Property->IsA(FStrProperty::StaticClass()))
	{
		TArray<FString>& DestArray = *(reinterpret_cast<TArray<FString>*>(Ptr));
		const int32 StartNum = DestArray.Num();
		for(int i = StartNum+Num-1;i>=StartNum;i--)
		{
			DestArray.Add(Container.Pop());
		}
	}
	else
	{
		FScriptArrayHelper TempHelper(ArrayProperty,Ptr);
		Num = Container.Num();

		const int32 AddStart = TempHelper.AddValues(Num);
		for(int32 i = AddStart+Num-1; i>=AddStart; i--)
		{
			Private::ImportString(*this,TempHelper.GetRawPtr(AddStart+i),Container.Pop());
		}
	}	
	Info.count-=1;
	Info.totalArrayElements-=Num;
}

FFoldDataVisitor::FFoldDataVisitor(uint8* StructPtr)
{
	Reset(StructPtr);
}

void FFoldDataVisitor::Reset()
{
	Struct = nullptr;
	bInArrayOfStruct = false;
	AddElementCount = Normal;
}

void FFoldDataVisitor::Reset(uint8* StructPtr)
{
	Ptr = StructPtr;
	Reset();
}

FUnfoldDataVisitor::FUnfoldDataVisitor(const uint8* StructPtr)
{
	Reset(StructPtr);
}
void FUnfoldDataVisitor::Reset()
{
	Struct = nullptr;
	bInArrayOfStruct = false;
	AddElementCount = Normal;
}
void FUnfoldDataVisitor::Reset(const uint8* StructPtr)
{
	Ptr = StructPtr;
	Reset();
}

#pragma endregion StringExport

void FDataExchange_POD::FillHapiAttribInfo(HAPI_AttributeInfo& AttributeInfo, const FProperty* InProperty, bool InbInArrayOfStruct)
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
bool FExportDataVisitor::operator()(T& Gather)
{
	// auto& Gather = reinterpret_cast<FDataGather_PODExport&>(Gather);
	const FString String = Gather.GetInputProperty()->GetAuthoredName();
	Gather.Info.owner = Owner;
	// Gather.Info.exists = true;
	// Gather.Info.originalOwner = Owner;
	
	Gather.UnpackArray();

	// We must add attribute before we set! 
	FHoudiniApi::AddAttribute(SessionId,NodeId,PartId,TCHAR_TO_ANSI(*String),&Gather.Info);
	HOUDINI_CHECK_RETURN(FHoudiniEngineUtilsExtenstion::HapiSetAttribData(
		Gather.GetContainer(),
		SessionId,NodeId,PartId
		,String,Gather.Info,
		{&Gather.SizeFixedArray,bTryEncode}),false);
	
	Gather.PackArray();
	return true;
}
template bool FExportDataVisitor::operator()(FDataExchange_POD& PODExport);
template bool FExportDataVisitor::operator()(FDataExchange_String& PODExport);

template <typename T>
bool FImportDataVisitor::operator()(T& Gather)
{
	// auto& PODExport = reinterpret_cast<FDataGather_PODExport&>(PODExport);
	const FString String = Gather.GetInputProperty()->GetAuthoredName();
	
	HAPI_AttributeInfo TempInfo;
	FHoudiniApi::AttributeInfo_Init(&TempInfo);
	FHoudiniApi::GetAttributeInfo(SessionId,NodeId,PartId,TCHAR_TO_ANSI(*String),Owner,&TempInfo);

	if(!TempInfo.exists)
		return false;
	//Todo Compare TempInfo with Gather.Info
	Gather.Info = TempInfo;
	UE_LOG(LogHoudiniEngine,Log,TEXT("Attrib [%s] Count:%i"),*String,Gather.Info);
	
	HOUDINI_CHECK_RETURN(FHoudiniEngineUtilsExtenstion::HapiGetAttribData(
		Gather.GetContainer(),
		SessionId,NodeId,PartId
		,String,Gather.Info,
		&Gather.SizeFixedArray),false);
	
	// For Array of Struct, get one of the children SizeFixedArray as its SizedFixedArray.
	// So we can know the size of struct array when we folding struct from their children
	if(ArrayOfStructExport!=nullptr && ArrayOfStructExport->ChildSizeFixedArray==nullptr)
	{
		ArrayOfStructExport->ChildSizeFixedArray = &Gather.SizeFixedArray;
	}
	Gather.PackArray();
	return true;
}
template bool FImportDataVisitor::operator()(FDataExchange_POD& PODExport);
template bool FImportDataVisitor::operator()(FDataExchange_String& PODExport);