// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/UnrealType.h"
#include "CoreMinimal.h"
#include "Containers/Array.h"

class FCustomScriptArrayHelper
{
	template <typename CallableType>
	auto WithScriptArray(CallableType&& Callable) const
	{
		if (!!(ArrayFlags & EArrayPropertyFlags::UsesMemoryImageAllocator))
		{
			return Callable(&const_cast<FCustomScriptArrayHelper*>(this)->FreezableArray);
		}
		else
		{
			return Callable(&const_cast<FCustomScriptArrayHelper*>(this)->HeapArray);
		}
	}

public:
	/**
	 *	Constructor, brings together a property and an instance of the property located in memory
	 *	@param	InProperty: the property associated with this memory
	 *	@param	InArray: pointer to raw memory that corresponds to this array. This can be NULL, and sometimes is, but in that case almost all operations will crash.
	**/
	FORCEINLINE FCustomScriptArrayHelper(const FArrayProperty* InProperty)
		: FCustomScriptArrayHelper(nullptr, InProperty->Inner, InProperty->Inner->ElementSize, InProperty->Inner->GetMinAlignment(), InProperty->ArrayFlags)
	{}
	
	FORCEINLINE FCustomScriptArrayHelper(const FProperty* InInnerProperty, EArrayPropertyFlags InArrayFlags = EArrayPropertyFlags::None)
		: FCustomScriptArrayHelper(nullptr, InInnerProperty, InInnerProperty->ElementSize, InInnerProperty->GetMinAlignment(), InArrayFlags)
	{}

	FORCEINLINE FCustomScriptArrayHelper(const UScriptStruct* InInnerStruct, EArrayPropertyFlags InArrayFlags = EArrayPropertyFlags::None)
		: FCustomScriptArrayHelper(InInnerStruct, nullptr, InInnerStruct->GetStructureSize(), InInnerStruct->GetCppStructOps()->GetAlignment(), InArrayFlags)
	{}
	
	FORCEINLINE FCustomScriptArrayHelper(const FProperty* InInnerProperty, const UScriptStruct* InInnerStruct, EArrayPropertyFlags InArrayFlags = EArrayPropertyFlags::None)
		: FCustomScriptArrayHelper(
			InInnerStruct, InInnerProperty,
			InInnerStruct?InInnerStruct->GetStructureSize():InInnerProperty->ElementSize,
			InInnerStruct?InInnerStruct->GetCppStructOps()->GetAlignment():InInnerProperty->GetMinAlignment(),
			InArrayFlags)
	{}
	/**
	 *	Index range check
	 *	@param	Index: Index to check
	 *	@return true if accessing this element is legal.
	**/
	FORCEINLINE bool IsValidIndex( int32 Index ) const
	{
		return Index >= 0 && Index < Num();
	}
	/**
	 *	Return the number of elements in the array.
	 *	@return	The number of elements in the array.
	**/
	FORCEINLINE int32 Num() const
	{
		int32 Result = WithScriptArray([](auto* Array) { return Array->Num(); });
		checkSlow(Result >= 0);
		return Result;
	}
	
