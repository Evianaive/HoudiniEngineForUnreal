// Fill out your copyright notice in the Description page of Project Settings.
PRAGMA_DISABLE_INLINING
PRAGMA_DISABLE_OPTIMIZATION

#include "HoudiniEngineBPExtension.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniOutputTranslator.h"
#include "UnrealStructExpand.h"


FHoudiniNodeCookState::FHoudiniNodeCookState()
	:bCooked(false)
	,Options(GetDefaultCookOptions())
{
	
}

const HAPI_CookOptions* FHoudiniNodeCookState::GetDefaultCookOptions()
{
	static HAPI_CookOptions DefaultCookOptions =
		[]()
		{
			auto Options = FHoudiniEngine::Get().GetDefaultCookOptions();
			return Options; 
		}();
	return &DefaultCookOptions;
}

FHoudiniNode::FHoudiniNode(int32 InNodeId)
	:Session(FHoudiniEngine::Get().GetSession())
	,NodeId(InNodeId)	
{}

bool FHoudiniNode::CookNodeNode(bool bWaitForCompletion)
{
	// Check for an invalid node id
	if (NodeId < 0)
		return false;
	
	if(CookState.bCooked)
		return true;
	
	// No Cook Options were specified, use the default one
	if (CookState.Options == nullptr)
	{
		// Use the default cook options
		const HAPI_CookOptions* CookOptions = FHoudiniNodeCookState::GetDefaultCookOptions();
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
			FHoudiniEngine::Get().GetSession(), NodeId, CookOptions), false);
	}
	else
	{
		// Use the provided CookOptions
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
			FHoudiniEngine::Get().GetSession(), NodeId, CookState.Options), false);
	}

	// If we don't need to wait for completion, return now
	if (!bWaitForCompletion)
	{
		CookState.bCooked = false;
		return true;
	}

	// Wait for the cook to finish
	HAPI_Result Result = HAPI_RESULT_SUCCESS;
	while (true)
	{
		// Get the current cook status
		int Status = HAPI_STATE_STARTING_COOK;
		HOUDINI_CHECK_ERROR_GET(&Result, FHoudiniApi::GetStatus(
			FHoudiniEngine::Get().GetSession(), HAPI_STATUS_COOK_STATE, &Status));

		if (Status == HAPI_STATE_READY)
		{
			// The cook has been successful.
			CookState.bCooked = true;
			return true;
		}
		else if (Status == HAPI_STATE_READY_WITH_FATAL_ERRORS || Status == HAPI_STATE_READY_WITH_COOK_ERRORS)
		{
			// There was an error while cooking the node.
			//FString CookResultString = FHoudiniEngineUtils::GetCookResult();
			//HOUDINI_LOG_ERROR();
			CookState.bCooked = false;
			return false;
		}

		// We want to yield a bit.
		FPlatformProcess::Sleep(0.1f);
	}
};

bool UHoudiniEngineBPExtension::GetNodePartInfo(
	FHoudiniNode& InNode, int32 PartId, EHoudiniPartType& Type,
	int32& FaceCount, int32& VertexCount, int32& PointCount)
{
	if(!InNode.CookNodeNode(true))
		return false;
	
	HAPI_PartInfo PartInfo;
	HOUDINI_CHECK_RETURN(FHoudiniApi::GetPartInfo(
		InNode.Session,InNode.NodeId,PartId,&PartInfo),false);
	
	Type = FHoudiniOutputTranslator::ConvertHapiPartType(PartInfo.type);
	FaceCount = PartInfo.faceCount;
	VertexCount = PartInfo.vertexCount;
	PointCount = PartInfo.pointCount;
	return true;
}

int32 UHoudiniEngineBPExtension::GetNodeItemCount(FHoudiniNode& InNode, int32 InPartId, EAttributeOwner ImportLevel)
{
	EHoudiniPartType PartType;	
	FIntVector3 Counts;
	if(!GetNodePartInfo(InNode, InPartId, PartType,
		Counts.X,Counts.Y,Counts.Z))
			return -1;
	int32 Count = -1;
	switch (ImportLevel)
	{
	case EAttributeOwner::Vertex: Count = Counts.X;	break;
	case EAttributeOwner::Point: Count = Counts.Y;	break;
	case EAttributeOwner::Prim: Count = Counts.Z;	break;
	case EAttributeOwner::Detail: Count = 1;		break;
	default:return -1;
	}
	return Count;
}

