// Fill out your copyright notice in the Description page of Project Settings.


#include "HoudiniEngineBPExtension.h"

#include "HoudiniEngine.h"

 
FHoudiniNode::FHoudiniNode(int32 InNodeId)
	:Session(FHoudiniEngine::Get().GetSession())
	,NodeId(InNodeId)	
{};

void UHoudiniEngineBPExtension::SetArrayOfStructOnNodeInternal(const FScriptArray& InArrayOfStruct,
	const FHoudiniNode& InNode, EAttributeOwner ImportLevel)
{
}

void UHoudiniEngineBPExtension::GetArrayOfStructOnNodeInternal(FScriptArray& InArrayOfStruct,
	const FHoudiniNode& InNode, EAttributeOwner ImportLevel)
{
}

void UHoudiniEngineBPExtension::SetArrayOfStructOnNode_BP(
	const TArray<int32>& InArrayOfStruct,
	const FHoudiniNode& InNode,
	EAttributeOwner ImportLevel)
{
	//We should never enter this
}

DEFINE_FUNCTION(UHoudiniEngineBPExtension::execSetArrayOfStructOnNode_BP)
{
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FArrayProperty>(NULL);
	void* ArrayAddr = Stack.MostRecentPropertyAddress;
	FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
	if (!ArrayProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}

	P_GET_STRUCT_REF(FHoudiniNode,InNode);

	P_FINISH;
	P_NATIVE_BEGIN;
	// MARK_PROPERTY_DIRTY(Stack.Object, ArrayProperty);
	// GenericArray_Insert(ArrayAddr, ArrayProperty, StorageSpace, Index);
	P_NATIVE_END;
	// InnerProp->DestroyValue(StorageSpace);
}