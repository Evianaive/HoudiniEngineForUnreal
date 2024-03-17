
#include "RegisterStructConvert.h"
#include "UnrealStructExpand.h"
#include "CoreUObject/Public/UObject/Class.h"

template<typename TTransform, typename TMatrix, template <typename> typename TStructure>
void RegisterTransform2Matrix()
{
	FStructConvertSpecialization::RegisterConvertFunc(
		TStructure<TTransform>::Get(),
		TStructure<TMatrix>::Get(),
		[](const uint8* Transform, uint8* Matrix)
		{
			*reinterpret_cast<TMatrix*>(Matrix) = reinterpret_cast<const TTransform*>(Transform)->ToMatrixWithScale();
			FVector A = reinterpret_cast<FMatrix*>(Matrix)->GetOrigin();
			FString Test = A.ToString();
			
		},
		[](const uint8* Matrix, uint8* Transform)
		{
			*reinterpret_cast<TTransform*>(Transform) = TTransform(*reinterpret_cast<const TMatrix*>(Matrix));
		});	
}

FRegisterStructConvert::FRegisterStructConvert()
{
	RegisterTransform2Matrix<FTransform,FMatrix,TBaseStructure>();
	RegisterTransform2Matrix<FTransform3d,FMatrix44d,TVariantStructure>();
	RegisterTransform2Matrix<FTransform3f,FMatrix44f,TVariantStructure>();
}

FRegisterStructConvert FRegisterStructConvert::AutoRegister{};