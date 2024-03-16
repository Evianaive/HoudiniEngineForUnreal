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
	using Func = void(const uint8*, uint8*);
	UScriptStruct* FromStruct {nullptr};
	UScriptStruct* ToStruct {nullptr};
	Func* ConvertToRaw {nullptr};
	Func* ConvertBackRaw {nullptr};
	/*use to specify that some struct must export as string (for example curve ramp)*/
	bool bExportString {false};

	template<bool bUE2Hou>
	Func* GetConversionMethodRaw() const
	{
		if constexpr (bUE2Hou)
			return ConvertToRaw;
		else
			return ConvertBackRaw;
	}
	template<bool bUE2Hou>
	FInstancedStruct PerformConvert(const uint8* InPtr) const
	{
		FInstancedStruct Struct{bUE2Hou?ToStruct:FromStruct};
		GetConversionMethodRaw<bUE2Hou>()(InPtr,Struct.GetMutableMemory());
		return Struct;
	}
	
	static void RegisterConvertFunc(
		UScriptStruct* InFromStruct,
		UScriptStruct* InToStruct = nullptr,
		Func* InConvertTo = nullptr,
		Func* InConvertBack = nullptr,
		bool InbExportString = false)
	{
		auto& Specialization = RegisteredSpecializations.FindOrAdd(InFromStruct);
		Specialization.FromStruct = InFromStruct;
		Specialization.ToStruct = InToStruct;
		Specialization.ConvertToRaw = InConvertTo;
		Specialization.ConvertBackRaw = InConvertBack;
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

struct FDataExchange_Struct;
struct FDataExchange_POD;
struct FDataExchange_String;

using FDataGather_Variant = TVariant<
	FDataExchange_Struct,
	FDataExchange_POD,
	FDataExchange_String>;

struct FDataExchange_Base
{
	FDataExchange_Base(const FProperty* InProperty)
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

	FDataExchange_Base(const UScriptStruct* InBaseStruct)
		:FromStruct(InBaseStruct)
	{
		if(const auto Re = FStructConvertSpecialization::RegisteredSpecializations.Find(FromStruct))
		{
			ConvertSpecialization = *Re; 
		}
	}
protected:
	template<typename Derived>
	void InitByParent(const FDataExchange_Struct& InParent);
	FDataExchange_Struct* ParentStruct {nullptr};
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

struct FDataExchange_Info : public FDataExchange_Base
{
	explicit FDataExchange_Info(const FProperty* Property);
	HAPI_AttributeInfo Info {};
	TArray<int32> SizeFixedArray;
	// Todo Perform CoordConvert
	CoordConvertFuncType* CoordConvertUE2Hou {nullptr};
	CoordConvertFuncType* CoordConvertHou2Ue {nullptr};
};

struct FDataExchange_POD : public FDataExchange_Info
{
	FDataExchange_POD(const FProperty* InProperty)
		:FDataExchange_Info(InProperty) // Here we have already get the tuple size and hapi_storage
		,ContainerHelper(Property,ConvertSpecialization.ToStruct)
	{
		// add function to process property we pass in here to convert to hapi_storage type and tuple size?
	}
	// FScriptArray Container;
	FCustomScriptArrayHelper ContainerHelper;
	static void FillHapiAttribInfo(HAPI_AttributeInfo& AttributeInfo,const FProperty* InProperty,bool InbInArrayOfStruct);
	
	// Todo Perform CoordConvert
	template<bool bAddInfoCount = true>
	void PropToContainer(const uint8* Ptr);
	void ArrayPropToContainer(const uint8* Ptr);
	template<bool bMinInfoCount = true>
	void ContainerToProp(uint8* Ptr);
	void ContainerToArrayProp(uint8* Ptr);
	
	void* GetContainer(){return &const_cast<FScriptArray&>(ContainerHelper.GetArray());};
	void* GetContainerRawPtr(){return ContainerHelper.GetRawPtr();};
	
	void UnpackArray();
	void PackArray();
};

struct FDataExchange_String : public FDataExchange_Info
{
	FDataExchange_String(const FProperty* Property)
		:FDataExchange_Info(Property)
	{}
	TArray<FString> Container;
	// void Init(const FDataGather_Struct& InParent);
	/*Todo Struct Export by Actual Struct*/
	static void FillHapiAttribInfo(HAPI_AttributeInfo& AttributeInfo,const FProperty* InProperty,bool InbInArrayOfStruct);
	
	// Todo Perform CoordConvert if needed
	template<bool bAddInfoCount = true>
	void PropToContainer(const uint8* Ptr);
	void ArrayPropToContainer(const uint8* Ptr);
	template<bool bMinInfoCount = true>
	void ContainerToProp(uint8* Ptr);
	void ContainerToArrayProp(uint8* Ptr);
	
	void* GetContainer(){return &Container;}
	void* GetContainerRawPtr(){return Container.GetData();};
	
	void UnpackArray() const{};
	void PackArray() const{};
};

struct FDataExchange_Struct : public FDataExchange_Base
{
	FDataExchange_Struct(const UScriptStruct* BaseStruct)
		:FDataExchange_Base(BaseStruct)
	{
		//Todo Rename
		MakeChildren();
	}
	FDataExchange_Struct(const FProperty* Property)
		:FDataExchange_Base(Property)
	{
		bArrayOfStruct = (ArrayProperty != nullptr);
	}
	TArray<FDataGather_Variant> Children;
	bool bArrayOfStruct {false};
	/*Set when import data. Used when*/
	TArray<int32>* ChildSizeFixedArray {nullptr};

	// void GatherDataFromParent(const FDataGather_Struct& InParent)
	// {
	// 	bInArrayOfStruct = InParent.bArrayOfStruct || InParent.bInArrayOfStruct;
	// }
	
	void MakeChildren()
	{
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
				Add<FDataExchange_String>(InProp);
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
				Add<FDataExchange_String>(FirstEnterProp);
			}
			else if(PodStructsStorageInfo.Find(FinalStruct->GetFName()))
			{
				Add<FDataExchange_POD>(FirstEnterProp);
				//Todo this property doesn't match the struct if struct conversion is Specialization!
			}
			else
			{
				Add<FDataExchange_Struct>(FirstEnterProp);
			}
		}
		else if (InProp->IsA(FNumericProperty::StaticClass()) || InProp->IsA(FBoolProperty::StaticClass()))
		{
			Add<FDataExchange_POD>(FirstEnterProp);
		}
		else
		{
			Add<FDataExchange_String>(FirstEnterProp);
		}
	}

	template<typename TVariant,typename... TArgs>
	TVariant& Add(TArgs&&... Args)
	{
		TVariant& NewChild = (new (Children) FDataGather_Variant(TInPlaceType<TVariant>(),Forward<TArgs>(Args)...))->Get<TVariant>();
		// avoid compile error
		reinterpret_cast<FDataExchange_Struct&>(NewChild).template InitByParent<TVariant>(*this);
		return NewChild;
	}
	
	template<typename TVariant,typename... TArgs>
	TVariant* Set(int32 Index, TArgs&&... Args)
	{
		if(!Children.IsValidIndex(Index))
			return nullptr;
		Children[Index].Set<TVariant>(Forward<TArgs>(Args)...);
		TVariant& NewChild = Children[Index].Get<TVariant>();
		// avoid compile error
		reinterpret_cast<FDataExchange_Struct&>(NewChild).template InitByParent<TVariant>(*this);
		return &NewChild;
	}
	template<typename TVisitor, typename Ret = decltype(DeclVal<TVisitor>()(DeclVal<FDataGather_Variant>()))>
	typename TEnableIf<std::is_same_v<Ret,void>>::Type Accept(TVisitor&& Visitor)
	{
		// using ReturnType = decltype(Visitor(DeclVal<decltype(Children)::ElementType>()));
		for(auto& Child : Children)
		{
			::Visit(Visitor,Child);
		}
	}
	template<typename TVisitor, typename Ret = decltype(DeclVal<TVisitor>()(DeclVal<FDataGather_Variant>()))>
	typename TEnableIf<!std::is_same_v<Ret,void>,Ret>::Type Accept(TVisitor&& Visitor)
	{
		Ret Result = 1;
		for(auto& Child : Children)
		{
			Result |= ::Visit(Visitor,Child);
		}
		return Result;
	}

	void GetConvertSpecializationTo(const uint8* InData, FInstancedStruct& OutConversion)
	{
		if(ConvertSpecialization.ToStruct)
		{
			OutConversion = ConvertSpecialization.PerformConvert<true>(InData);
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
	FDataExchange_Struct* CurrentVisitStructExport {nullptr};
	TArray<FDataExchange_Struct*> WaitToProcess;

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
				CurrentVisitStructExport->Set<FDataExchange_String>(ChildId,InputProperty);
			}
			else
			{
				CurrentVisitStructExport->Set<FDataExchange_POD>(ChildId,InputProperty);
			}
		}
		ExportIndicesInStructWithEnum.Reset();
	}
