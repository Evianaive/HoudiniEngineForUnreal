// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HoudiniEngineExtensionTest.generated.h"

class UHoudiniAssetComponent;

USTRUCT(BlueprintType)
struct FHoudiniEngineExtensionTest
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere)
	int8 int8{0};
	UPROPERTY(EditAnywhere)
	int16 int16{0};
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	int32 int32{0};
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	int64 int64{0};
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	uint8 uint8{0};
	UPROPERTY(EditAnywhere)
	uint16 uint16{0};
	UPROPERTY(EditAnywhere)
	uint32 uint32{0};
	UPROPERTY(EditAnywhere)
	uint64 uint64{0};

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float float32{0.f};
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	double float64{0.f};
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector FVector{};
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector2D FVector2D{};

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FString String{};
};

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

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TArray<FHoudiniEngineExtensionTest> TestDatum;
};

USTRUCT()
struct FHoudiniEngineExtensionEasyTest
{
	GENERATED_BODY()
	UPROPERTY()
	int32 int32;
};

USTRUCT()
struct FHoudiniEngineExtensionArrayTest
{
	GENERATED_BODY()
	UPROPERTY()
	TArray<FString> StringArray;
};

UCLASS()
class UHoudiniEngineTestLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	static void CheckHoudiniAssetComponent(UHoudiniAssetComponent* Component);
	
};
