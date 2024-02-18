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
	TArray<FHoudiniEngineExtensionTest> TestDatum{
		{
			8,88,99,24332,33,32,42,1,0.5,0.6,{0.4,0.3,0.2},{0.2,0.3},TEXT("MyString")
		},
		{
			8,8,99,332,33,32,42,1,0.5,0.6,{0.4,0.3,0.2},{0.2,0.3},TEXT("MyString")
		},
		{
			8,78,99,2332,33,32,42,1,0.5,0.6,{0.4,0.3,0.2},{0.2,0.3},TEXT("MyStrfdag")
		},
		{
			8,99,99,2432,33,32,42,1,0.5,0.6,{0.4,0.3,0.2},{0.2,0.3},TEXT("MyStrfdaig")
		},
		{
			8,21,43,2433,33,32,42,1,0.5,0.6,{0.4,0.3,0.2},{0.2,0.3},TEXT("MyStrfdig")
		}};

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
	}
	
	FDataExchange_Struct Gather{decltype(TestDatum)::ElementType::StaticStruct()};
	FUnfoldDataVisitor GatherDataVisitor{nullptr};
	for (auto& Data: TestDatum)
	{
		GatherDataVisitor.Reset(reinterpret_cast<const uint8*>(&Data));
		Gather.Accept(GatherDataVisitor);
	}
	const auto SessionId = FHoudiniEngine::Get().GetSession();
	FExportDataVisitor ExportDataVisitor{SessionId,NodeId,HAPI_AttributeOwner::HAPI_ATTROWNER_POINT};
	Gather.Accept(ExportDataVisitor);

	// Commit the geo.
	FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), NodeId);
}