public:
	void operator()(FDataExchange_Struct& Struct)
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
		constexpr bool bStringContainer = std::is_same_v<T,FDataExchange_String>;
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

struct FFoldDataVisitor
{
	FFoldDataVisitor(uint8* StructPtr);
	uint8* Ptr;
	UScriptStruct* Struct;
	bool bInArrayOfStruct;
	int32 AddElementCount;
	
	enum
	{
		InArrayOfStructSignal = -1,
		Normal = -2
	};
	void Reset();
	void Reset(uint8* StructPtr);
	bool ConvertPtr(FDataExchange_Struct& StructExport, FInstancedStruct& OutInstanceStruct)
	{
		uint8* CachePtr = Ptr;
		if(StructExport.ConvertSpecialization.ToStruct)
			Ptr = OutInstanceStruct.GetMutableMemory();
		bool Result = StructExport.Accept(*this);
		if(StructExport.ConvertSpecialization.ToStruct)
			StructExport.ConvertSpecialization.ConvertBackRaw(Ptr,CachePtr);
		return Result;
	}
	bool operator()(FDataExchange_Struct& StructExport)
	{
		/*必须从后往前转换成，对于Array属性，其序号无法维护，因为有SizeFixedArray，需要对每个Array都维护一个序号，这是不可能的*/
		TGuardValue GuardPtr(Ptr,Ptr+StructExport.OffsetInParentStruct);
		TGuardValue GuardArray(bInArrayOfStruct,(bInArrayOfStruct||StructExport.bArrayOfStruct));
		
		FInstancedStruct ConvertResult{StructExport.ConvertSpecialization.FromStruct};
		if(StructExport.ArrayProperty)
		{
			bool Result = true;
			FScriptArrayHelper Helper(StructExport.ArrayProperty,Ptr);
			if(StructExport.ChildSizeFixedArray == nullptr)
			{
				UE_LOG(LogTemp,Log,TEXT("Invalid SizeFixedArray"));
				return false;
			}
			const int32 Num = StructExport.ChildSizeFixedArray->Last();
			const int32 Start = Helper.AddValues(Num);
			
			TGuardValue GuardAddCount(AddElementCount,static_cast<int32>(InArrayOfStructSignal));
			for(int32 i = Start;i<Start+Helper.Num();i++)
			{
				Ptr = Helper.GetRawPtr(i);
				Result &= ConvertPtr(StructExport,ConvertResult);
			}
			
			AddElementCount=Helper.Num();
			Result &= StructExport.Accept(*this);
			return Result;
		}
		else
		{
			// AddElementCount = Normal;
			// AddElementCount = FMath::Max(Normal,AddElementCount);
			return ConvertPtr(StructExport,ConvertResult);
		}
	}
	template<typename Export>
	bool operator()(Export& PODExport)
	{
		FDataExchange_Info& AsExport = PODExport;
		TGuardValue GuardPtr(Ptr,Ptr+AsExport.OffsetInParentStruct);

		if(!AsExport.Info.exists)
			return false;
		
		if(AddElementCount>InArrayOfStructSignal)
		{
			AsExport.SizeFixedArray.RemoveAt(AsExport.SizeFixedArray.Num()-1);
			
			AsExport.Info.totalArrayElements -= AddElementCount;
			AsExport.Info.count -= 1;
		}
		else
		{
			if(
			// HapiStorageTraits::IsArrayStorage(AsExport.Info.storage) 
			AsExport.ArrayProperty
			&& !bInArrayOfStruct)
			{
				PODExport.ContainerToArrayProp(Ptr);
			}
			else
			{
				AddElementCount==Normal?PODExport.ContainerToProp(Ptr):PODExport.ContainerToProp<false>(Ptr);
			}
		}
		return true;
	}
};

