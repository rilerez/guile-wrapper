#pragma once

#include <libguile.h>
#include <complex>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>

#define FN(...) [&](auto _) { return __VA_ARGS__; }

namespace guile {
using namespace std;
struct scm {
  SCM obj;
  void protect() { scm_gc_protect_object(obj); }

  scm(SCM obj) : obj{obj} {}
  operator SCM() { return obj; }

  template <class... arg_t>
  scm operator()(arg_t&&... arg) {
    return scm_call(obj, forward<arg_t>(arg)..., SCM_UNDEFINED);
  }

  optional<scm> toOpt() {  // is this the correct null value?
    if(*this == SCM_UNSPECIFIED) return nullopt;
    return optional{*this};
  }

  // basic scm_to_ functions
#define DEF_CAST_TO(type, fnname)                                              \
  operator type() { return fnname(obj); }

  DEF_CAST_TO(char, scm_to_char)
  DEF_CAST_TO(double, scm_to_double)
  DEF_CAST_TO(signed char, scm_to_schar)
  DEF_CAST_TO(unsigned char, scm_to_uchar)
  DEF_CAST_TO(short, scm_to_short)
  DEF_CAST_TO(unsigned short, scm_to_ushort)
  DEF_CAST_TO(int, scm_to_int)
  DEF_CAST_TO(unsigned int, scm_to_uint)
  DEF_CAST_TO(long, scm_to_long)
  DEF_CAST_TO(unsigned long, scm_to_ulong)
  DEF_CAST_TO(long long, scm_to_long_long)
  DEF_CAST_TO(unsigned long long, scm_to_ulong_long)
  DEF_CAST_TO(complex<double>,
              FN(complex{scm_c_real_part(_), scm_c_imag_part(_)}))
#undef DEF_CAST_TO

  // constructors (scm_from_)
#define DEF_CONSTRUCT_FROM(fnname, type)                                       \
  scm(type var) { obj = fnname(var); }

  DEF_CONSTRUCT_FROM(scm_from_char, char)
  DEF_CONSTRUCT_FROM(scm_from_schar, signed char)
  DEF_CONSTRUCT_FROM(scm_from_uchar, unsigned char)
  DEF_CONSTRUCT_FROM(scm_from_short, short)
  DEF_CONSTRUCT_FROM(scm_from_ushort, unsigned short)
  DEF_CONSTRUCT_FROM(scm_from_int, int)
  DEF_CONSTRUCT_FROM(scm_from_uint, unsigned int)
  DEF_CONSTRUCT_FROM(scm_from_long, long)
  DEF_CONSTRUCT_FROM(scm_from_ulong, unsigned long)
  DEF_CONSTRUCT_FROM(scm_from_long_long, long long)
  DEF_CONSTRUCT_FROM(scm_from_ulong_long, unsigned long long)
  DEF_CONSTRUCT_FROM(scm_from_double, double)
  DEF_CONSTRUCT_FROM(scm_from_bool, bool)
#undef DEF_CONSTRUCT_FROM
};

// do i want to do this with adl somehow? do i want to specify std::hash?
template <class Key>
struct hash;
template <>
struct hash<scm> {
  static constexpr auto size = sizeof(void*) * 8;
  size_t operator()(scm x) { return scm{scm_hash(x, scm{size})}; }
};

// arithmetic
#define MAKE_BIN_OP(csym, scm_name)                                            \
  auto operator csym(scm x, scm y) { return scm{scm_name(x.obj, y.obj)}; }

MAKE_BIN_OP(+, scm_sum)
MAKE_BIN_OP(-, scm_difference)
MAKE_BIN_OP(*, scm_product)
MAKE_BIN_OP(/, scm_divide)
MAKE_BIN_OP(&, scm_logand)
MAKE_BIN_OP(|, scm_logior)
MAKE_BIN_OP(^, scm_logxor)
#undef MAKE_BIN_OP

#define MAKE_UN_FN(name, scm_name)                                             \
  scm name(scm x) { return scm_name(x); }
MAKE_UN_FN(abs, scm_abs)
MAKE_UN_FN(floor, scm_floor)
MAKE_UN_FN(ceil, scm_ceiling)
#undef MAKE_UN_FN

template <class F>
auto with_guile(F f_no_args) {
  using ret_t = decltype(f_no_args());
  constexpr auto is_void = is_void_v<ret_t>;
  using ret_no_void = conditional<is_void, char, ret_t>;
  auto catch_ret = unique_ptr<ret_no_void>{(ret_no_void*)scm_with_guile(
      +[](void* erased_closure) -> void* {
        auto& typed_closure = *(F*)erased_closure;
        if constexpr(is_void) {
          typed_closure();
          return nullptr;
        } else {
          return typed_closure();
        }
      },
      (void*)&f_no_args)};
  if constexpr(is_void) {
    return;
  } else {
    return *catch_ret;
  }
}

template <class... arg_t>
scm list(arg_t&&... arg) {
  return scm_list_n(scm{forward<arg_t>(arg)}..., SCM_UNDEFINED);
}

#undef FN
}  // namespace guile
