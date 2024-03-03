// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/Object.h"
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


// Force the user make an valid instance in blueprint
USTRUCT(BlueprintType,meta=(HasNativeMake="/Script/HoudiniEngine.HoudiniEngineBPExtension.MakeHoudiniNode_BP"))
struct FHoudiniNode
{
	GENERATED_BODY()

	FHoudiniNode(){};
	FHoudiniNode(int32 InNodeId);
	
	const HAPI_Session* Session {nullptr};
	UPROPERTY(BlueprintReadWrite, meta=(ExposeOnSpawn))
	int32 NodeId{-1};
};

UCLASS()
class HOUDINIENGINE_API UHoudiniEngineBPExtension : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintPure, Category = "HoudiniEngine Extension", meta = (NativeMakeFunc, DisplayName = "Make Houdini Node"))
	static FHoudiniNode MakeHoudiniNode_BP(int32 InNodeId) { return FHoudiniNode(InNodeId); }

	template<typename TStruct>
	static void SetArrayOfStructOnNode(const TArray<TStruct>& InArrayOfStruct, const FHoudiniNode& InNode, EAttributeOwner ImportLevel);
	static void SetArrayOfStructOnNodeInternal(const FScriptArray& InArrayOfStruct, const FHoudiniNode& InNode, EAttributeOwner ImportLevel);

	template<typename TStruct>
	static void GetArrayOfStructOnNode(TArray<TStruct>& InArrayOfStruct, const FHoudiniNode& InNode, EAttributeOwner ImportLevel);
	static void GetArrayOfStructOnNodeInternal(FScriptArray& InArrayOfStruct, const FHoudiniNode& InNode, EAttributeOwner ImportLevel);
	
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(ArrayParm = "InArrayOfStruct"))
	static void SetArrayOfStructOnNode_BP(const TArray<int32>& InArrayOfStruct, UPARAM(ref) const FHoudiniNode& InNode, EAttributeOwner ImportLevel);
	//
	DECLARE_FUNCTION(execSetArrayOfStructOnNode_BP);
	
};

template <typename TStruct>
void UHoudiniEngineBPExtension::SetArrayOfStructOnNode(const TArray<TStruct>& InArrayOfStruct,
	const FHoudiniNode& InNode, EAttributeOwner ImportLevel)
{
}

template <typename TStruct>
void UHoudiniEngineBPExtension::GetArrayOfStructOnNode(TArray<TStruct>& InArrayOfStruct, const FHoudiniNode& InNode,
	EAttributeOwner ImportLevel)
{
}
