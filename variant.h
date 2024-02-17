#ifndef VARIANT_H
#define VARIANT_H

#include "vdefines.h"
#include "vutils.h"

inline constexpr size_t variant_npos = -1;

namespace details {
struct undefined_t {};

inline constexpr undefined_t undefined;

template <template <typename...> typename wrapper, bool trivially_destructible, typename... Types>
struct variadic_union_base {
  constexpr variadic_union_base() = default;

  constexpr variadic_union_base(undefined_t) noexcept {}
};

template <template <typename...> typename wrapper, bool trivially_destructible, typename Head, typename... Tail>
struct variadic_union_base<wrapper, trivially_destructible, Head, Tail...> {
  constexpr variadic_union_base() : head() {}

  constexpr variadic_union_base(undefined_t u) noexcept : tail(u) {}

  template <typename... Args>
  constexpr variadic_union_base(in_place_index_t<0>, Args&&... args) : head(std::forward<Args>(args)...) {}

  template <typename... Args, size_t Index>
  constexpr variadic_union_base(in_place_index_t<Index>, Args&&... args)
      : tail(in_place_index<Index - 1>, std::forward<Args>(args)...) {}

  template <size_t Index>
  constexpr void reset(in_place_index_t<Index>) {}

  constexpr ~variadic_union_base() = default;

  union {
    Head head;
    wrapper<Tail...> tail;
  };
};

template <template <typename...> typename wrapper, typename Head, typename... Tail>
struct variadic_union_base<wrapper, false, Head, Tail...> {
  constexpr variadic_union_base() : head() {}

  constexpr variadic_union_base(undefined_t u) noexcept : tail(u) {}

  template <typename... Args>
  constexpr variadic_union_base(in_place_index_t<0>, Args&&... args) : head(std::forward<Args>(args)...) {}

  template <typename... Args, size_t Index>
  constexpr variadic_union_base(in_place_index_t<Index>, Args&&... args)
      : tail(in_place_index<Index - 1>, std::forward<Args>(args)...) {}

  template <size_t Index>
  constexpr void reset(in_place_index_t<Index>) {
    if constexpr (Index > 0) {
      tail.reset(in_place_index<Index - 1>);
    } else {
      head.~Head();
    }
  }

  constexpr ~variadic_union_base() {}

  union {
    Head head;
    wrapper<Tail...> tail;
  };
};

template <typename... Types>
struct variadic_union : variadic_union_base<variadic_union, is_trivially_destructible<Types...>, Types...> {
  constexpr variadic_union() = default;

  constexpr variadic_union(undefined_t) noexcept {}
};

template <typename Head, typename... Types>
struct variadic_union<Head, Types...>
    : variadic_union_base<variadic_union, is_trivially_destructible<Head, Types...>, Head, Types...> {
  using base = variadic_union_base<variadic_union, is_trivially_destructible<Head, Types...>, Head, Types...>;
  using base::head;
  using base::tail;

  constexpr variadic_union() = default;

  constexpr variadic_union(undefined_t) : base(undefined) {}

  template <typename... Args, size_t Index>
  constexpr variadic_union(in_place_index_t<Index> in, Args&&... args) : base(in, std::forward<Args>(args)...) {}

  template <typename... Args, size_t Index>
  constexpr auto& set(in_place_index_t<Index>, Args&&... args) {
    if constexpr (Index > 0) {
      return tail.set(in_place_index<Index - 1>, std::forward<Args>(args)...);
    } else {
      std::construct_at(std::addressof(head), std::forward<Args>(args)...);
      return head;
    }
  }

  template <size_t Index>
  constexpr auto& get(in_place_index_t<Index>) noexcept {
    if constexpr (Index > 0) {
      return tail.get(in_place_index<Index - 1>);
    } else {
      return head;
    }
  }

  template <size_t Index>
  constexpr const auto& get(in_place_index_t<Index>) const noexcept {
    if constexpr (Index > 0) {
      return tail.get(in_place_index<Index - 1>);
    } else {
      return head;
    }
  }

  constexpr ~variadic_union() = default;
};

template <bool trivially_destructible, typename... Types>
struct vstorage_base {
  constexpr vstorage_base() : index_(0) {}

