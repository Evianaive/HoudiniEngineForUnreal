// Copyright Evianaive, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CustomScriptArrayHelper.h"
#include "HapiStorageTypeTraits.h"
#include "InstancedStruct.h"
#include "HAPI/HAPI_Common.h"
#include "Misc/TVariant.h"
//#include "UnrealStructExpand.generated.h"


// struct FInstancedStruct;
struct FStructConvertSpecialization
{
	// FStructConvertSpecialization()
	// {
	// 	ConvertTo = ConvertBack = [](const uint8*){return FInstancedStruct();};
	// };
	using Func = FInstancedStruct(const uint8*);
	UScriptStruct* ToStruct {nullptr};
	Func* ConvertTo {nullptr};
	Func* ConvertBack {nullptr};
	/*use to specify that some struct must export as string (for example curve ramp)*/
	bool bExportString {false};

	template<bool bUE2Hou>
	Func* GetConversionMethod() const
	{
		if constexpr (bUE2Hou)
			return ConvertTo;
		else
			return ConvertBack;
	}
	
	static void RegisterConvertFunc(
		UScriptStruct* InFromStruct,
		UScriptStruct* InToStruct = nullptr,
		Func* InConvertTo = nullptr,
		Func* InConvertBack = nullptr,
		bool InbExportString = false)
	{
		auto& Specialization = RegisteredSpecializations.FindOrAdd(InFromStruct);
		Specialization.ToStruct = InToStruct;
		Specialization.ConvertTo = InConvertTo;
		Specialization.ConvertBack = InConvertBack;
		Specialization.bExportString = InbExportString;
	}
	// template<typename TFromStruct,typename TToStruct>
	// static void RegisterConvertFunc(bool InbExportString = false)
	// {
	// 	RegisterConvertFunc(
	// 	TBaseStructure<TFromStruct>::Get(),
	// 	TBaseStructure<TToStruct>::Get(),
	// 	[](const uint8* InData)
	// 	{
	// 		FInstancedStruct ToStructInstance;
	// 		ToStructInstance.InitializeAs<TToStruct>(*(reinterpret_cast<TFromStruct*>(InData)));
	// 		return ToStructInstance;
	// 	},
	// 	[](const uint8* InData){
	// 		FInstancedStruct BackStructInstance;
	// 		if constexpr (TIsConstructible<TFromStruct,TToStruct>::Value)
	// 		{
	// 			BackStructInstance.InitializeAs<TFromStruct>(*(reinterpret_cast<TToStruct*>(InData)));
	// 		}
	// 		else
	// 		{
	// 			BackStructInstance.InitializeAs<TFromStruct>((reinterpret_cast<TToStruct*>(InData))->ConvertBack());
	// 		}
	// 		return BackStructInstance;
	// 	},
	// 	InbExportString);
	// }
	static TMap<UScriptStruct*,FStructConvertSpecialization> RegisteredSpecializations;
};

struct FDataGather_Struct;
struct FDataGather_PODExport;
struct FDataGather_StringExport;

using FDataGather_Variant = TVariant<
	FDataGather_Struct,
	FDataGather_PODExport,
	FDataGather_StringExport>;

struct FDataGather_Base
{
	FDataGather_Base(const FProperty* InProperty)
		: OffsetInParentStruct(InProperty->GetOffset_ForInternal())
	{
		if(const FArrayProperty* InArrayProperty = CastField<const FArrayProperty>(InProperty))
		{
			ArrayProperty = InArrayProperty;
			Property = InArrayProperty->Inner;
		}
		else
		{
			Property = InProperty;
		}
		if(const FStructProperty* StructProperty = CastField<const FStructProperty>(Property))
		{
			FromStruct = StructProperty->Struct;
			if(const auto Re = FStructConvertSpecialization::RegisteredSpecializations.Find(FromStruct))
			{
				ConvertSpecialization = *Re; 
			}
		}
		if(const UEnum* Enum = GetEnum(Property))
		{
			FromEnum = Enum;
		}
		if(const FObjectProperty* ObjectProperty = CastField<const FObjectProperty>(Property))
		{
			FromClass = ObjectProperty->PropertyClass;
		}
	}

