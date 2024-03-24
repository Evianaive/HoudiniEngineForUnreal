// Fill out your copyright notice in the Description page of Project Settings.


#include "CurveExport2Hou.h"

#include "Curves/CurveFloat.h"

FCurveExport2Hou::FCurveExport2Hou()
	:Keys({{0,0},{1,1}})
{
}

void FCurveExport2Hou::FromOriginalStruct(const FRuntimeFloatCurve& UECurve)
{
	Keys.Reset();
	auto& OriginalCurveKeys = UECurve.GetRichCurveConst()->Keys;
	Keys.SetNum(OriginalCurveKeys.Num());
	for(int i = 0; i < OriginalCurveKeys.Num(); i++)
	{
		auto& Key = OriginalCurveKeys[i];
		Keys[i].KeyPos = Key.Time;
		Keys[i].KeyValue = Key.Value;
		Keys[i].KeyType = Key.InterpMode;
	}
}


void FCurveExport2Hou::ToOriginalStruct(FRuntimeFloatCurve& UECurve) const
{
	auto& OriginalCurveKeys = UECurve.GetRichCurve()->Keys;
	OriginalCurveKeys.Reset();
	OriginalCurveKeys.SetNum(Keys.Num());
	for(int i = 0; i < Keys.Num(); i++)
	{
		auto& Key = OriginalCurveKeys[i];
		Key.Time = Keys[i].KeyPos;
		Key.Value = Keys[i].KeyValue;
		Key.InterpMode = Keys[i].KeyType;
	}	
}

bool FCurveExport2Hou::ExportTextItem(
	FString& ValueStr, 
	FCurveExport2Hou const& DefaultValue,
	UObject* Parent,
	int32 PortFlags,
	UObject* ExportRootScope) const
{
	FString KeysString;
	for (const auto& Key : Keys)
	{
		KeysString += FString::Printf(TEXT(",\n{\"t\":%f,\n\"rgba\":[%f,%f,%f,%f],\n\"basis\":\"%s\"}"),
			Key.KeyPos,
			Key.KeyValue,Key.KeyValue,Key.KeyValue,Key.KeyValue,
			(Key.KeyType==RCIM_Constant?
				TEXT("constant"):(Key.KeyType==RCIM_Cubic?
					TEXT("monotonecubic"):TEXT("linear"))));
	}	
	if(Keys.Num()==0)
		KeysString = TEXT(",\n{\"t\":0,\n\"rgba\":[0,0,0,0],\n\"basis\":\"constant\"}");
	ValueStr = FString::Printf(
		TEXT("{\n\t\"colortype\":\"RGB\",\n\t\"points\":[%i%s]\n}"),
		Keys.Num(),*KeysString);

	return true;
}

bool FCurveExport2Hou::ImportTextItem(
	const TCHAR*& Buffer,
	int32 PortFlags,
	UObject* Parent,
	FOutputDevice* ErrorText,
	FArchive* InSerializingArchive)
{
	const FStringView View (Buffer);
	int32 SrtIndex = View.Find(TEXT("points\":["));
	if(SrtIndex==INDEX_NONE)
		return false;
	SrtIndex+=9;// Len of points":[
	const int32 EndIndex = View.Find(TEXT(","),SrtIndex);
	if(EndIndex==INDEX_NONE)
		return false;
	// View.Find(TEXT(","),SrtIndex);
	// View.RightChopInline(SrtIndex);
	// FString KeysCountString;
	// FString KeysString;
	const FString CountString{View.Mid(SrtIndex,EndIndex-SrtIndex)};
	Keys.SetNum(FCString::Atoi(*CountString));
	int32 CurSrtIndex = EndIndex;
	int32 CurEndIndex = EndIndex;
	for(int i=0; i<Keys.Num();i++)
	{
		CurSrtIndex = View.Find(TEXT("\":"),CurEndIndex)+1;
		CurEndIndex = View.Find(TEXT(","),CurSrtIndex);
		FString CurString{View.Mid(CurSrtIndex,CurEndIndex-CurSrtIndex)};
		Keys[i].KeyPos = FCString::Atof(*CurString);
		CurSrtIndex = View.Find(TEXT("\":["),CurEndIndex)+2;
		CurEndIndex = View.Find(TEXT(","),CurSrtIndex);
		CurString = View.Mid(CurSrtIndex,CurEndIndex-CurSrtIndex);
		Keys[i].KeyValue = FCString::Atof(*CurString);
		CurSrtIndex = View.Find(TEXT("\":\""),CurEndIndex)+2;
		CurEndIndex = View.Find(TEXT("\""),CurSrtIndex);
		CurString = View.Mid(CurSrtIndex,CurEndIndex-CurSrtIndex);
		if(CurString==TEXT("linear"))
			Keys[i].KeyType = ERichCurveInterpMode::RCIM_Linear;
		if(CurString==TEXT("monotonecubic"))
			Keys[i].KeyType = ERichCurveInterpMode::RCIM_Cubic;
		if(CurString==TEXT("constant"))
            Keys[i].KeyType = ERichCurveInterpMode::RCIM_Constant;
	}
	
	return true;
}
