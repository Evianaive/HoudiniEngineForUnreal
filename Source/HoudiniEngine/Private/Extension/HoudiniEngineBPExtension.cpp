// Fill out your copyright notice in the Description page of Project Settings.
PRAGMA_DISABLE_INLINING
PRAGMA_DISABLE_OPTIMIZATION

#include "HoudiniEngineBPExtension.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniOutputTranslator.h"
#include "UnrealStructExpand.h"
#include "Engine/UserDefinedStruct.h"


UUserDefinedStruct* UScriptStructWrapper::GetStruct()
{
	if(UUserDefinedStruct* Resolved = Cast<UUserDefinedStruct>(ScriptStruct.ResolveObject()))
		return Resolved;	
	return nullptr;
}

void UScriptStructWrapper::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(GET_MEMBER_NAME_CHECKED(UScriptStructWrapper,ScriptStruct)==PropertyChangedEvent.GetPropertyName())
		SaveConfig();
}

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

	if(Session == nullptr)
		Session = FHoudiniEngine::Get().GetSession();
	// No Cook Options were specified, use the default one
	if (CookState.Options == nullptr)
	{
		// Use the default cook options
		const HAPI_CookOptions* CookOptions = FHoudiniNodeCookState::GetDefaultCookOptions();
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
			Session, NodeId, CookOptions), false);
	}
	else
	{
		// Use the provided CookOptions
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
			Session, NodeId, CookState.Options), false);
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
	int32& VertexCount, int32& PointCount, int32& FaceCount)
{
	if(!InNode.CookNodeNode(true))
		return false;
	
	HAPI_PartInfo PartInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(
		InNode.Session,InNode.NodeId,PartId,&PartInfo),false);
	
	Type = FHoudiniOutputTranslator::ConvertHapiPartType(PartInfo.type);
	FaceCount = PartInfo.faceCount;
	VertexCount = PartInfo.vertexCount;
	PointCount = PartInfo.pointCount;
	return true;
}

bool UHoudiniEngineBPExtension::GetNodeInputOutputInfo(FHoudiniNode& InNode, TArray<FName>& Inputs,
	TArray<FName>& Outputs)
{
	if(!InNode.IsValid())
	{
		HOUDINI_LOG_ERROR(TEXT("Invalid NodeId or SessionId"));
		return false;
	}
	HAPI_NodeInfo NodeInfo;
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::GetNodeInfo(InNode.Session,InNode.NodeId,&NodeInfo),false);
	FString Name;
	for(int i=0;i<NodeInfo.inputCount;i++)
	{
		HAPI_StringHandle Handle;
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::GetNodeInputName(InNode.Session,InNode.NodeId,i,&Handle),false);
		FHoudiniEngineString::ToFString(Handle,Name);
		Inputs.Add(FName(Name));
	}
	for(int i=0;i<NodeInfo.outputCount;i++)
	{
		HAPI_StringHandle Handle;
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::GetNodeOutputName(InNode.Session,InNode.NodeId,i,&Handle),false);
		FHoudiniEngineString::ToFString(Handle,Name);
		Outputs.Add(FName(Name));
	}
	return true;
}

bool UHoudiniEngineBPExtension::QueryNodeOutputConnectNodes(FHoudiniNode& InNode, int32 OutputIndex,
	TArray<int32>& OutputNodeIds)
{
	if(!InNode.IsValid())
	{
		HOUDINI_LOG_ERROR(TEXT("Invalid NodeId or SessionId"));
		return false;
	}
	int32 ConnectCount = -1;
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::QueryNodeOutputConnectedCount(InNode.Session,InNode.NodeId,OutputIndex,false,false,&ConnectCount),false);
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::QueryNodeOutputConnectedNodes(InNode.Session,InNode.NodeId,OutputIndex,false,false,OutputNodeIds.GetData(),0,ConnectCount),false);
	return true;
}

bool UHoudiniEngineBPExtension::QueryNodeInputConnectNode(FHoudiniNode& InNode, int32 InputIndex,
	int32& InputNodeId)
{
	if(!InNode.IsValid())
	{
		HOUDINI_LOG_ERROR(TEXT("Invalid NodeId or SessionId"));
		return false;
	}
	InputNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::QueryNodeInput(InNode.Session,InNode.NodeId,InputIndex,&InputNodeId),false);
	
	return true;
}

bool UHoudiniEngineBPExtension::ConnectNodeOutputToNodeInput(FHoudiniNode& OutputNode, int32 OutputIndex,
	FHoudiniNode& InputNode, int32 InputIndex)
{
	if(!OutputNode.IsValid())
	{
		HOUDINI_LOG_ERROR(TEXT("Invalid NodeId or SessionId"));
		return false;
	}
	if(OutputNode.Session!=InputNode.Session)
		return false;
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::ConnectNodeInput(OutputNode.Session,InputNode.NodeId,InputIndex,OutputNode.NodeId,OutputIndex),false);
	return true;
}