	static UEnum* GetEnum(const FProperty* InProperty)
	{
		if(const FEnumProperty* EnumProperty = CastField<const FEnumProperty>(InProperty))
		{
			return EnumProperty->GetEnum();
		}
		if(const FNumericProperty* NumericProperty = CastField<FNumericProperty>(InProperty))
		{
			return NumericProperty->GetIntPropertyEnum();
		}
		return nullptr;
	}

	FDataGather_Base(const UScriptStruct* InBaseStruct)
		:FromStruct(InBaseStruct)
	{
		if(const auto Re = FStructConvertSpecialization::RegisteredSpecializations.Find(FromStruct))
		{
			ConvertSpecialization = *Re; 
		}
	}
protected:
	// Todo Change all init to init by parent! Call this in derived class
	void Init(const FDataGather_Struct& InParent)
	{
		ParentStruct = const_cast<FDataGather_Struct*>(&InParent);
	}
	FDataGather_Struct* ParentStruct {nullptr};
public:
	bool bInArrayOfStruct {false};
	int32 OffsetInParentStruct {0};
	
	const FProperty* Property {nullptr};
	const FArrayProperty* ArrayProperty {nullptr};
	
	const FProperty* GetInputProperty() const
	{
		return ArrayProperty?ArrayProperty:Property;
	}
	const FProperty* GetContainerElementRepresentProperty() const
	{
		return bInArrayOfStruct? GetInputProperty() : Property;
	}
	template<bool bUE2Hou>
	const UScriptStruct* GetDestStruct() const
	{
		if constexpr (bUE2Hou)
		{
			return ConvertSpecialization.ToStruct;
		}
		else
		{
			return FromStruct;
		}
	};
	// union
	// {
		const UScriptStruct* FromStruct {nullptr};
		const UEnum* FromEnum {nullptr};
		const UClass* FromClass {nullptr};
	// };
	FStructConvertSpecialization ConvertSpecialization {};
	
	using CoordConvertFuncType = void(uint8*);
	struct FStorageInfo
	{
		int32 ElementTupleCount;
		HAPI_StorageType StorageType;	
		CoordConvertFuncType* CoordConvertUE2Hou {nullptr};
		CoordConvertFuncType* CoordConvertHou2Ue {nullptr};	
	};
	/*Attrib info stored in POD Container(FScriptArray)*/
	static TMap<FName,FStorageInfo> PodStructsStorageInfo;
	static TMap<FName,FStorageInfo> PropertyStorageInfo;

	template<bool bUE2Hou>
	void PerformStructConversion(FInstancedStruct& Struct, const uint8* InPtr);
	template<bool bUE2Hou>
	void PerformCoordinateConversion(const uint8* InPtr);

	// static int32 HAPI_StorageTypeSizes[HAPI_STORAGETYPE_MAX];
};

struct FDataGather_ExportInfo : public FDataGather_Base
{
	explicit FDataGather_ExportInfo(const FProperty* Property);
	static void FillHapiAttribInfo(HAPI_AttributeInfo& AttributeInfo,const FProperty* InProperty,bool InbInArrayOfStruct);
	void Init(const FDataGather_Struct& InParent);
	HAPI_AttributeInfo Info {};
	TArray<int32> SizeFixedArray;
	CoordConvertFuncType* CoordConvertUE2Hou {nullptr};
	CoordConvertFuncType* CoordConvertHou2Ue {nullptr};
};

struct FDataGather_PODExport : public FDataGather_ExportInfo
{
	FDataGather_PODExport(const FProperty* InProperty)
		:FDataGather_ExportInfo(InProperty) // Here we have already get the tuple size and hapi_storage
		,ContainerHelper(Property,ConvertSpecialization.ToStruct)
	{
		// add function to process property we pass in here to convert to hapi_storage type and tuple size?
	}
	// FScriptArray Container;
	FCustomScriptArrayHelper ContainerHelper;
	void Init(const FDataGather_Struct& InParent);
	template<bool bAddInfoCount = true>
	void PropToContainer(const uint8* Ptr);
	void ArrayPropToContainer(const uint8* Ptr);
	void* GetContainer(){return &const_cast<FScriptArray&>(ContainerHelper.GetArray());};
	void* GetContainerRawPtr(){return ContainerHelper.GetRawPtr();};
	
