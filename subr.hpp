#pragma once

#include <libguile.h>
#include <functional>
#include <optional>

namespace guile {


namespace {
template <class...>
struct function_traits_helper;
template <class ret_t, class... arg_t>
struct function_traits_helper<function<ret_t(arg_t...)>> {
  using return_t = ret_t;
  using arguments_t = tuple<arg_t...>;
  static constexpr auto nargs = sizeof...(arg_t);
};

template <class F>
using function_traits = function_traits_helper<decltype(function{*(F*)0})>;

template <class T, auto... seq>
using repeat = T;

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
#define FCALL f(reg_arg...)(opt_arg.toOpt()...)(rest_arg...)
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
        template <size_t... rest_idx_>
        struct rest<index_sequence<rest_idx_...>> {
          static auto call(arg_t... arg) {
            auto args = tuple{arg...};
            // no rest parameters
            if constexpr(rest_idx::size() == 0) {
              static_assert(sizeof...(rest_idx_) == 0);
              return f(get<reg_idx_>(args)...)(
                  maybe_arg_prepare<opt_idx_>(args)...)();
            } else {
              return f(get<reg_idx_>(args)...)(maybe_arg_prepare<opt_idx_>(
                  args)...)(list(get<rest_idx_>(args)...));
            }
          }
        };
      };
    };

    static constexpr auto callit =
        reg<reg_idx>::template opt<opt_idx>::template rest<rest_idx>::call;
  };
};
}  // namespace

template <auto primitive>
constexpr auto def_prim = wrap_helper<primitive>::def_prim;

template <auto primitive, class... arg_t>
constexpr auto call_prim =
    wrap_helper<primitive>::template call<arg_t...>::callit;

#define GUILE_SUBR_LAMBDA(reg_args, opt_args, rest_args, ...)                  \
  [] reg_args { return [&] opt_args { return [&] rest_args { __VA_ARGS__ }; }; }

#define GUILE_DEF_SUBR(cname, scm_name, ...)                                   \
  namespace subr_impl {                                                        \
  constexpr auto cname = +GUILE_SUBR_LAMBDA(__VA_ARGS__);                      \
  }                                                                            \
  namespace definer {                                                          \
  scm cname() { return def_prim<subr_impl::cname>(scm_name); }                 \
  }                                                                            \
  template <class... arg_t>                                                    \
  auto cname(arg_t&&... arg) {                                                 \
    call_prim<subr_impl::cname, arg...>(forward<arg_t>(arg)...);               \
  }
}  // namespace guile
