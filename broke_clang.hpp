#include <libguile.h>
#include <boost/preprocessor/cat.hpp>
#include <complex>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>

#define FN(...) [&](auto _) { return __VA_ARGS__; }

namespace guile {
using namespace std;
struct scm {
  SCM obj;
  void protect() { scm_gc_protect_object(obj); }

  scm(SCM obj) : obj{obj} {}
  operator SCM() { return obj; }

  template <class... arg_t>
  scm operator()(arg_t... arg) {
    return scm_call(obj, arg..., SCM_UNDEFINED);
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
  return scm_list_n(scm{arg}..., SCM_UNDEFINED);
}

namespace {
template <class...>
struct function_traits_helper;
template <class ret_t, class... arg_t>
struct function_traits_helper<function<ret_t(arg_t...)>> {
  using return_t = ret_t;
  using arguments_t = tuple<arg_t...>;
  static constexpr auto nargs = sizeof...(arg_t);
};
}  // namespace

template <class F>
using function_traits = function_traits_helper<decltype(function{*(F*)0})>;

template <class T, auto... seq>
using repeat = T;

namespace {
template <auto f>
struct wrap_helper {
  using F = decltype(f);
  template <class>
  struct reg;
  template <auto... reg_arg_t>
  struct reg<index_sequence<reg_arg_t...>> {
    template <class...>
    struct opt;
    template <auto... opt_arg_t>
    struct opt<index_sequence<opt_arg_t...>> {
      template <class>
      struct rest;
      template <auto... rest_arg_t>
      struct rest<index_sequence<rest_arg_t...>> {
        static_assert(sizeof...(rest_arg_t) <= 1);
        static scm wrapped(repeat<scm, reg_arg_t>... reg_arg,
                           repeat<scm, opt_arg_t>... opt_arg,
                           repeat<scm, rest_arg_t>... rest_arg) {
#define FCALL f(scm{reg_arg}...)(scm{opt_arg}.toOpt()...)(rest_arg...)
          if constexpr(is_void_v<decltype(FCALL)>) {
            FCALL;
            return SCM_UNSPECIFIED;
          } else {
            return scm{FCALL};
          }
#undef FCALL
        }
      };
    };
  };

  using reg_traits = function_traits<F>;
  using reg_args = typename reg_traits::arguments_t;
  using opt_traits = function_traits<typename reg_traits::return_t>;
  using opt_args = typename opt_traits::arguments_t;
  using rest_traits = function_traits<typename opt_traits::return_t>;
  using rest_args = typename rest_traits::arguments_t;

  static constexpr auto nreg = tuple_size_v<reg_args>;
  static constexpr auto nopt = tuple_size_v<opt_args>;
  static constexpr auto nrest = tuple_size_v<rest_args>;

  static constexpr auto wrap =
      reg<make_index_sequence<nreg>>::template opt<make_index_sequence<nopt>>::
          template rest<make_index_sequence<nrest>>::wrapped;

  static scm def_prim(string name) {
    return scm_c_define_gsubr(name.c_str(), nreg, nopt, nrest, (void*)wrap);
  }

  template <class... arg_t>
  struct call {
    using reg_idx = make_index_sequence<nreg>;

    template <size_t offset, class seq>
    struct offset_helper;
    template <size_t offset, size_t... idx>
    struct offset_helper<offset, index_sequence<idx...>> {
      using type = std::index_sequence<idx + offset...>;
    };
    template <size_t offset, class seq>
    using offset_seq = typename offset_helper<offset, seq>::type;

    using opt_idx = offset_seq<nreg, make_index_sequence<nopt>>;

    static constexpr auto rest_start = nreg + nopt;
    using rest_idx =
        offset_seq<rest_start,
                   make_index_sequence<sizeof...(arg_t) - rest_start>>;

    template <size_t i, class tup_t>
    static decltype(get<i>(opt_args{})) maybe_arg_prepare(tup_t tup) {
      if constexpr(i < nopt + nreg) {
        return optional{get<i>(tup)};
      } else {
        return nullopt;
      }
    }

    template <class>
    struct reg;
    template <size_t... reg_idx_>
    struct reg<index_sequence<reg_idx_...>> {
      template <class>
      struct opt;
      template <size_t... opt_idx_>
      struct opt<index_sequence<opt_idx_...>> {
        template <class>
        struct rest;
        template <>
        struct rest<index_sequence<>> {
          static auto call(arg_t... arg) {
            auto args = tuple{arg...};
            return f(get<reg_idx_>(args)...)
              (maybe_arg_prepare<opt_idx_>(args)...)
              ();
          }
        };
      };
    };

    static constexpr auto callit = reg<reg_idx>
      ::template opt<opt_idx>
      ::template rest<rest_idx>
      ::call;
  };
};
}  // namespace

template <auto f>
constexpr auto def_prim = wrap_helper<f>::def_prim;

#define GUILE_SUBR_LAMBDA(reg_args, opt_args, rest_args, ...)                  \
  [] reg_args { return [&] opt_args { return [&] rest_args { __VA_ARGS__ }; }; }

#define GUILE_DEF_SUBR(cname, scm_name, ...)                                   \
  namespace subr_impl {                                                        \
  constexpr auto cname = +GUILE_SUBR_LAMBDA(__VA_ARGS__);                      \
  }                                                                            \
  namespace definer {                                                            \
  scm cname() { return def_prim<subr_impl::cname>(scm_name); }                 \
  }                                                                            \
  template <class... arg_t>                                                    \
  auto cname(arg_t&&... arg) {                                                 \
    wrap_helper<subr_impl::cname>::call<arg_t...>::callit(                        \
        forward<arg_t>(arg)...);                                               \
  }
}  // namespace guile

#undef FN