	void UnpackArray();
	void PackArray();
};

struct FDataGather_StringExport : public FDataGather_ExportInfo
{
	FDataGather_StringExport(const FProperty* Property)
		:FDataGather_ExportInfo(Property)
	{
		Info.storage = HAPI_STORAGETYPE_STRING;
		Info.tupleSize = 1;
	}
	TArray<FString> Container;
	// void Init(const FDataGather_Struct& InParent);
	/*Todo Struct Export by Actual Struct*/
	template<bool bAddInfoCount = true>
	void PropToContainer(const uint8* Ptr);
	void ArrayPropToContainer(const uint8* Ptr);
	void* GetContainer(){return &Container;}
	void* GetContainerRawPtr(){return Container.GetData();};
	
	void UnpackArray() const{};
	void PackArray() const{};
};

struct FDataGather_Struct : public FDataGather_Base
{
	FDataGather_Struct(const UScriptStruct* BaseStruct)
		:FDataGather_Base(BaseStruct)
	{
		//Todo Rename
		Init2();
	}
	FDataGather_Struct(const FProperty* Property)
		:FDataGather_Base(Property)
	{
		bArrayOfStruct = (ArrayProperty != nullptr);
	}
	TArray<FDataGather_Variant> Children;
	bool bArrayOfStruct {false};

	// void GatherDataFromParent(const FDataGather_Struct& InParent)
	// {
	// 	bInArrayOfStruct = InParent.bArrayOfStruct || InParent.bInArrayOfStruct;
	// }
	void Init2()
	{
		const UScriptStruct* InitStruct = ConvertSpecialization.ToStruct? ConvertSpecialization.ToStruct : FromStruct;
		for(const auto* Prop : TFieldRange<FProperty>(InitStruct))
		{
			MakeChild(Prop);
		}
	}
	/*Todo 将parent struct的参数放到init里，并且为每个类都配一个init用于初始化 HAPI_AttributeInfo*/
	void Init(const FDataGather_Struct& InParent)
	{
		bInArrayOfStruct = InParent.bArrayOfStruct || InParent.bInArrayOfStruct;
		const UScriptStruct* InitStruct = ConvertSpecialization.ToStruct? ConvertSpecialization.ToStruct : FromStruct;
		
		for(const auto* Prop : TFieldRange<FProperty>(InitStruct))
		{
			MakeChild(Prop);
		}
	}	

	void MakeChild(const FProperty* InProp, const FArrayProperty* InArrayProp = nullptr)
	{
		const FProperty* FirstEnterProp = InArrayProp ? InArrayProp: InProp;
		
		if(const FArrayProperty* ArrayProp = CastField<const FArrayProperty>(InProp))
		{
			/*First Enter*/
			if(bArrayOfStruct || bInArrayOfStruct)
			{
				/*An Array Property in ArrayOfStruct*/
				Add<FDataGather_StringExport>(InProp);
			}
			else
			{
				/*An Array Property*/
				MakeChild(ArrayProp->Inner,ArrayProp);
			}
		}
		else if(const FStructProperty* StructProperty = CastField<const FStructProperty>(InProp))
		{
			const UScriptStruct* FinalStruct = StructProperty->Struct;
			bool bExportString = false;
			/*We Specified Convert the struct property to another struct*/
			if(const auto* Re = FStructConvertSpecialization::RegisteredSpecializations.Find(StructProperty->Struct))
			{
				FinalStruct = Re->ToStruct;
				bExportString = Re->bExportString;
			}
			if(bExportString)
			{
				Add<FDataGather_StringExport>(FirstEnterProp);
			}
			else if(PodStructsStorageInfo.Find(FinalStruct->GetFName()))
			{
				Add<FDataGather_PODExport>(FirstEnterProp);
				//Todo this property doesn't match the struct if struct conversion is Specialization!
			}
			else
			{
				Add<FDataGather_Struct>(FirstEnterProp);
			}
		}
		else if (InProp->IsA(FNumericProperty::StaticClass()))
		{
			Add<FDataGather_PODExport>(FirstEnterProp);
		}
		else
		{
			Add<FDataGather_StringExport>(FirstEnterProp);
		}
	}