	/**
	 *	Returns a uint8 pointer to an element in the array
	 *	@param	Index: index of the item to return a pointer to.
	 *	@return	Pointer to this element, or NULL if the array is empty
	**/
	FORCEINLINE uint8* GetRawPtr(int32 Index = 0)
	{
		if (!Num())
		{
			checkSlow(!Index);
			return NULL;
		}
		checkSlow(IsValidIndex(Index)); 
		return (uint8*)WithScriptArray([](auto* Array) { return Array->GetData(); }) + Index * ElementSize;
	}
	/**
	*	Empty the array, then add blank, constructed values to a given size.
	*	@param	Count: the number of items the array will have on completion.
	**/
	void EmptyAndAddValues(int32 Count)
	{ 
		check(Count>=0);
		checkSlow(Num() >= 0); 
		EmptyValues(Count);
		AddValues(Count);
	}
	/**
	*	Empty the array, then add uninitialized values to a given size.
	*	@param	Count: the number of items the array will have on completion.
	**/
	void EmptyAndAddUninitializedValues(int32 Count)
	{ 
		check(Count>=0);
		checkSlow(Num() >= 0); 
		EmptyValues(Count);
		AddUninitializedValues(Count);
	}
	/**
	*	Expand the array, if needed, so that the given index is valid
	*	@param	Index: index for the item that we want to ensure is valid
	*	@return true if expansion was necessary
	*	NOTE: This is not a count, it is an INDEX, so the final count will be at least Index+1 this matches the usage.
	**/
	bool ExpandForIndex(int32 Index)
	{ 
		check(Index>=0);
		checkSlow(Num() >= 0); 
		if (Index >= Num())
		{
			AddValues(Index - Num() + 1);
			return true;
		}
		return false;
	}
	/**
	*	Add or remove elements to set the array to a given size.
	*	@param	Count: the number of items the array will have on completion.
	**/
	void Resize(int32 Count)
	{ 
		check(Count>=0);
		int32 OldNum = Num();
		if (Count > OldNum)
		{
			AddValues(Count - OldNum);
		}
		else if (Count < OldNum)
		{
			RemoveValues(Count, OldNum - Count);
		}
	}
	/**
	*	Add blank, constructed values to the end of the array.
	*	@param	Count: the number of items to insert.
	*	@return	the index of the first newly added item.
	**/
	int32 AddValues(int32 Count)
	{ 
		const int32 OldNum = AddUninitializedValues(Count);		
		ConstructItems(OldNum, Count);
		return OldNum;
	}
	/**
	*	Add a blank, constructed values to the end of the array.
	*	@return	the index of the newly added item.
	**/
	FORCEINLINE int32 AddValue()
	{ 
		return AddValues(1);
	}
	/**
	*	Add uninitialized values to the end of the array.
	*	@param	Count: the number of items to insert.
	*	@return	the index of the first newly added item.
	**/
	int32 AddUninitializedValues(int32 Count)
	{
		check(Count>=0);
		checkSlow(Num() >= 0);
		const int32 OldNum = WithScriptArray([this, Count](auto* Array) { return Array->Add(Count, ElementSize, ElementAlignment); });
		return OldNum;
	}
	/**
	*	Add an uninitialized value to the end of the array.
	*	@return	the index of the newly added item.
	**/
	FORCEINLINE int32 AddUninitializedValue()
	{
		return AddUninitializedValues(1);
	}
	/**
	 *	Insert blank, constructed values into the array.
	 *	@param	Index: index of the first inserted item after completion
	 *	@param	Count: the number of items to insert.
	**/
	void InsertValues( int32 Index, int32 Count = 1)
	{
		check(Count>=0);
		check(Index>=0 && Index <= Num());
		WithScriptArray([this, Index, Count](auto* Array) { Array->Insert(Index, Count, ElementSize, ElementAlignment); });
		ConstructItems(Index, Count);
	}
	/**
	 *	Remove all values from the array, calling destructors, etc as appropriate.
	 *	@param Slack: used to presize the array for a subsequent add, to avoid reallocation.
	**/
	void EmptyValues(int32 Slack = 0)
	{
		checkSlow(Slack>=0);
		const int32 OldNum = Num();
		if (OldNum)
		{
			DestructItems(0, OldNum);
		}
		if (OldNum || Slack)
		{
			WithScriptArray([this, Slack](auto* Array) { Array->Empty(Slack, ElementSize, ElementAlignment); });
		}
	}
	/**
	 *	Remove values from the array, calling destructors, etc as appropriate.
	 *	@param Index: first item to remove.
	 *	@param Count: number of items to remove.
	**/
	void RemoveValues(int32 Index, int32 Count = 1)
	{
		check(Count>=0);
		check(Index>=0 && Index + Count <= Num());
		DestructItems(Index, Count);
		WithScriptArray([this, Index, Count](auto* Array) { Array->Remove(Index, Count, ElementSize, ElementAlignment); });
	}

