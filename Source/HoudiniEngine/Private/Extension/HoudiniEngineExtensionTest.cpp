// Fill out your copyright notice in the Description page of Project Settings.


#include "HoudiniEngineExtensionTest.h"

#include "HoudiniEngineUtils.h"
#include "HoudiniEngineUtilsExtenstion.h"
#include "UnrealStructExpand.h"


// Sets default values
AHoudiniEngineExtensionTestActor::AHoudiniEngineExtensionTestActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	TestDatum = {
		{
			8,99,24332,44,33,77,42,1,0.5,0.6,
			{0.4,0.3,0.2},{0.2,0.3},
			TEXT("MyString")
		},
		{
			8,99,332,45,33,32,77,1,0.5,0.6,
			{0.4,0.3,0.2},{0.2,0.3},
			TEXT("MyString")
		},
		{
			8,99,2332,46,33,32,78,1,0.5,0.6,
			{0.5,0.3,0.2},{0.2,0.3},
			TEXT("MyStrfdag")
		},
		{
			8,99,2432,43,33,32,89,1,0.5,0.6,
			{0.4,0.3,0.2},{0.2,0.3},
			TEXT("MyStrfdaig")
		},
		{
			8,43,2433,48,33,32,77,1,0.5,0.6,
			{0.4,0.40,0.2},{0.2,0.3},
			TEXT("MyStrfdig")
		}};	
}

// Called when the game starts or when spawned
void AHoudiniEngineExtensionTestActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AHoudiniEngineExtensionTestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AHoudiniEngineExtensionTestActor::Test()
{
	HAPI_NodeId NodeId;
	FHoudiniEngineUtils::CreateInputNode(TEXT("test"),NodeId);

	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);
	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.vertexCount = 0;
	Part.faceCount = 0;
	Part.pointCount = TestDatum.Num();
	Part.type = HAPI_PARTTYPE_MESH;
	
	HAPI_Result ResultPartInfo = FHoudiniApi::SetPartInfo(
	FHoudiniEngine::Get().GetSession(), NodeId, 0, &Part);

	Part.id = 1;
	Part.type = HAPI_PARTTYPE_BOX;
	HAPI_Result ResultPartInfo2 = FHoudiniApi::SetPartInfo(
	FHoudiniEngine::Get().GetSession(), NodeId, 1, &Part);

	{
		// Create point attribute info for P.
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = Part.pointCount;
		AttributeInfoPoint.tupleSize = 3;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), NodeId, 0,
			HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint);

		// Set the point's position
		FVector3f ObjectPosition{4,5,6};
		TArray<float> Position;
		for (int i = 0; i<Part.pointCount;i++)
		{
			Position.Append({ObjectPosition.X,ObjectPosition.Y,ObjectPosition.Z});	
		}		

		// Now that we have raw positions, we can upload them for our attribute.
		FHoudiniEngineUtils::HapiSetAttributeFloatData(
			Position, NodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, AttributeInfoPoint);
		
		FHoudiniEngineUtils::HapiSetAttributeFloatData(
			Position, NodeId, 1, HAPI_UNREAL_ATTRIB_POSITION, AttributeInfoPoint);
	}
	const auto SessionId = FHoudiniEngine::Get().GetSession();
	{	
		FDataExchange_Struct Gather{decltype(TestDatum)::ElementType::StaticStruct()};
		
		FUnfoldDataVisitor GatherDataVisitor{nullptr};
		for (auto& Data: TestDatum)
		{
			GatherDataVisitor.Reset(reinterpret_cast<const uint8*>(&Data));
			Gather.Accept(GatherDataVisitor);
		}
		FExportDataVisitor ExportDataVisitor{SessionId,NodeId,HAPI_AttributeOwner::HAPI_ATTROWNER_POINT};
		Gather.Accept(ExportDataVisitor);

		// Commit the geo.
		FHoudiniApi::CommitGeo(SessionId, NodeId);

		
		FHoudiniEngineUtils::HapiCookNode(NodeId,nullptr,true);
		
		HAPI_AttributeInfo TempInfo;
		FHoudiniApi::AttributeInfo_Init(&TempInfo);
		FHoudiniApi::GetAttributeInfo(SessionId,NodeId,0,"P",HAPI_ATTROWNER_POINT,&TempInfo);
		
	}

	FImportDataVisitor ImportDataVisitor{SessionId,NodeId,HAPI_AttributeOwner::HAPI_ATTROWNER_POINT};
	FDataExchange_Struct Gather2{decltype(TestDatum)::ElementType::StaticStruct()};
	Gather2.Accept(ImportDataVisitor);
	
	FFoldDataVisitor FoldDataVisitor{nullptr};

	TArray<FHoudiniEngineExtensionTest> ImportDatum;
	ImportDatum.AddDefaulted(TestDatum.Num());
	for (int i=ImportDatum.Num()-1; i>=0; i--)
	{
		FoldDataVisitor.Reset(reinterpret_cast<uint8*>(&ImportDatum[i]));
		Gather2.Accept(FoldDataVisitor);
	}
}

void UHoudiniEngineTestLibrary::CheckHoudiniAssetComponent(UHoudiniAssetComponent* Component)
{
	auto Session = FHoudiniEngine::Get().GetSession();
	auto AssetId = Component->GetAssetId();
	// FHoudiniEngineUtils::HapiGetNodePath()
	TArray<int32> NodeIds = {-1};
	for (int i = 0; i < NodeIds.Num(); ++i)
	{
		FHoudiniApi::GetOutputNodeId(Session,AssetId,i,NodeIds.GetData());
	}
	UE_LOG(LogTemp,Log,TEXT("Node Id: %i"), NodeIds[0]);
	
	TArray<HAPI_GeoInfo> GeoInfos;
	int32 Count = 0;
	FHoudiniApi::GetOutputGeoCount(Session, AssetId, &Count);
	GeoInfos.SetNum(Count);
	for (int32 i = 0; i< Count; i++)
	{
		auto& GeoInfo = GeoInfos[i];  
		FHoudiniApi::GeoInfo_Init(&GeoInfo);
	}
	FHoudiniApi::GetOutputGeoInfos(Session, AssetId, &GeoInfos[0], Count);
	for (int32 i = 0; i< Count; i++)
	{
		auto& GeoInfo = GeoInfos[i];  
		FString NameString;
		FHoudiniEngineString::ToFString(GeoInfo.nameSH, NameString);
		UE_LOG(LogTemp,Log,TEXT("part count %i, Name %s"), GeoInfo.partCount,*NameString);
	}
	
	
	
	// FHoudiniApi::GetNodePath(Session,AssetId,)
	
	// FHoudiniApi::GetOutputNodeId()
	// FHoudiniApi::GetNodeFromPath()
}