int32 UHoudiniEngineBPExtension::CreateNode(int32 InParentId, const FString& InNodeName, const FString& InNodeLabel)
{
	int32 OutNodeId = -1;
	FHoudiniEngineUtils::CreateNode(InParentId,InNodeName,InNodeLabel,true,&OutNodeId);
	return OutNodeId;
}

int32 UHoudiniEngineBPExtension::CreateInputNode(const FString& InNodeLabel)
{
	int32 OutNodeId = -1;
	FHoudiniEngineUtils::CreateInputNode(InNodeLabel,OutNodeId);
	return OutNodeId;
}

bool UHoudiniEngineBPExtension::SetPartInfo(const FHoudiniNode& InNode, int32 InPartId, FIntVector Counts, bool bCreateDefaultP)
{
	if(!InNode.IsValid())
	{
		HOUDINI_LOG_ERROR(TEXT("Invalid NodeId or SessionId"));
		return false;
	}
	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);
	Part.id = InPartId;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.vertexCount = Counts.X;
	Part.faceCount = Counts.Y;
	Part.pointCount = Counts.Z;
	Part.type = HAPI_PARTTYPE_MESH;
	
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
		InNode.Session, InNode.NodeId, InPartId, &Part),false);

	// Add Default P Attrib
	if(bCreateDefaultP)
	{
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = Counts.Z;
		AttributeInfoPoint.tupleSize = 3;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;
	
		FHoudiniApi::AddAttribute(
		InNode.Session, InNode.NodeId, InPartId,
		HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint);
#if 1
		// TArray<float> EmptyArray{0.0f};
		const FVector3f DefaultPos{0,0,0};
		// FHoudiniApi::SetAttributeFloatUniqueData(InNode.Session,InNode.NodeId,PartId,
		// 	HAPI_UNREAL_ATTRIB_POSITION,&AttributeInfoPoint,&DefaultPos.X,3,0,InArrayOfStruct.Num());

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatUniqueData(
					DefaultPos.X, InNode.NodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, AttributeInfoPoint), false);
#else
		// Set the point's position
		FVector3f ObjectPosition{4,5,6};
		TArray<float> Position;
		for (int i = 0; i<InArrayOfStruct.Num();i++)
		{
			Position.Append({ObjectPosition.X,ObjectPosition.Y,ObjectPosition.Z});	
		}
		// Now that we have raw positions, we can upload them for our attribute.
		FHoudiniEngineUtils::HapiSetAttributeFloatData(
			Position, InNode.NodeId, PartId, HAPI_UNREAL_ATTRIB_POSITION, AttributeInfoPoint);
#endif
	}
	return true;
}

bool UHoudiniEngineBPExtension::SetArrayOfStructOnNodeInternal(
	const FScriptArray& InArrayOfStruct,
	const UScriptStruct* InStruct,
	const FHoudiniNode& InNode,
	int32 PartId,
	EAttributeOwner ImportLevel,
	bool bCommitGeo)
{
	if(!InNode.IsValid())
	{
		HOUDINI_LOG_ERROR(TEXT("Invalid NodeId or SessionId"));
		return false;
	}
	FDataExchange_Struct ExchangeData{InStruct};
	FUnfoldDataVisitor UnfoldDataVisitor{nullptr};
	const int32 Size = InStruct->GetStructureSize();
	const uint8* Data = static_cast<const uint8*>(InArrayOfStruct.GetData());
	bool Result = true;
	for(int i=0;i<InArrayOfStruct.Num();i++)
	{
		UnfoldDataVisitor.Reset(Data);
		Result &= ExchangeData.Accept(UnfoldDataVisitor);
		Data+=Size;
	}
	FExportDataVisitor ExportDataVisitor{InNode.Session,InNode.NodeId,static_cast<HAPI_AttributeOwner>(ImportLevel),PartId};
	Result &= ExchangeData.Accept(ExportDataVisitor);
	const auto SessionId = FHoudiniEngine::Get().GetSession();
	if(bCommitGeo)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
			SessionId,InNode.NodeId),false);
	}
	return Result;
}

