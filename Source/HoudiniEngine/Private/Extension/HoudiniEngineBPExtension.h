// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HoudiniGeoPartObject.h"
#include "HAPI/HAPI_Common.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HoudiniEngineBPExtension.generated.h"

class UHoudiniAssetComponent;
class UUserDefinedStruct;
/**
 * 
 */
UENUM()
enum class EAttributeOwner : uint8
{
	Vertex = 0,
	Point,
	Prim,
	Detail
};

UCLASS(BlueprintType,Config=HoudiniEngineExtension)
class UScriptStructWrapper : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Config,meta=(AllowedClasses="/Script/Engine.UserDefinedStruct"))
	FSoftObjectPath ScriptStruct;
	UFUNCTION(BlueprintCallable,BlueprintPure)
	UUserDefinedStruct* GetStruct();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
};

USTRUCT(BlueprintType)
struct FHoudiniNodeCookState
{
	GENERATED_BODY()
	FHoudiniNodeCookState();
	UPROPERTY(BlueprintReadWrite)
	bool bCooked;
	const HAPI_CookOptions* Options;

	static const HAPI_CookOptions* GetDefaultCookOptions();
};

// Force the user make an valid instance in blueprint
USTRUCT(BlueprintType,meta=(HasNativeMake="/Script/HoudiniEngine.HoudiniEngineBPExtension.MakeHoudiniNode_BP"))
struct FHoudiniNode
{
	GENERATED_BODY()

	FHoudiniNode(){};
	FHoudiniNode(int32 InNodeId);

	bool CookNodeNode(bool bWaitForCompletion = true);
	bool IsValid() const
	{
		return NodeId != -1 && Session!=nullptr;
	}
	
	const HAPI_Session* Session {nullptr};
	UPROPERTY(BlueprintReadWrite, meta=(ExposeOnSpawn))
	int32 NodeId{-1};
	UPROPERTY(BlueprintReadWrite)
	FHoudiniNodeCookState CookState{};
};