	template<typename TVariant,typename... TArgs>
	TVariant& Add(TArgs&&... Args)
	{
		TVariant& NewChild = (new (Children) FDataGather_Variant(TInPlaceType<TVariant>(),Forward<TArgs>(Args)...))->Get<TVariant>();
		NewChild.Init(*this);
		return NewChild;
	}
	
	template<typename TVariant,typename... TArgs>
	TVariant* Set(int32 Index, TArgs&&... Args)
	{
		if(!Children.IsValidIndex(Index))
			return nullptr;
		Children[Index].Set<TVariant>(Forward<TArgs>(Args)...);
		TVariant& NewChild = Children[Index].Get<TVariant>();
		NewChild.Init(*this);
		return &NewChild;
	}
	template<typename TVisitor>
	void Accept(TVisitor&& Visitor)
	{
		for(auto& Child : Children)
		{
			::Visit(Visitor,Child);
		}
	}

	void GetConvertSpecializationTo(const uint8* InData, FInstancedStruct& OutConversion)
	{
		if(ConvertSpecialization.ToStruct)
		{
			OutConversion = ConvertSpecialization.ConvertTo(InData);
		}
	}
};

struct FSwitchEnumContainerVisitor
{
	FSwitchEnumContainerVisitor(bool InbSwitchToString)
	:bSwitchToString(InbSwitchToString)
	{}
	bool bSwitchToString;	
private:
	int32 Index {0};
	TArray<int32> ExportIndicesInStructWithEnum;
	FDataGather_Struct* CurrentVisitStructExport {nullptr};
	TArray<FDataGather_Struct*> WaitToProcess;

	void SwitchCurrentStructGatherEnumChild()
	{
		for(const int32 ChildId : ExportIndicesInStructWithEnum)
		{
			const FProperty* InputProperty;
			::Visit([&](const auto& Variant)
			{
				InputProperty = Variant.GetInputProperty();
			},CurrentVisitStructExport->Children[ChildId]);
			
			if(bSwitchToString)
			{
				CurrentVisitStructExport->Set<FDataGather_StringExport>(ChildId,InputProperty);
			}
			else
			{
				CurrentVisitStructExport->Set<FDataGather_PODExport>(ChildId,InputProperty);
			}
		}
		ExportIndicesInStructWithEnum.Reset();
	}
public:
	void operator()(FDataGather_Struct& Struct)
	{
		if(CurrentVisitStructExport)
		{
			WaitToProcess.Add(&Struct);
			Index += 1;
		}
		else
		{
			CurrentVisitStructExport = &Struct;
			Index = 0;
			Struct.Accept(*this);
			SwitchCurrentStructGatherEnumChild();
			CurrentVisitStructExport = nullptr;
			while (WaitToProcess.Num())
			{
				operator()(*WaitToProcess.Pop());
			}
		}
	}
	template<typename T>
	void operator()(T& Exporter)
	{
		constexpr bool bStringContainer = std::is_same_v<T,FDataGather_StringExport>;
		if(Exporter.FromEnum)
        {
			if(bSwitchToString != bStringContainer)
			{
				ExportIndicesInStructWithEnum.Add(Index);
			}
        }
		Index += 1;
	}
};

struct FGatherDataVisitor
{
	FGatherDataVisitor(const uint8* StructPtr);
	const uint8* Ptr;
	UScriptStruct* Struct;
	bool bInArrayOfStruct;
	int32 AddElementCount;
public:
	enum
	{
		InArrayOfStructSignal = -1,
		Normal = -2
	};
	void Reset();
	void Reset(const uint8* StructPtr);
	
