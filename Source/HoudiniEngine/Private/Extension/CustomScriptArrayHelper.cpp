#include "CustomScriptArrayHelper.h"

#include "PrivateAccess.h"

struct FScriptArrayArrayNumAccess{using Type=int32 TScriptArray<FHeapAllocator>::*;};
template struct TStaticPtrInit<FScriptArrayArrayNumAccess,&FScriptArray::ArrayNum>;;
struct FScriptArrayArrayMaxAccess{using Type=int32 TScriptArray<FHeapAllocator>::*;};
template struct TStaticPtrInit<FScriptArrayArrayMaxAccess,&FScriptArray::ArrayMax>;;

void FCustomScriptArrayHelper::PackElement(int32 InPackSize)
{
	HeapArray.*PRIVATE_ACCESS(FScriptArray,ArrayNum) /= InPackSize;
	HeapArray.*PRIVATE_ACCESS(FScriptArray,ArrayMax) /= InPackSize;
	
	StorePackSize = 1;
}

void FCustomScriptArrayHelper::UnPackElement(int32 InPackSize)
{
	StorePackSize = InPackSize;
	
	HeapArray.*PRIVATE_ACCESS(FScriptArray,ArrayNum) *= StorePackSize;
	HeapArray.*PRIVATE_ACCESS(FScriptArray,ArrayMax) *= StorePackSize;
}
