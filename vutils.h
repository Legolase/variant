#ifndef NUMBER_TYPE_H
#define NUMBER_TYPE_H

#include "vdefines.h"
#include <array>
#include <cstddef>
#include <exception>
#include <functional>
#include <type_traits>

template <typename... Types>
struct variant;

namespace details {
template <typename... Types>
struct vstorage;
}
class bad_variant_access : std::exception {
  const char* message = "bad variant access";

public:
  bad_variant_access() noexcept = default;

  explicit bad_variant_access(const char* str) noexcept : message(str) {}

  virtual const char* what() const noexcept override {
    return message;
  }
};

namespace details {
template <size_t Index, typename... Types>
struct get_type;

template <typename First, typename... Tail>
struct get_type<0, First, Tail...> {
  using type = First;
};

template <size_t Index, typename First, typename... Tail>
struct get_type<Index, First, Tail...> {
  using type = typename get_type<Index - 1, Tail...>::type;
};

template <size_t Index>
struct get_type<Index> {
  using type = void;
};

template <size_t Index, typename... Types>
using get_type_t = typename get_type<Index, Types...>::type;

template <typename... Types>
struct pack_size : std::integral_constant<size_t, sizeof...(Types)> {};

} // namespace details

template <typename Variant>
struct variant_size;

template <typename... Types>
struct variant_size<const variant<Types...>> : variant_size<variant<Types...>> {};

template <typename... Types>
struct variant_size<volatile variant<Types...>> : variant_size<variant<Types...>> {};

template <typename... Types>
struct variant_size<const volatile variant<Types...>> : variant_size<variant<Types...>> {};

template <typename... Types>
struct variant_size<variant<Types...>> : details::pack_size<Types...> {};

template <typename... Types>
struct variant_size<details::vstorage<Types...>> : details::pack_size<Types...> {};

template <typename Variant>
inline constexpr size_t variant_size_v = variant_size<Variant>::value;

template <size_t Index, typename... Types>
struct variant_alternative;

template <size_t Index, typename... Types>
struct variant_alternative<Index, variant<Types...>> {
  using type = details::get_type_t<Index, Types...>;
};

template <size_t Index, typename... Types>
struct variant_alternative<Index, const variant<Types...>> {
  using type = const details::get_type_t<Index, Types...>;
};

template <size_t Index, typename... Types>
struct variant_alternative<Index, volatile variant<Types...>> {
  using type = volatile details::get_type_t<Index, Types...>;
};

template <size_t Index, typename... Types>
struct variant_alternative<Index, const volatile variant<Types...>> {
  using type = const volatile details::get_type_t<Index, Types...>;
};

template <size_t Index, typename Variant>
using variant_alternative_t = typename variant_alternative<Index, Variant>::type;

namespace details {

template <size_t Index, typename Type>
struct position_type {
  static constexpr size_t index = Index;
  using type = Type;
};

template <typename T, size_t Index, typename... Vars>
struct best_construct_type;

template <typename T, size_t Index, typename Var>
struct best_construct_type<T, Index, Var> {
  static position_type<Index, Var> choose(Var) requires is_valid_conversion<T, Var>;
};

template <typename T, size_t Index, typename Var, typename... Types>
struct best_construct_type<T, Index, Var, Types...> : best_construct_type<T, Index + 1, Types...> {
  using best_construct_type<T, Index + 1, Types...>::choose;

  static position_type<Index, Var> choose(Var) requires is_valid_conversion<T, Var>;
};

template <typename T, typename... Types>
using chosen_construct_type = decltype(best_construct_type<T, 0, Types...>::choose(std::declval<T>()));

template <typename T, typename... Types>
struct count_of;

template <typename T>
struct count_of<T> {
  static constexpr size_t count = 0;
};

template <typename T, typename Last>
struct count_of<T, Last> {
  static constexpr size_t count = (std::is_same_v<T, Last>) ? 1 : 0;
};

template <typename T, typename Head, typename... Types>
struct count_of<T, Head, Types...> {
  static constexpr size_t count = ((std::is_same_v<T, Head>) ? 1 : 0) + count_of<T, Types...>::count;
};

template <typename T, typename... Types>
inline constexpr size_t count_of_v = count_of<T, Types...>::count;

template <typename Searching, size_t Index, typename... Types>
struct find_first;

template <typename Searching, size_t Index>
struct find_first<Searching, Index> {
  static constexpr size_t index = -1;
};

template <typename Searching, size_t Index, typename Last>
struct find_first<Searching, Index, Last> {
  static constexpr size_t index = (std::is_same_v<Searching, Last>) ? Index : -1;
};

template <typename Searching, size_t Index, typename Head, typename... Types>
struct find_first<Searching, Index, Head, Types...> {
  static constexpr size_t index =
      (std::is_same_v<Searching, Head>) ? Index : find_first<Searching, Index + 1, Types...>::index;
};

template <typename T, typename... Types>
inline constexpr size_t find_first_v = find_first<T, 0, Types...>::index;

} // namespace details

template <typename T, typename... Types>
constexpr bool holds_alternative(variant<Types...> const& v) noexcept requires(details::count_of_v<T, Types...> == 1) {
  return details::find_first_v<T, Types...> == v.index();
}

template <size_t Index, typename... Types>
constexpr variant_alternative_t<Index, variant<Types...>>& get(variant<Types...>& v) {
  if (Index == v.index()) {
    return v.storage.get(in_place_index<Index>);
  }
  throw bad_variant_access{};
}

template <size_t Index, typename... Types>
constexpr const variant_alternative_t<Index, variant<Types...>>& get(variant<Types...> const& v) {
  if (Index == v.index()) {
    return v.storage.get(in_place_index<Index>);
  }
  throw bad_variant_access{};
}

