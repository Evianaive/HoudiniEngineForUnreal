// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Classes/Curves/RealCurve.h"
#include "CurveExport2Hou.generated.h"

/**
 * 
 */
struct FRuntimeFloatCurve;


USTRUCT(BlueprintType)
struct FCurveExport2HouKey
{
	GENERATED_BODY()
	UPROPERTY()
	float KeyPos;
	UPROPERTY()
	float KeyValue;
	UPROPERTY()
	TEnumAsByte<ERichCurveInterpMode> KeyType;
};

USTRUCT(BlueprintType)
struct HOUDINIENGINE_API FCurveExport2Hou
{
	GENERATED_BODY()
	FCurveExport2Hou();
	UPROPERTY()
	TArray<FCurveExport2HouKey> Keys;
	
	void FromOriginalStruct(const FRuntimeFloatCurve& UECurve);
	void ToOriginalStruct(FRuntimeFloatCurve& UECurve) const;

	bool ExportTextItem(FString& ValueStr, FCurveExport2Hou const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;
	bool ImportTextItem( const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText, FArchive* InSerializingArchive = nullptr );
	
};


template<> struct TStructOpsTypeTraits<FCurveExport2Hou> : public TStructOpsTypeTraitsBase2<FCurveExport2Hou>
{
	enum
	{
		WithExportTextItem = true,
		WithImportTextItem = true
	};
};