  constexpr vstorage_base(undefined_t u) noexcept : index_(variant_npos), storage(u) {}

  template <typename... Args, size_t Index>
  constexpr vstorage_base(in_place_index_t<Index> in, Args&&... args)
      : index_(Index), storage(in, std::forward<Args>(args)...) {}

  constexpr void reset() {}

  //        constexpr size_t index() {
  //            return index_;
  //        }

  constexpr ~vstorage_base() = default;

protected:
  size_t index_ = variant_npos;
  variadic_union<Types...> storage;
};

template <typename... Types>
struct vstorage_base<false, Types...> {
  constexpr vstorage_base() : index_(0) {}

  constexpr vstorage_base(undefined_t u) noexcept : index_(variant_npos), storage(u) {}

  template <typename... Args, size_t Index>
  constexpr vstorage_base(in_place_index_t<Index>, Args&&... args)
      : index_(Index), storage(in_place_index<Index>, std::forward<Args>(args)...) {}

  constexpr void reset() {
    if (index_ == variant_npos) {
      return;
    }
    visit_by_index([this](auto index) { (*this).storage.reset(in_place_index<index>); },
                   *static_cast<vstorage<Types...>*>(this));
  }

  constexpr ~vstorage_base() {
    reset();
  }

protected:
  size_t index_ = variant_npos;
  variadic_union<Types...> storage;
};

template <typename... Types>
struct vstorage : vstorage_base<is_trivially_destructible<Types...>, Types...> {
  using base = vstorage_base<is_trivially_destructible<Types...>, Types...>;
  using base::index_;
  using base::reset;
  using base::storage;

  constexpr vstorage() = default;

  constexpr vstorage(undefined_t u) : base(u) {}

  template <typename... Args, size_t Index>
  constexpr vstorage(in_place_index_t<Index> in, Args&&... args) : base(in, std::forward<Args>(args)...) {}

  template <typename... Args, size_t Index>
  constexpr decltype(auto) set(in_place_index_t<Index>, Args&&... args) {
    auto& result = storage.set(in_place_index<Index>, std::forward<Args>(args)...);
    index_ = Index;
    return result;
  }

  constexpr size_t index() const {
    return index_;
  }
};

template <typename... Types>
using vstorage_t = vstorage<Types...>;

} // namespace details

template <typename... Types>
struct variant : private details::vstorage_t<Types...> {
  template <typename T, typename... Types_, size_t index>
  friend constexpr const T& get(const variant<Types_...>& v);

  template <typename T, typename... Types_, size_t index>
  friend constexpr T& get(variant<Types_...>& v);

  template <typename... Types_>
  friend struct details::vstorage;

  template <size_t Index, typename... Types_>
  friend constexpr variant_alternative_t<Index, variant<Types_...>>& get(variant<Types_...>& v);

  template <size_t Index, typename... Types_>
  friend constexpr const variant_alternative_t<Index, variant<Types_...>>& get(const variant<Types_...>& v);

  using base = details::vstorage_t<Types...>;
  using default_t = details::get_type_t<0, Types...>;
  using parent_t = details::vstorage_t<Types...>;

  constexpr variant() noexcept(
      std::is_nothrow_default_constructible_v<default_t>) requires std::is_default_constructible_v<default_t> = default;

  constexpr variant() noexcept(std::is_nothrow_default_constructible_v<default_t>) = delete;

  template <typename From, typename To_Info = details::chosen_construct_type<From, Types...>>
  constexpr variant(From&& f) noexcept(std::is_nothrow_constructible_v<From, typename To_Info::type>)
      requires details::is_convertible_to<From, typename To_Info::type, variant, Types...>
      : parent_t(in_place_index<To_Info::index>, std::forward<From>(f)) {}

  template <typename From, typename To_Info = details::chosen_construct_type<From, Types...>>
  constexpr variant& operator=(From&& f) noexcept(std::is_nothrow_assignable_v<typename To_Info::type&, From>&&
                                                      std::is_nothrow_constructible_v<typename To_Info::type, From>)
      requires details::is_convertible_to<From, typename To_Info::type, variant, Types...> {
    if (To_Info::index == this->index()) {
      get<To_Info::index>(*this) = std::forward<From>(f);
      return *this;
    }

    if (std::is_nothrow_move_constructible_v<typename To_Info::type> &&
        !std::is_nothrow_constructible_v<typename To_Info::type, From>) {
      emplace<To_Info::index>(typename To_Info::type(std::forward<From>(f)));
    } else {
      emplace<To_Info::index>(std::forward<From>(f));
    }
    return *this;
  }