template <size_t Index, typename... Types>
constexpr variant_alternative_t<Index, variant<Types...>>&& get(variant<Types...>&& v) {
  return std::move(get<Index>(v));
}

template <size_t Index, typename... Types>
constexpr const variant_alternative_t<Index, variant<Types...>>&& get(variant<Types...> const&& v) {
  return std::move(get<Index>(v));
}

template <typename T, typename... Types, size_t Index = details::find_first_v<T, Types...>>
constexpr T& get(variant<Types...>& v) {
  if (holds_alternative<T>(v)) {
    return v.storage.get(in_place_index<Index>);
  }
  throw bad_variant_access{};
}

template <typename T, typename... Types, size_t Index = details::find_first_v<T, Types...>>
constexpr const T& get(variant<Types...> const& v) {
  if (holds_alternative<T>(v)) {
    return v.storage.get(in_place_index<Index>);
  }
  throw bad_variant_access{};
}

template <typename T, typename... Types, size_t Index = details::find_first_v<T, Types...>>
constexpr variant_alternative_t<Index, variant<Types...>>&& get(variant<Types...>&& v) {
  return std::move(get<Index>(v));
}

template <typename T, typename... Types, size_t Index = details::find_first_v<T, Types...>>
constexpr const variant_alternative_t<Index, variant<Types...>>&& get(variant<Types...> const&& v) {
  return std::move(get<Index>(v));
}

template <size_t Index, typename... Types>
constexpr std::add_pointer_t<variant_alternative_t<Index, variant<Types...>>> get_if(variant<Types...>* v) noexcept {
  return (v != nullptr && Index == v->index()) ? std::addressof(get<Index>(*v)) : nullptr;
}

template <size_t Index, typename... Types>
constexpr std::add_pointer_t<const variant_alternative_t<Index, variant<Types...>>>
get_if(const variant<Types...>* v) noexcept {
  return (v != nullptr && Index == v->index()) ? std::addressof(get<Index>(*v)) : nullptr;
}

template <typename T, typename... Types, size_t Index = details::find_first_v<T, Types...>>
constexpr std::add_pointer_t<T> get_if(variant<Types...>* v) noexcept {
  return get_if<Index>(v);
}

template <typename T, typename... Types, size_t Index = details::find_first_v<T, Types...>>
constexpr std::add_pointer_t<const T> get_if(const variant<Types...>* v) noexcept {
  return get_if<Index>(v);
}

namespace details {

template <typename... Types>
constexpr std::array<std::common_type_t<Types...>, sizeof...(Types)> array_ctor(Types&&... args) noexcept {
  return {std::forward<Types>(args)...};
}

template <typename T>
constexpr T& in_impl(T& m) noexcept {
  return m;
}

template <typename T, typename... Vars>
constexpr auto& in_impl(T& m, size_t index, Vars... indexes) noexcept {
  return in_impl(m[index], indexes...);
}

template <typename T, typename... Types, size_t... Vars>
constexpr auto create_func_matrix_base(std::index_sequence<Vars...>) noexcept {
  return [](T func, Types... tp) { return std::invoke(std::forward<T>(func), get<Vars>(std::forward<Types>(tp))...); };
}

template <typename T, typename... Types, size_t... Vars1, size_t... Vars2, typename... Vars3>
constexpr auto create_func_matrix_base(std::index_sequence<Vars1...>, std::index_sequence<Vars2...>,
                                       Vars3... tail) noexcept {
  return array_ctor(create_func_matrix_base<T, Types...>(std::index_sequence<Vars1..., Vars2>(), tail...)...);
};

template <typename T, size_t... Vars>
constexpr auto create_func_matrix_base_in(std::index_sequence<Vars...>) noexcept {
  return [](T func) { return std::invoke(std::forward<T>(func), (std::integral_constant<size_t, Vars>())...); };
}

template <typename T, size_t... Vars1, size_t... Vars2, typename... Vars3>
constexpr auto create_func_matrix_base_in(std::index_sequence<Vars1...>, std::index_sequence<Vars2...>,
                                          Vars3... tail) noexcept {
  return array_ctor(create_func_matrix_base_in<T>(std::index_sequence<Vars1..., Vars2>(), tail...)...);
};

template <typename T, typename... Types>
constexpr auto create_matrix() noexcept {
  return create_func_matrix_base<T, Types...>(
      std::index_sequence<>(), std::make_index_sequence<variant_size_v<std::remove_reference_t<Types>>>()...);
}

template <typename T, typename... Types>
constexpr auto create_matrix_by_index() noexcept {
  return create_func_matrix_base_in<T>(std::index_sequence<>(),
                                       std::make_index_sequence<variant_size_v<std::remove_reference_t<Types>>>()...);
}

template <typename T, typename... Types>
inline constexpr auto matrix_by_index_v = create_matrix_by_index<T, Types...>();

template <typename T, typename... Types>
constexpr decltype(auto) visit_by_index(T&& visitor, Types&&... vars) {
  return in_impl(matrix_by_index_v<T&&, Types&&...>, vars.index()...)(std::forward<T>(visitor));
}

template <typename T, typename... Types>
inline constexpr auto matrix_v = create_matrix<T, Types...>();
} // namespace details

template <typename T, typename... Types>
constexpr decltype(auto) visit(T&& visitor, Types&&... vars) {
  if ((vars.valueless_by_exception() || ...)) {
    throw bad_variant_access();
  }
  return in_impl(details::matrix_v<T&&, Types&&...>, vars.index()...)(std::forward<T>(visitor),
                                                                      std::forward<Types>(vars)...);
}

#endif