bool UHoudiniEngineBPExtension::GetArrayOfStructOnNodeInternal(
	FScriptArray& InArrayOfStruct,
	const UScriptStruct* InStruct,
	FHoudiniNode& InNode,
	int32 PartId,
	EAttributeOwner ImportLevel)
{
	if(!InNode.IsValid())
	{
		HOUDINI_LOG_ERROR(TEXT("Invalid NodeId or SessionId"));
		return false;
	}
	FDataExchange_Struct ExchangeData{InStruct};
	FFoldDataVisitor FoldDataVisitor{nullptr};
	
	const int32 Size = InStruct->GetStructureSize();
	const int32 Count = InArrayOfStruct.Num();
	uint8* Data = static_cast<uint8*>(InArrayOfStruct.GetData()) + Size*(Count-1);
	bool Result = true;
	for(int i=Count-1;i>=0;i--)
	{
		FoldDataVisitor.Reset(Data);
		Result &= ExchangeData.Accept(FoldDataVisitor);
		Data-=Size;
	}
	FImportDataVisitor ImportDataVisitor{InNode.Session,InNode.NodeId,static_cast<HAPI_AttributeOwner>(ImportLevel),PartId};
	Result &= ExchangeData.Accept(ImportDataVisitor);
	return Result;
}

void UHoudiniEngineBPExtension::SetArrayOfStructOnNode_BP(
	const TArray<int32>& InArrayOfStruct,
	const FHoudiniNode& InNode, int32 PartId,
	EAttributeOwner ImportLevel,bool bCommitGeo)
{
	//We should never enter this
}

void UHoudiniEngineBPExtension::GetArrayOfStructOnNode_BP(
	TArray<int32>& InArrayOfStruct, const FHoudiniNode& InNode,
	int32 InPartId, EAttributeOwner ImportLevel)
{
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
	const UScriptStruct* InnerStruct = nullptr;
	if(auto* StructProperty = CastField<FStructProperty>(ArrayProperty->Inner))
	{
		InnerStruct = StructProperty->Struct;
	}
	P_GET_STRUCT_REF(FHoudiniNode,InNode);
	P_GET_PROPERTY(FIntProperty,InPartId);
	P_GET_ENUM(EAttributeOwner,ImportLevel);
	P_GET_UBOOL(bCommitGeo)

	P_FINISH;
	P_NATIVE_BEGIN;
	//Todo Add return bool
	SetArrayOfStructOnNodeInternal(*reinterpret_cast<const FScriptArray*>(ArrayAddr),InnerStruct,InNode,InPartId,ImportLevel,bCommitGeo);
	P_NATIVE_END;
	// InnerProp->DestroyValue(StorageSpace);
}

DEFINE_FUNCTION(UHoudiniEngineBPExtension::execGetArrayOfStructOnNode_BP)
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
	const UScriptStruct* InnerStruct = nullptr;
	if(auto* StructProperty = CastField<FStructProperty>(ArrayProperty->Inner))
	{
		InnerStruct = StructProperty->Struct;
	}
	P_GET_STRUCT_REF(FHoudiniNode,InNode);
	P_GET_PROPERTY(FIntProperty,InPartId);
	P_GET_ENUM(EAttributeOwner,ImportLevel);

	P_FINISH;
	P_NATIVE_BEGIN;
	int32 Count = GetNodeItemCount(InNode,InPartId,ImportLevel);
	if(Count<=0)
		//Todo Should we return like this?
		return;
	
	FScriptArrayHelper Helper{ArrayProperty,ArrayAddr};
	Helper.AddValues(Count-Helper.Num());
	//Todo Add return bool
	GetArrayOfStructOnNodeInternal(*reinterpret_cast<FScriptArray*>(ArrayAddr),InnerStruct,InNode,InPartId,ImportLevel);
	P_NATIVE_END;
}
PRAGMA_ENABLE_INLINING
PRAGMA_ENABLE_OPTIMIZATION