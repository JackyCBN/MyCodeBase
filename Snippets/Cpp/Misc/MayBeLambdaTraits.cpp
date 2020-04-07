#include <functional>
#include <type_traits>
#include <vector>

template <typename FN>
struct CppCouldBeLambda
{
	static constexpr bool value = std::is_class_v<FN> && !std::is_assignable_v<FN, FN>;
};

template <typename R, typename... P>
struct CppCouldBeLambda <std::function<R(P...)>>
{
	static constexpr bool value = false;
};

template<typename  _Ty, typename  Enable = void>
struct is_lambda
	:std::false_type
{

};


template<typename  _Ty>
struct is_lambda<_Ty,
	std::enable_if_t<CppCouldBeLambda<_Ty>::value, 
	std::void_t<decltype(&_Ty::operator())>>>
	:std::true_type
{	
};

template<class _Ty>
constexpr bool is_lambda_v = is_lambda<_Ty>::value;


void foo()
{
	
}

struct test
{
	int operator()()
	{
		return 1;
	}
};
int main()
{
	auto a = []() {};
	std::function<int()> f;
	std::cout << std::is_class_v<decltype(a)> << std::endl;
	std::cout << "aaa" << std::endl;
	std::cout << is_lambda_v<decltype(f)> << std::endl;
	//is_lambda<testFunctor, int> s;
	//s.foo();
}