UCLASS()
class HOUDINIENGINE_API UHoudiniEngineBPExtension : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "HoudiniEngine Extension", meta = (NativeMakeFunc, DisplayName = "Make Houdini Node"))
	static FHoudiniNode MakeHoudiniNode_BP(int32 InNodeId) { return FHoudiniNode(InNodeId); }

	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static bool CookNode(UPARAM(ref)FHoudiniNode& Node, bool bWaitForCompletion = true) { return Node.CookNodeNode(bWaitForCompletion); }
	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static bool GetNodePartInfo(UPARAM(ref)FHoudiniNode& InNode, int32 PartId, EHoudiniPartType& Type, int32& VertexCount, int32& PointCount, int32& FaceCount);
	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static bool GetNodeInputOutputInfo(UPARAM(ref)FHoudiniNode& InNode, TArray<FName>& Inputs, TArray<FName>& Outputs);
	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static bool QueryNodeOutputConnectNodes(UPARAM(ref)FHoudiniNode& InNode, int32 OutputIndex, TArray<int32>& OutputNodeIds);
	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static bool QueryNodeInputConnectNode(UPARAM(ref)FHoudiniNode& InNode, int32 InputIndex, int32& InputNodeId);
	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static bool ConnectNodeOutputToNodeInput(UPARAM(ref)FHoudiniNode& OutputNode, int32 OutputIndex, UPARAM(ref)FHoudiniNode& InputNode, int32 InputIndex);
	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static bool DisConnectNodeInput(UPARAM(ref)FHoudiniNode& InNode, int32 InputIndex);
	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension", CustomThunk, meta=(CustomStructureParam="Value"))
	static bool SetParamValue(UPARAM(ref)FHoudiniNode& InNode, const FString& ParamName, const int32 ParamTupleIndex, const int32& Value);
	static bool Generic_SetParamValue(FHoudiniNode& InNode, const FString& ParamName, int32 ParamTupleIndex,const void* Value, const FProperty* Property);
	DECLARE_FUNCTION(execSetParamValue);

	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static int32 GetNodeItemCount(UPARAM(ref)FHoudiniNode& InNode, int32 PartId, EAttributeOwner Type);

	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static int32 CreateSOPNode(int32 InParentId = -1, FString InNodeName = TEXT("null"), const FString& InNodeLabel = TEXT("Input"), bool bWaitForCompletion = true);
	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static int32 CreateInputNode(const FString& InNodeLabel);
	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension", meta=(ToolTip="Counts is vertex count, point count, face count"))
	static bool SetPartInfo(UPARAM(ref)const FHoudiniNode& InNode, int32 InPartId, FIntVector Counts, bool bCreateDefaultP = true);
	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static int32 GetNodeParent(UPARAM(ref)const FHoudiniNode& InNode);
	
	template<typename TStruct>
	static bool SetArrayOfStructOnNode(const TArray<TStruct>& InArrayOfStruct, const FHoudiniNode& InNode, int32 PartId, EAttributeOwner ImportLevel, bool bCommitGeo = true);
	static bool SetArrayOfStructOnNodeInternal(const FScriptArray& InArrayOfStruct,	const UScriptStruct* InStruct, const FHoudiniNode& InNode, int32 PartId, EAttributeOwner ImportLevel, bool bCommitGeo = true);

	template<typename TStruct>
	static bool GetArrayOfStructOnNode(TArray<TStruct>& InArrayOfStruct, FHoudiniNode& InNode, int32 InPartId, EAttributeOwner ImportLevel);
	static bool GetArrayOfStructOnNodeInternal(FScriptArray& InArrayOfStruct, const UScriptStruct* InStruct, FHoudiniNode& InNode, int32 InPartId, EAttributeOwner ImportLevel);
	
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(ArrayParm = "InArrayOfStruct"))
	static bool SetArrayOfStructOnNode_BP(const TArray<int32>& InArrayOfStruct, UPARAM(ref) const FHoudiniNode& InNode, int32 InPartId, EAttributeOwner ImportLevel, bool bCommitGeo = true);
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(ArrayParm = "InArrayOfStruct"))
	static bool GetArrayOfStructOnNode_BP(TArray<int32>& InArrayOfStruct, UPARAM(ref) const FHoudiniNode& InNode, int32 InPartId, EAttributeOwner ImportLevel);
	
	DECLARE_FUNCTION(execSetArrayOfStructOnNode_BP);
	DECLARE_FUNCTION(execGetArrayOfStructOnNode_BP);

	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static void GetStructPropertyNameArray(UScriptStruct* ScriptStruct, TArray<FName>& Properties);
	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static void GetStructPropertyAuthorNameArray(UScriptStruct* ScriptStruct, TArray<FString>& Properties);
	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static bool GetPropertyMetaData(const FName& PropertyName, UScriptStruct* ScriptStruct, TMap<FName,FString>& MetaData);
	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static bool SetPropertyMetaData(const FName& PropertyName, UScriptStruct* ScriptStruct, UPARAM(ref) const TMap<FName,FString>& MetaData);
	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static bool MarkObjectDirty(UObject* ObjectChanged);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HoudiniEngine Extension")
	static int32 GetHoudiniAssetComponentNodeId(const UHoudiniAssetComponent* AssetComponent);
};

template <typename TStruct>
bool UHoudiniEngineBPExtension::SetArrayOfStructOnNode(const TArray<TStruct>& InArrayOfStruct,
	const FHoudiniNode& InNode, int32 InPartId, EAttributeOwner ImportLevel, bool bCommitGeo)
{
	// InArrayOfStruct
	return UHoudiniEngineBPExtension::SetArrayOfStructOnNodeInternal(reinterpret_cast<const FScriptArray&>(InArrayOfStruct),TBaseStructure<TStruct>::Get(),InNode,InPartId,ImportLevel,bCommitGeo);
}

template <typename TStruct>
bool UHoudiniEngineBPExtension::GetArrayOfStructOnNode(TArray<TStruct>& InArrayOfStruct, FHoudiniNode& InNode,
	int32 InPartId, EAttributeOwner ImportLevel)
{
	int32 Count = GetNodeItemCount(InNode,InPartId,ImportLevel);
	if(Count<=0)
		return Count==0;
	InArrayOfStruct.SetNumZeroed(Count);
	return GetArrayOfStructOnNodeInternal(reinterpret_cast<FScriptArray&>(InArrayOfStruct),TBaseStructure<TStruct>::Get(),InNode,InPartId);
}