  constexpr variant(const variant& other) = delete;

  constexpr variant(const variant& other) requires(details::is_copy_constructible<Types...>)
      : parent_t(details::undefined) {
    if (!other.valueless_by_exception()) {
      details::visit_by_index([&other, this](auto index) { this->emplace<index>(get<index>(other)); }, other);
    }
  }

  constexpr variant(const variant&) requires(details::is_trivially_copy_constructible<Types...>) = default;

  constexpr variant& operator=(const variant& ind) = delete;

  constexpr variant& operator=(const variant& other) requires(details::is_copy_assignable<Types...>) {
    if (other.valueless_by_exception()) {
      if (!valueless_by_exception()) {
        make_valueless();
      } else {
        return *this;
      }
    }
    details::visit_by_index(
        [&other, this](auto ind) {
          if (ind != index()) {
            if (!std::is_nothrow_copy_constructible_v<details::get_type_t<ind, Types...>> &&
                std::is_nothrow_move_constructible_v<details::get_type_t<ind, Types...>>) {
              (*this) = variant(other);
            } else {
              emplace<ind>(get<ind>(other));
            }
          } else {
            get<ind>(*this) = get<ind>(other);
          }
        },
        other);
    return *this;
  }

  constexpr variant& operator=(const variant& ind) requires(details::is_trivially_copy_assignable<Types...>) = default;

  constexpr variant(variant&& other) = delete;

  constexpr variant(variant&& other) noexcept((details::is_nothrow_move_constructible<Types...>))
      requires(details::is_move_constructible<Types...>)
      : parent_t(details::undefined) {
    if (!other.valueless_by_exception()) {
      details::visit_by_index(
          [&other, this](auto index) { (*this).template emplace<index>(get<index>(std::move(other))); }, other);
    }
  }

  constexpr variant(variant&& other) noexcept((details::is_nothrow_move_constructible<Types...>))
      requires(details::is_trivially_move_constructible<Types...>) = default;

  constexpr variant& operator=(variant&& ind) = delete;

  constexpr variant& operator=(variant&& other) noexcept(
      details::is_nothrow_move_constructible<Types...>&& details::is_nothrow_move_assignable<Types...>)
      requires(details::is_move_assignable<Types...>) {
    if (other.valueless_by_exception()) {
      if (!valueless_by_exception()) {
        make_valueless();
      }
    } else {
      details::visit_by_index(
          [&other, this](auto ind) {
            if (ind != index()) {
              emplace<ind>(get<ind>(std::move(other)));
            } else {
              get<ind>(*this) = get<ind>(std::move(other));
            }
          },
          other);
    }
    return *this;
  }

  constexpr variant& operator=(variant&& ind) noexcept(
      details::is_nothrow_move_constructible<Types...>&& details::is_nothrow_move_assignable<Types...>)
      requires(details::is_trivially_move_assignable<Types...>) = default;

  template <typename T, size_t Index = details::find_first_v<T, Types...>, typename... Args>
  constexpr explicit variant(in_place_type_t<T>, Args&&... args)
      requires((details::count_of_v<T, Types...> == 1) && std::is_constructible_v<T, Args...>)
      : parent_t(in_place_index<Index>, std::forward<Args>(args)...) {}

  template <size_t Index, typename Type_by_Index = details::get_type_t<Index, Types...>, typename... Args>
  constexpr explicit variant(in_place_index_t<Index>, Args&&... args)
      requires(std::is_constructible_v<Type_by_Index, Args...> && !std::is_same_v<Type_by_Index, void>)
      : parent_t(in_place_index<Index>, std::forward<Args>(args)...) {}

  template <size_t Index, typename Type_by_Index = details::get_type_t<Index, Types...>, typename... Args>
  constexpr Type_by_Index& emplace(Args&&... args) requires(std::is_constructible_v<Type_by_Index, Args...>) {
    static_assert(Index < sizeof...(Types));

    make_valueless();
    return parent_t::set(in_place_index<Index>, std::forward<Args>(args)...);
  }

