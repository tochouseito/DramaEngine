#pragma once
#include <concepts>
#include <type_traits>
template<class T>
concept AllowedVector =
std::is_same_v<T, int> ||
std::is_same_v<T, unsigned int> ||
std::is_same_v<T, uint32_t> ||
std::is_same_v<T, float> ||
std::is_same_v<T, double>;