bool UHoudiniEngineBPExtension::DisConnectNodeInput(FHoudiniNode& InNode, int32 InputIndex)
{
	if(!InNode.IsValid())
	{
		HOUDINI_LOG_ERROR(TEXT("Invalid NodeId or SessionId"));
		return false;
	}
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::DisconnectNodeInput(InNode.Session,InNode.NodeId,InputIndex),false);
	return true;
}

bool UHoudiniEngineBPExtension::SetParamValue(FHoudiniNode& InNode, const FString& ParamName, const int32 ParamTupleIndex, const int32& Value)
{
	//We should never enter this
	return false;
}

bool UHoudiniEngineBPExtension::Generic_SetParamValue(FHoudiniNode& InNode, const FString& ParamName, int32 ParamTupleIndex, const void* Value,
	const FProperty* Property)
{
	if(!InNode.IsValid())
	{
		HOUDINI_LOG_ERROR(TEXT("Invalid NodeId or SessionId"));
		return false;
	}
	const auto ParamNameANSI = TCHAR_TO_ANSI(*ParamName);
	HAPI_ParmInfo ParamInfo;
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::GetParmInfoFromName(InNode.Session,InNode.NodeId,ParamNameANSI,&ParamInfo),false);
#define TryGetCastProperty(PropType) const F##PropType##Property* PropType##Prop = CastField<F##PropType##Property>(Property)

	// ParamInfo.size;
	// FDataExchange_Base::PodStructsStorageInfo;
	// FDataExchange_Base::PropertyStorageInfo;
	// const FName& FiledName = Property->GetClass()->GetFName();
	// if(TryGetCastProperty(Struct))
	// {
	// 	const FName& StructName = StructProp->Struct->GetFName();
	// 	if(auto* StorageInfo = FDataExchange_Base::PodStructsStorageInfo.Find(StructName))
	// 	{
	// 		StorageInfo->ElementTupleCount;
	// 		StorageInfo->StorageType;
	// 	}
	// }
	
	if(TryGetCastProperty(Str))
	{
		const FString String = StrProp->GetPropertyValue(Value);
		FHoudiniApi::SetParmStringValue(InNode.Session,InNode.NodeId,TCHAR_TO_ANSI(*String),ParamInfo.id,ParamTupleIndex);
	}
	else if(TryGetCastProperty(Int))
	{
		FHoudiniApi::SetParmIntValue(InNode.Session,InNode.NodeId,ParamNameANSI,ParamTupleIndex,IntProp->GetPropertyValue(Value));
	}
	else if(TryGetCastProperty(Double))
	{
		FHoudiniApi::SetParmFloatValue(InNode.Session,InNode.NodeId,ParamNameANSI,ParamTupleIndex,float(DoubleProp->GetPropertyValue(Value)));
	}
	else if(TryGetCastProperty(Float))
	{
		FHoudiniApi::SetParmFloatValue(InNode.Session,InNode.NodeId,ParamNameANSI,ParamTupleIndex,FloatProp->GetPropertyValue(Value));
	}
	else
	{
		return false;
	}
	
	return true;
#undef TryGetCastProperty
}