  template <typename T, typename... Args, size_t Index = details::find_first_v<T, Types...>>
  constexpr T& emplace(Args&&... args)
      requires((details::count_of_v<T, Types...> == 1) && std::is_constructible_v<T, Args...>) {
    return emplace<Index, T>(std::forward<Args>(args)...);
  }

  constexpr void swap(variant& other) noexcept(
      details::is_nothrow_move_constructible<Types...>&& details::is_nothrow_swappable<Types...>) {
    if (valueless_by_exception() && other.valueless_by_exception()) {
      return;
    }
    if (valueless_by_exception()) {
      details::visit_by_index([&other, this](auto index) { this->emplace<index>(get<index>(std::move(other))); },
                              other);
      other.make_valueless();
      return;
    }
    if (other.valueless_by_exception()) {
      details::visit_by_index(
          [&other, this](auto index) { other.set(in_place_index<index()>, get<index()>(std::move(*this))); }, *this);
      this->make_valueless();
      return;
    }
    if (index() != other.index()) {
      using std::swap;
      swap(other, *this);
      return;
    }
    details::visit_by_index(
        [&other, this](auto index) {
          using std::swap;
          swap(get<index>(*this), get<index>(other));
          swap(this->index_, other.index_);
        },
        other);
  }

  constexpr bool valueless_by_exception() const noexcept {
    return index() == variant_npos;
  }

  constexpr size_t index() const noexcept {
    return base::index();
  }

  constexpr void make_valueless() noexcept {
    if (this->index_ != variant_npos) {
      this->reset();
      this->index_ = variant_npos;
    }
  }

  constexpr ~variant() = default;
};

template <typename... Types>
constexpr bool operator==(variant<Types...> const& a, variant<Types...> const& b) noexcept {
  if (a.index() != b.index()) {
    return false;
  }
  if (a.valueless_by_exception()) {
    return true;
  }
  return details::visit_by_index([&](auto index) { return get<index>(a) == get<index>(b); }, b);
}

template <typename... Types>
constexpr bool operator!=(variant<Types...> const& a, variant<Types...> const& b) noexcept {
  return !(a == b);
}

template <typename... Types>
constexpr bool operator<(variant<Types...> const& a, variant<Types...> const& b) noexcept {
  if (b.valueless_by_exception()) {
    return false;
  }
  if (a.valueless_by_exception()) {
    return true;
  }
  if (a.index() < b.index()) {
    return true;
  }
  if (a.index() > b.index()) {
    return false;
  }
  return details::visit_by_index([&](auto index) { return get<index>(a) < get<index>(b); }, b);
}

template <typename... Types>
constexpr bool operator>(variant<Types...> const& a, variant<Types...> const& b) noexcept {
  if (a.valueless_by_exception()) {
    return false;
  }
  if (b.valueless_by_exception()) {
    return true;
  }
  if (a.index() > b.index()) {
    return true;
  }
  if (a.index() < b.index()) {
    return false;
  }
  return details::visit_by_index([&](auto index) { return get<index>(a) > get<index>(b); }, b);
}

template <typename... Types>
constexpr bool operator<=(variant<Types...> const& a, variant<Types...> const& b) noexcept {
  if (a.valueless_by_exception()) {
    return true;
  }
  if (b.valueless_by_exception()) {
    return false;
  }
  if (a.index() < b.index()) {
    return true;
  }
  if (a.index() > b.index()) {
    return false;
  }
  return details::visit_by_index([&](auto index) { return get<index>(a) <= get<index>(b); }, b);
}

template <typename... Types>
constexpr bool operator>=(variant<Types...> const& a, variant<Types...> const& b) noexcept {
  if (b.valueless_by_exception()) {
    return true;
  }
  if (a.valueless_by_exception()) {
    return false;
  }
  if (a.index() > b.index()) {
    return true;
  }
  if (a.index() < b.index()) {
    return false;
  }
  return details::visit_by_index([&](auto index) { return get<index>(a) >= get<index>(b); }, b);
}

#endif