	void ConvertPtr(FDataGather_Struct& StructExport, FInstancedStruct& OutInstanceStruct)
	{
		if(StructExport.ConvertSpecialization.ToStruct)
		{
			StructExport.GetConvertSpecializationTo(Ptr,OutInstanceStruct);
			Ptr = OutInstanceStruct.GetMutableMemory();
		}
	}
	void operator()(FDataGather_Struct& StructExport)
	{
		// ScopeExitSupport::TScopeGuard<>();
		// ON_SCOPE_EXIT{return;}
		TGuardValue GuardPtr(Ptr,Ptr+StructExport.OffsetInParentStruct);
		TGuardValue GuardArray(bInArrayOfStruct,(bInArrayOfStruct||StructExport.bArrayOfStruct));
		FInstancedStruct ConvertResult;
		if(StructExport.ArrayProperty)
		{
			FScriptArrayHelper Helper(StructExport.ArrayProperty,Ptr);
			for(int32 i = 0;i<Helper.Num();i++)
			{
				Ptr = Helper.GetRawPtr(i);
				ConvertPtr(StructExport,ConvertResult);
				StructExport.Accept(*this);				
			}
			AddElementCount = InArrayOfStructSignal;
			TGuardValue GuardAddCount(AddElementCount,Helper.Num());
			StructExport.Accept(*this);
		}
		else
		{
			AddElementCount = Normal;
			ConvertPtr(StructExport,ConvertResult);
			StructExport.Accept(*this);
		}
	}
	template<typename Export>
	void operator()(Export& PODExport)
	{
		FDataGather_ExportInfo& AsExport = PODExport;
		TGuardValue GuardPtr(Ptr,Ptr+AsExport.OffsetInParentStruct);
		
		if(AddElementCount>InArrayOfStructSignal)
		{
			AsExport.SizeFixedArray.Add(AddElementCount);
			AsExport.Info.totalArrayElements += AddElementCount;
			AsExport.Info.count += 1;
		}
		else
		{
			if(
			// HapiStorageTraits::IsArrayStorage(AsExport.Info.storage) 
			AsExport.ArrayProperty 
			&& !bInArrayOfStruct)
			{
				PODExport.ArrayPropToContainer(Ptr);
			}
			else
			{
				AddElementCount==Normal?PODExport.PropToContainer(Ptr):PODExport.PropToContainer<false>(Ptr);
			}
		}
	}
	// void operator()(FDataGather_StringExport& StringExport)
	// {
	// 	const uint8* PropertyPtr = Ptr+StringExport.OffsetInParentStruct;
	// 	if(bInArrayOfStruct)
	// 	{
	// 		StringExport.GetInputProperty()
	// 		->ExportTextItem_Direct(StringExport.Container.Add_GetRef({}),PropertyPtr,PropertyPtr,nullptr,0);
	// 	}
	// 	else
	// 	{
	// 		if(StringExport.ArrayProperty)
	// 		{
	// 			FScriptArrayHelper Helper(StringExport.ArrayProperty,PropertyPtr);
	// 			for(int32 i = 0;i<Helper.Num();i++)
	// 			{
	// 				Helper.GetRawPtr(i);
	// 				StringExport.Container.Add_GetRef({});
	// 			}
	// 		}		
	// 	}
	// }
};

struct FExportDataVisitor
{
	const HAPI_Session* SessionId {nullptr};
	HAPI_NodeId NodeId {0};
	HAPI_AttributeOwner Owner {HAPI_AttributeOwner::HAPI_ATTROWNER_INVALID};
	HAPI_PartId PartId {0};
	bool bTryEncode {false};

	void operator()(FDataGather_Struct& StructExport)
	{
		StructExport.Accept(*this);
	}
	template<typename Export>
	void operator()(Export& Gather);
};

struct FImportDataVisitor
{
	const HAPI_Session* SessionId {nullptr};
	HAPI_NodeId NodeId {0};
	HAPI_AttributeOwner Owner {HAPI_AttributeOwner::HAPI_ATTROWNER_INVALID};
	HAPI_PartId PartId {0};
	
	void operator()(FDataGather_Struct& StructExport)
	{
		StructExport.Accept(*this);
	}
	template<typename Export>
	void operator()(Export& Gather);
};