	/**
	*	Clear values in the array. The meaning of clear is defined by the property system.
	*	@param Index: first item to clear.
	*	@param Count: number of items to clear.
	**/
	void ClearValues(int32 Index, int32 Count = 1)
	{
		check(Count>=0);
		check(Index>=0);
		ClearItems(Index, Count);
	}

	/**
	 *	Swap two elements in the array, does not call constructors and destructors
	 *	@param A index of one item to swap.
	 *	@param B index of the other item to swap.
	**/
	void SwapValues(int32 A, int32 B)
	{
		WithScriptArray([this, A, B](auto* Array) { Array->SwapMemory(A, B, ElementSize); });
	}

	/**
	 *	Move the allocation from another array and make it our own.
	 *	@note The arrays MUST be of the same type, and this function will NOT validate that!
	 *	@param InOtherArray The array to move the allocation from.
	**/
	void MoveAssign(void* InOtherArray)
	{
		checkSlow(InOtherArray);
		WithScriptArray([this, InOtherArray](auto* Array) { Array->MoveAssign(*static_cast<decltype(Array)>(InOtherArray), ElementSize, ElementAlignment); });
	}

	/**
	 *	Used by memory counting archives to accumulate the size of this array.
	 *	@param Ar archive to accumulate sizes
	**/
	void CountBytes( FArchive& Ar  ) const
	{
		WithScriptArray([this, &Ar](auto* Array) { Array->CountBytes(Ar, ElementSize); });
	}

	/**
	 * Destroys the container object - THERE SHOULD BE NO MORE USE OF THIS HELPER AFTER THIS FUNCTION IS CALLED!
	 */
	void DestroyContainer_Unsafe()
	{
		WithScriptArray([](auto* Array) { DestructItem(Array); });
	}
private:
	FCustomScriptArrayHelper(const UScriptStruct* InInnerStruct, const FProperty* InInnerProperty, int32 InElementSize, uint32 InElementAlignment, EArrayPropertyFlags InArrayFlags = EArrayPropertyFlags::None)
		: InnerStruct(InInnerStruct)
		, InnerProperty(InInnerProperty)
		, ElementSize(InElementSize)
		, ElementAlignment(InElementAlignment)
		, ArrayFlags(InArrayFlags)
		// , StorePackSize(1)
	{
		//@todo, we are casting away the const here
		
		check(ElementSize > 0);
		check(InnerProperty);
	}
	
