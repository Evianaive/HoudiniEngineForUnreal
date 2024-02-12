#pragma once

template <typename Member>
class TPrivateAccess {
public:
	inline static typename Member::Type MemberPtr;
};

template <typename Member, typename Member::Type Ptr>
struct TStaticPtrInit {
	struct FConstructPrivateAccess {
		FConstructPrivateAccess() {
			TPrivateAccess<Member>::MemberPtr = Ptr;
		}
	};
	inline static FConstructPrivateAccess TriggerConstruct;
};

#define DECLARE_PRIVATE_ACCESS(Typename,MemberName,MemberType)\
struct Typename##MemberName##Access{using Type=MemberType Typename::*;};\
template struct TStaticPtrInit<Typename##MemberName##Access,&##Typename##::##MemberName>;\

#define PRIVATE_ACCESS(Typename,MemberName)\
TPrivateAccess<Typename##MemberName##Access>::MemberPtr