DEFINE_FUNCTION(UHoudiniEngineBPExtension::execSetParamValue)
{
	P_GET_STRUCT_REF(FHoudiniNode,InNode);
	P_GET_PROPERTY(FStrProperty,ParamName);
	P_GET_PROPERTY(FIntProperty,ParamTupleIndex)
	
	Stack.StepCompiledIn<FArrayProperty>(NULL);
	FProperty* SourceProperty = Stack.MostRecentProperty;
	void* SourceValuePtr = Stack.MostRecentPropertyAddress;
	
	P_FINISH;
	if (!SourceProperty || !SourceValuePtr)
	{
		// const FBlueprintExceptionInfo ExceptionInfo(
		// 	EBlueprintExceptionType::AccessViolation,
		// 	NSLOCTEXT("HoudiniExtension","SetField_MissingInputProperty", "Failed to resolve the input parameter for SetField.")
		// );
		// FBlueprintCoreDelegates::ThrowScriptException(Stack, ExceptionInfo);
		P_NATIVE_BEGIN;
		*(bool*)RESULT_PARAM = false;
		P_NATIVE_END;
	}
	else
	{
		P_NATIVE_BEGIN;
		*(bool*)RESULT_PARAM = UHoudiniEngineBPExtension::Generic_SetParamValue(InNode,ParamName,ParamTupleIndex,SourceValuePtr,SourceProperty);
		P_NATIVE_END;
	}
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

int32 UHoudiniEngineBPExtension::CreateSOPNode(int32 InParentId, FString InNodeName, const FString& InNodeLabel, bool bWaitForCompletion)
{
	int32 OutNodeId = -1;
	if(InParentId<0)
		InNodeName = TEXT("SOP/")+InNodeName;
	FHoudiniEngineUtils::CreateNode(InParentId,InNodeName,InNodeLabel,bWaitForCompletion,&OutNodeId);
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
	Part.pointCount = Counts.Y;
	Part.faceCount = Counts.Z;
	Part.type = HAPI_PARTTYPE_MESH;
	
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
		InNode.Session, InNode.NodeId, InPartId, &Part),false);

	// Add Default P Attrib
	if(bCreateDefaultP)
	{
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = Part.pointCount;
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

int32 UHoudiniEngineBPExtension::GetNodeParent(const FHoudiniNode& InNode)
{
	if(!InNode.IsValid())
	{
		HOUDINI_LOG_ERROR(TEXT("Invalid NodeId or SessionId"));
		return false;
	}
	HAPI_NodeInfo NodeInfo;
	NodeInfo.parentId = -1;
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::GetNodeInfo(InNode.Session,InNode.NodeId,&NodeInfo),false);
	return NodeInfo.parentId;
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
	bool Result = true;
	
	FImportDataVisitor ImportDataVisitor{InNode.Session,InNode.NodeId,static_cast<HAPI_AttributeOwner>(ImportLevel),PartId};
	Result &= ExchangeData.Accept(ImportDataVisitor);
	
	const int32 Size = InStruct->GetStructureSize();
	const int32 Count = InArrayOfStruct.Num();
	uint8* Data = static_cast<uint8*>(InArrayOfStruct.GetData()) + Size*(Count-1);
	for(int i=Count-1;i>=0;i--)
	{
		FoldDataVisitor.Reset(Data);
		Result &= ExchangeData.Accept(FoldDataVisitor);
		Data-=Size;
	}
	return Result;
}

bool UHoudiniEngineBPExtension::SetArrayOfStructOnNode_BP(
	const TArray<int32>& InArrayOfStruct,
	const FHoudiniNode& InNode, int32 PartId,
	EAttributeOwner ImportLevel,bool bCommitGeo)
{
	//We should never enter this
	return false;
}

bool UHoudiniEngineBPExtension::GetArrayOfStructOnNode_BP(
	TArray<int32>& InArrayOfStruct, const FHoudiniNode& InNode,
	int32 InPartId, EAttributeOwner ImportLevel)
{
	//We should never enter this
	return false;
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
	*(bool*)RESULT_PARAM = SetArrayOfStructOnNodeInternal(*reinterpret_cast<const FScriptArray*>(ArrayAddr),InnerStruct,InNode,InPartId,ImportLevel,bCommitGeo);
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
	{
		*(bool*)RESULT_PARAM = false;
	}
	else
	{
		FScriptArrayHelper Helper{ArrayProperty,ArrayAddr};
		Helper.AddValues(Count-Helper.Num());
		*(bool*)RESULT_PARAM = GetArrayOfStructOnNodeInternal(*reinterpret_cast<FScriptArray*>(ArrayAddr),InnerStruct,InNode,InPartId,ImportLevel);
	}
	P_NATIVE_END;
}

void UHoudiniEngineBPExtension::GetStructPropertyNameArray(UScriptStruct* ScriptStruct, TArray<FName>& Properties)
{
	for(const auto* Prop : TFieldRange<FProperty>(ScriptStruct))
	{
		Properties.Add(Prop->GetFName());
	}
}

void UHoudiniEngineBPExtension::GetStructPropertyAuthorNameArray(UScriptStruct* ScriptStruct, TArray<FString>& Properties)
{
	for(const auto* Prop : TFieldRange<FProperty>(ScriptStruct))
	{
		Properties.Add(Prop->GetAuthoredName());
	}
}

bool UHoudiniEngineBPExtension::GetPropertyMetaData(const FName& PropertyName, UScriptStruct* ScriptStruct,
													TMap<FName, FString>& MetaData)
{
	if(auto Property = ScriptStruct->FindPropertyByName(PropertyName))
	{
		MetaData = *Property->GetMetaDataMap();
		return true;
	}
	return false;
}

bool UHoudiniEngineBPExtension::SetPropertyMetaData(const FName& PropertyName, UScriptStruct* ScriptStruct,
	const FName& MetaDataName, const FString& MetaDataValue)
{
	if(auto Property = ScriptStruct->FindPropertyByName(PropertyName))
	{
		Property->SetMetaData(MetaDataName,FString(MetaDataValue));
		return true;
	}
	return false;
}

PRAGMA_ENABLE_INLINING
PRAGMA_ENABLE_OPTIMIZATION