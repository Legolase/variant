#ifndef VARIANT_DEFINES
#define VARIANT_DEFINES

#include <cstddef>
#include <memory>
#include <type_traits>

struct in_place_t {
  explicit in_place_t() = default;
};

inline constexpr in_place_t in_place;

template <size_t N>
struct in_place_index_t {
  explicit in_place_index_t() = default;
};

template <size_t N>
inline constexpr in_place_index_t<N> in_place_index;

template <typename T>
struct in_place_type_t {
  explicit in_place_type_t() = default;
};

template <typename T>
inline constexpr in_place_type_t<T> in_place_type;

namespace details {
template <typename T>
struct is_in_place_type_t {
  static constexpr bool value = false;
};

template <typename T>
struct is_in_place_type_t<in_place_type_t<T>> {
  static constexpr bool value = true;
};

template <size_t N>
struct is_in_place_type_t<in_place_index_t<N>> {
  static constexpr bool value = true;
};

template <typename T>
inline constexpr bool is_in_place_type_v = is_in_place_type_t<T>::value;

template <typename... Types>
concept is_trivially_destructible = (std::is_trivially_destructible_v<Types> && ...);

template <typename... Types>
concept is_copy_constructible = (std::is_copy_constructible_v<Types> && ...);

template <typename... Types>
concept is_move_constructible = (std::is_move_constructible_v<Types> && ...);

template <typename... Types>
concept is_copy_assignable = is_copy_constructible<Types...> && (std::is_copy_assignable_v<Types> && ...);

template <typename... Types>
concept is_move_assignable = is_move_constructible<Types...> && (std::is_move_assignable_v<Types> && ...);

template <typename... Types>
concept is_trivially_copy_constructible =
    (is_copy_constructible<Types...> && (std::is_trivially_copy_constructible_v<Types> && ...));

template <typename... Types>
concept is_trivially_move_constructible =
    (is_move_constructible<Types...> && (std::is_trivially_move_constructible_v<Types> && ...));

template <typename... Types>
concept is_trivially_copy_assignable = (is_copy_assignable<Types...> && is_trivially_copy_constructible<Types...> &&
                                        (std::is_trivially_copy_assignable_v<Types> && ...));

template <typename... Types>
concept is_trivially_move_assignable = (is_move_assignable<Types...> && is_trivially_move_constructible<Types...> &&
                                        (std::is_trivially_move_assignable_v<Types> && ...));

template <typename... Types>
concept is_nothrow_move_constructible = (std::is_nothrow_move_constructible_v<Types> && ...);

template <typename... Types>
concept is_nothrow_move_assignable = (std::is_nothrow_move_assignable_v<Types> && ...);

template <typename... Types>
concept is_nothrow_swappable = (std::is_nothrow_swappable_v<Types> && ...);

template <typename From, typename To>
concept is_valid_conversion = requires(From && t) {
  To{std::forward<From>(t)};
};

template <typename From, typename To, typename Variant, typename... Types>
concept is_convertible_to =
    (sizeof...(Types) > 0) && (!std::is_same_v<From, Variant>)&&(std::is_constructible_v<To, From>);

} // namespace details

#endif