	/**
	 *	Internal function to call into the property system to construct / initialize elements.
	 *	@param Index: first item to .
	 *	@param Count: number of items to .
	**/
	void ConstructItems(int32 Index, int32 Count)
	{
		checkSlow(Count >= 0);
		checkSlow(Index >= 0); 
		checkSlow(Index <= Num());
		checkSlow(Index + Count <= Num());
		if (Count > 0)
		{
			uint8* Dest = GetRawPtr(Index);
			if(!InnerStruct)
			{
				///////////////////////////////////////////////////////
				if (InnerProperty->PropertyFlags & CPF_ZeroConstructor)
				{
					FMemory::Memzero(Dest, Count * ElementSize);
				}
				else
				{
					for (int32 LoopIndex = 0; LoopIndex < Count; LoopIndex++, Dest += ElementSize)
					{
						InnerProperty->InitializeValue(Dest);
					}
				}
				///////////////////////////////////////////////////////
				return;
			}
			if(InnerStruct->GetCppStructOps()->GetComputedPropertyFlags() & CPF_ZeroConstructor)
			{
				FMemory::Memzero(Dest, Count * ElementSize);
			}
			else
			{
				for (int32 LoopIndex = 0; LoopIndex < Count; LoopIndex++, Dest += ElementSize)
				{
					InnerStruct->InitializeStruct(Dest);
				}					
			}
		}
	}
	/**
	 *	Internal function to call into the property system to destruct elements.
	 *	@param Index: first item to .
	 *	@param Count: number of items to .
	**/
	void DestructItems(int32 Index, int32 Count)
	{
		if(!InnerStruct)
		{
			///////////////////////////////////////////////////////
			if (!(InnerProperty->PropertyFlags & (CPF_IsPlainOldData | CPF_NoDestructor)))
			{
				checkSlow(Count >= 0);
				checkSlow(Index >= 0); 
				checkSlow(Index < Num());
				checkSlow(Index + Count <= Num());
				if (Count > 0)
				{
					uint8* Dest = GetRawPtr(Index);
					for (int32 LoopIndex = 0; LoopIndex < Count; LoopIndex++, Dest += ElementSize)
					{
						InnerProperty->DestroyValue(Dest);
					}
				}
			}
			///////////////////////////////////////////////////////
			return;
		}
		
		checkSlow(Count >= 0);
		checkSlow(Index >= 0); 
		checkSlow(Index < Num());
		checkSlow(Index + Count <= Num());
		if(!(InnerStruct->GetCppStructOps()->GetComputedPropertyFlags() & (CPF_IsPlainOldData | CPF_NoDestructor)))
		{
			uint8* Dest = GetRawPtr(Index);
			for (int32 LoopIndex = 0; LoopIndex < Count; LoopIndex++, Dest += ElementSize)
			{
				InnerStruct->DestroyStruct(Dest);
			}
		}
	}
	/**
	 *	Internal function to call into the property system to clear elements.
	 *	@param Index: first item to .
	 *	@param Count: number of items to .
	**/
	void ClearItems(int32 Index, int32 Count)
	{
		checkSlow(Count >= 0);
		checkSlow(Index >= 0); 
		checkSlow(Index < Num());
		checkSlow(Index + Count <= Num());
		if (Count > 0)
		{
			uint8* Dest = GetRawPtr(Index);
			if(!InnerStruct)
			{
				///////////////////////////////////////////////////////
				if ((InnerProperty->PropertyFlags & (CPF_ZeroConstructor | CPF_NoDestructor)) == (CPF_ZeroConstructor | CPF_NoDestructor))
				{
					FMemory::Memzero(Dest, Count * ElementSize);
				}
				else
				{
					for (int32 LoopIndex = 0; LoopIndex < Count; LoopIndex++, Dest += ElementSize)
					{
						InnerProperty->ClearValue(Dest);
					}
				}
				///////////////////////////////////////////////////////
				return;
			}
			if ((InnerStruct->GetCppStructOps()->GetComputedPropertyFlags() & (CPF_ZeroConstructor | CPF_NoDestructor)) == (CPF_ZeroConstructor | CPF_NoDestructor))
			{
				FMemory::Memzero(Dest, Count * ElementSize);
			}
			else
			{
				for (int32 LoopIndex = 0; LoopIndex < Count; LoopIndex++, Dest += ElementSize)
				{
					InnerStruct->ClearScriptStruct(Dest);
				}
			}
		}
	}
	const UScriptStruct* InnerStruct;
	const FProperty* InnerProperty;
	
	FScriptArray HeapArray;
	/*never used*/
	FFreezableScriptArray FreezableArray;
	
	int32 ElementSize;
	uint32 ElementAlignment;
	/*we construct ScriptArrayHelper from inner property directly, but ArrayFlags is a property of FArrayProperty*/
	EArrayPropertyFlags ArrayFlags;
	int32 StorePackSize;
public:
	int32 GetElementSize() const {return ElementSize;}
	const FScriptArray& GetArray(){return HeapArray;}

	void PackElement(int32 InPackSize);
	void UnPackElement(int32 InPackSize);	
	int32 GetStorePackSize() const
	{
		return StorePackSize;
	}	
};