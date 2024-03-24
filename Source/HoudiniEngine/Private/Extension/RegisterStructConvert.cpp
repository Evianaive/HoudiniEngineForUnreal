
#include "RegisterStructConvert.h"
#include "CurveExport2Hou.h"
#include "UnrealStructExpand.h"
#include "CoreUObject/Public/UObject/Class.h"
#include "Curves/CurveFloat.h"

template<typename TTransform, typename TMatrix, template <typename> typename TStructure>
void RegisterTransform2Matrix()
{
	FStructConvertSpecialization::RegisterConvertFunc(
		TStructure<TTransform>::Get(),
		TStructure<TMatrix>::Get(),
		[](const uint8* Transform, uint8* Matrix)
		{
			*reinterpret_cast<TMatrix*>(Matrix) = reinterpret_cast<const TTransform*>(Transform)->ToMatrixWithScale();
		},
		[](const uint8* Matrix, uint8* Transform)
		{
			*reinterpret_cast<TTransform*>(Transform) = TTransform(*reinterpret_cast<const TMatrix*>(Matrix));
		});	
}

FRegisterStructConvert::FRegisterStructConvert()
{
}

void FRegisterStructConvert::RegisterConvert()
{
	RegisterTransform2Matrix<FTransform,FMatrix,TBaseStructure>();
	RegisterTransform2Matrix<FTransform3d,FMatrix44d,TVariantStructure>();
	RegisterTransform2Matrix<FTransform3f,FMatrix44f,TVariantStructure>();
	
	FStructConvertSpecialization::RegisterConvertFunc(
		FRuntimeFloatCurve::StaticStruct(),
		FCurveExport2Hou::StaticStruct(),
		[](const uint8* FloatCurve, uint8* Export2Hou)
		{
			reinterpret_cast<FCurveExport2Hou*>(Export2Hou)->FromOriginalStruct(
				*reinterpret_cast<const FRuntimeFloatCurve*>(FloatCurve));		
		},
		[](const uint8* Export2Hou, uint8* FloatCurve)
		{
			reinterpret_cast<const FCurveExport2Hou*>(Export2Hou)->ToOriginalStruct(
				*reinterpret_cast<FRuntimeFloatCurve*>(FloatCurve));
		},true);	
}
