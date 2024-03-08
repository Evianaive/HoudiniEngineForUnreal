﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HoudiniGeoPartObject.h"
#include "HAPI/HAPI_Common.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HoudiniEngineBPExtension.generated.h"

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

	UFUNCTION(BlueprintPure, Category = "HoudiniEngine Extension", meta = (NativeMakeFunc, DisplayName = "Make Houdini Node"))
	static FHoudiniNode MakeHoudiniNode_BP(int32 InNodeId) { return FHoudiniNode(InNodeId); }

	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static bool CookNode(UPARAM(ref)FHoudiniNode& Node, bool bWaitForCompletion = true) { return Node.CookNodeNode(bWaitForCompletion); }
	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static bool GetNodePartInfo(UPARAM(ref)FHoudiniNode& InNode, int32 PartId, EHoudiniPartType& Type, int32& FaceCount, int32& VertexCount, int32& PointCount);
	UFUNCTION(BlueprintCallable, Category = "HoudiniEngine Extension")
	static int32 GetNodeItemCount(UPARAM(ref)FHoudiniNode& InNode, int32 PartId, EAttributeOwner Type);
	
	template<typename TStruct>
	static void SetArrayOfStructOnNode(const TArray<TStruct>& InArrayOfStruct, const FHoudiniNode& InNode, int32 PartId, EAttributeOwner ImportLevel, bool bCommitGeo = true);
	static void SetArrayOfStructOnNodeInternal(const FScriptArray& InArrayOfStruct,	const UScriptStruct* InStruct, const FHoudiniNode& InNode, int32 PartId, EAttributeOwner ImportLevel, bool bCommitGeo = true);

	template<typename TStruct>
	static void GetArrayOfStructOnNode(TArray<TStruct>& InArrayOfStruct, FHoudiniNode& InNode, int32 InPartId, EAttributeOwner ImportLevel);
	static void GetArrayOfStructOnNodeInternal(FScriptArray& InArrayOfStruct, const UScriptStruct* InStruct, FHoudiniNode& InNode, int32 InPartId, EAttributeOwner ImportLevel);
	
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(ArrayParm = "InArrayOfStruct"))
	static void SetArrayOfStructOnNode_BP(const TArray<int32>& InArrayOfStruct, UPARAM(ref) const FHoudiniNode& InNode, int32 InPartId, EAttributeOwner ImportLevel, bool bCommitGeo = true);
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(ArrayParm = "InArrayOfStruct"))
	static void GetArrayOfStructOnNode_BP(TArray<int32>& InArrayOfStruct, UPARAM(ref) const FHoudiniNode& InNode, int32 InPartId, EAttributeOwner ImportLevel);
	
	DECLARE_FUNCTION(execSetArrayOfStructOnNode_BP);
	DECLARE_FUNCTION(execGetArrayOfStructOnNode_BP);
	
};

template <typename TStruct>
void UHoudiniEngineBPExtension::SetArrayOfStructOnNode(const TArray<TStruct>& InArrayOfStruct,
	const FHoudiniNode& InNode, int32 InPartId, EAttributeOwner ImportLevel, bool bCommitGeo)
{
	// InArrayOfStruct
	UHoudiniEngineBPExtension::SetArrayOfStructOnNodeInternal(reinterpret_cast<const FScriptArray&>(InArrayOfStruct),TBaseStructure<TStruct>::Get(),InNode,InPartId,ImportLevel,bCommitGeo);
}

template <typename TStruct>
void UHoudiniEngineBPExtension::GetArrayOfStructOnNode(TArray<TStruct>& InArrayOfStruct, FHoudiniNode& InNode,
	int32 InPartId, EAttributeOwner ImportLevel)
{
	int32 Count = GetNodeItemCount(InNode,InPartId,ImportLevel);
	if(Count<=0)
		return;
	InArrayOfStruct.SetNumZeroed(Count);
	GetArrayOfStructOnNodeInternal(reinterpret_cast<FScriptArray&>(InArrayOfStruct),TBaseStructure<TStruct>::Get(),InNode,InPartId);
}
