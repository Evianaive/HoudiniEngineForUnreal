// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HoudiniEngineExtensionTest.generated.h"

UCLASS()
class HOUDINIENGINE_API AHoudiniEngineExtensionTestActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AHoudiniEngineExtensionTestActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(CallInEditor,BlueprintCallable)
	void Test();
};


USTRUCT()
struct FHoudiniEngineExtensionTest
{
	GENERATED_BODY()
	UPROPERTY()
	int8 int8;
	UPROPERTY()
	int16 int16;
	UPROPERTY()
	int32 int32;
	UPROPERTY()
	int64 int64;
	
	UPROPERTY()
	uint8 uint8;
	UPROPERTY()
	uint16 uint16;
	UPROPERTY()
	uint32 uint32;
	UPROPERTY()
	uint64 uint64;

	UPROPERTY()
	float float32;
	UPROPERTY()
	double float64;
	
	UPROPERTY()
	FVector FVector;
	UPROPERTY()
	FVector2D FVector2D;

	UPROPERTY()
	FString String;
};

USTRUCT()
struct FHoudiniEngineExtensionArrayTest
{
	GENERATED_BODY()
	UPROPERTY()
	TArray<FString> StringArray;
};