struct FUnfoldDataVisitor
{
	FUnfoldDataVisitor(const uint8* StructPtr);
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
	
	void ConvertPtr(FDataExchange_Struct& StructExport, FInstancedStruct& OutInstanceStruct)
	{
		if(StructExport.ConvertSpecialization.ToStruct)
		{
			StructExport.GetConvertSpecializationTo(Ptr,OutInstanceStruct);
			Ptr = OutInstanceStruct.GetMutableMemory();
		}
	}
	bool operator()(FDataExchange_Struct& StructExport)
	{
		// ScopeExitSupport::TScopeGuard<>();
		// ON_SCOPE_EXIT{return;}
		TGuardValue GuardPtr(Ptr,Ptr+StructExport.OffsetInParentStruct);
		TGuardValue GuardArray(bInArrayOfStruct,(bInArrayOfStruct||StructExport.bArrayOfStruct));
		FInstancedStruct ConvertResult;
		bool Result = true;
		if(StructExport.ArrayProperty)
		{
			FScriptArrayHelper Helper(StructExport.ArrayProperty,Ptr);
			TGuardValue GuardAddCount(AddElementCount,static_cast<int32>(InArrayOfStructSignal));
			for(int32 i = 0;i<Helper.Num();i++)
			{
				Ptr = Helper.GetRawPtr(i);
				ConvertPtr(StructExport,ConvertResult);
				Result &= StructExport.Accept(*this);
			}
			//Todo 这里可以更改为只给其中一个成员吗，在export时也只使用其上面的
			AddElementCount = Helper.Num();
			Result &= StructExport.Accept(*this);
		}
		else
		{
			// AddElementCount = Normal;
			// AddElementCount = FMath::Max(Normal,AddElementCount);
			ConvertPtr(StructExport,ConvertResult);
			Result &= StructExport.Accept(*this);
		}
		return Result;
	}
	template<typename Export>
	bool operator()(Export& PODExport)
	{
		FDataExchange_Info& AsExport = PODExport;
		TGuardValue GuardPtr(Ptr,Ptr+AsExport.OffsetInParentStruct);
		
		if(AddElementCount>InArrayOfStructSignal)
		{
			AsExport.SizeFixedArray.Add(AddElementCount);
			AsExport.Info.totalArrayElements += AddElementCount;
			AsExport.Info.count += 1;
		}
		else
		{
			// AddElementCount == Normal or InArrayOfStructSignal
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
		return true;
	}
};

struct FExportDataVisitor
{
	const HAPI_Session* SessionId {nullptr};
	HAPI_NodeId NodeId {0};
	HAPI_AttributeOwner Owner {HAPI_AttributeOwner::HAPI_ATTROWNER_INVALID};
	HAPI_PartId PartId {0};
	bool bTryEncode {false};

	bool operator()(FDataExchange_Struct& StructExport)
	{
		return StructExport.Accept(*this);
	}
	template<typename Export>
	bool operator()(Export& Gather);
};

struct FImportDataVisitor
{
	const HAPI_Session* SessionId {nullptr};
	HAPI_NodeId NodeId {0};
	HAPI_AttributeOwner Owner {HAPI_AttributeOwner::HAPI_ATTROWNER_INVALID};
	HAPI_PartId PartId {0};

	FDataExchange_Struct* ArrayOfStructExport {nullptr};
	
	bool operator()(FDataExchange_Struct& StructExport)
	{
		if(StructExport.bArrayOfStruct)
		{
			TGuardValue GuardPtr(ArrayOfStructExport,&StructExport);
		}		
		return StructExport.Accept(*this);
	}
	template<typename Export>
	bool operator()(Export& Gather);
};