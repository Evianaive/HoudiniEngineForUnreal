// Copyright Evianaive, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InstancedStruct.h"
#include "HAPI/HAPI_Common.h"
#include "Misc/TVariant.h"
#include "UnrealStructExpand.generated.h"


struct FInstancedStruct;
struct FStructConvertSpecialization
{
	FStructConvertSpecialization();
	using Func = FInstancedStruct(const uint8*);
	UScriptStruct* ToStruct {nullptr};
	Func* ConvertTo {nullptr};
	Func* ConvertBack {nullptr};
	bool bExportString {false};
	
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
	template<typename TFromStruct,typename TToStruct>
	static void RegisterConvertFunc(bool InbExportString = false)
	{
		RegisterConvertFunc(
		TBaseStructure<TFromStruct>::Get(),
		TBaseStructure<TToStruct>::Get(),
		[](const uint8* InData)
		{
			FInstancedStruct ToStructInstance;
			ToStructInstance.InitializeAs<TToStruct>(*(reinterpret_cast<TFromStruct*>(InData)));
			return ToStructInstance;
		},
		[](const uint8* InData){
			FInstancedStruct BackStructInstance;
			if constexpr (TIsConstructible<TFromStruct,TToStruct>::Value)
			{
				BackStructInstance.InitializeAs<TFromStruct>(*(reinterpret_cast<TToStruct*>(InData)));
			}
			else
			{
				BackStructInstance.InitializeAs<TFromStruct>((reinterpret_cast<TToStruct*>(InData))->ConvertBack());
			}
			return BackStructInstance;
		},
		InbExportString);
	}
	static inline TMap<UScriptStruct*,FStructConvertSpecialization> RegisteredSpecializations;
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

	// static int32 HAPI_StorageTypeSizes[HAPI_STORAGETYPE_MAX];
};

struct FDataGather_ExportInfo : public FDataGather_Base
{
	explicit FDataGather_ExportInfo(const FProperty* Property);
	void Init(const FDataGather_Struct& InParent);
	static void FillHapiAttribInfo(HAPI_AttributeInfo& AttributeInfo,const FProperty* InProperty);
	HAPI_AttributeInfo Info {};
	CoordConvertFuncType* CoordConvertUE2Hou {nullptr};
	CoordConvertFuncType* CoordConvertHou2Ue {nullptr};	
};

struct FDataGather_PODExport : public FDataGather_ExportInfo
{
	FDataGather_PODExport(const FProperty* Property)
		:FDataGather_ExportInfo(Property)
	{
		
	}
	FScriptArray Container;
};

struct FDataGather_StringExport : public FDataGather_ExportInfo
{
	FDataGather_StringExport(const FProperty* Property)
		:FDataGather_ExportInfo(Property)
	{
		
	}
	TArray<FString> Container;
	/*Todo Struct Export by Actual Struct*/
};

struct FDataGather_Struct : public FDataGather_Base
{
	FDataGather_Struct(const UScriptStruct* BaseStruct)
		:FDataGather_Base(BaseStruct)
	{
		
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
			const UScriptStruct* ConvertTo = StructProperty->Struct;
			bool bExportString = false;
			if(const auto* Re = FStructConvertSpecialization::RegisteredSpecializations.Find(StructProperty->Struct))
			{
				ConvertTo = Re->ToStruct;
				bExportString = Re->bExportString;
			}
			if(PodStructsStorageInfo.Find(ConvertTo->GetFName()))
			{
				Add<FDataGather_PODExport>(FirstEnterProp);
			}
			else if(bExportString)
			{
				Add<FDataGather_StringExport>(FirstEnterProp);
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
	template<typename TVisitor>
	void Accept(TVisitor&& Visitor)
	{
		for(auto& Child : Children)
		{
			Visitor.Visit(Child);
		}
	}
};

struct TSwitchEnumContainerVisitor
{
	TSwitchEnumContainerVisitor(bool InbSwitchToString)
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
				CurrentVisitStructExport->Children[ChildId].Set<FDataGather_StringExport>(InputProperty);
				//Todo Post Set? Or Automatic down in construct
			}
			else
			{
				CurrentVisitStructExport->Children[ChildId].Set<FDataGather_PODExport>(InputProperty);
				//Todo Post Set?
			}
		}
		ExportIndicesInStructWithEnum.Reset();
	}

	void Visit(FDataGather_Struct& Struct)
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
				Visit(*WaitToProcess.Pop());
			}
		}
	}
	template<typename T>
	void Visit(T& Exporter)
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

struct TGatherDataVisitor
{
	void Visit(FDataGather_Struct& Struct)
	{
		
	}
	void Visit(FDataGather_PODExport& PODData)
	{
		
	}
	void Visit(FDataGather_StringExport& String)
	{
		
	}
};