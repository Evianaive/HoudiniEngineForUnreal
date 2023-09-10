// Copyright Evianaive, Inc. All Rights Reserved.

#pragma once
#include "Delegates/IntegerSequence.h"

template<template<int> typename TemplateClass,typename TIntSequence>
struct TJumpTable;

template<template<int> typename TemplateClass,int... Is>
struct TJumpTable<TemplateClass,TIntegerSequence<int, Is...>>
{
	using FuncPtrType = decltype(&TemplateClass<0>::Dispatch);
	FuncPtrType Func[sizeof...(Is)] = {&TemplateClass<Is>::Dispatch...};

	FORCEINLINE FuncPtrType& operator[](int i)
	{
		return Func[i];
	};
	FORCEINLINE const FuncPtrType& operator[](int i) const
	{
		return Func[i];
	};
};
