dnl Makeup aclocal macros for pthread support.
dnl
dnl Copyright 2015 - 2021, Ron <ron@debian.org>
dnl
dnl These macros are distributed under the terms of the GNU GPL version 2.
dnl
dnl As a special exception to the GPL, it may be distributed without
dnl modification as a part of a program using a makeup generated build
dnl system, under the same distribution terms as the program itself.


# ACM_FUNC_PTHREAD_SETNAME
#
# Check for pthread_setname_np.  We can't just use AC_CHECK_FUNCS for this one
# because it has different signatures on different platforms.  Right now we
# mostly care about distinguishing the GNU version which takes two parameters
# and the MacOS one which only takes a name and must be called from the thread
# where the name is to be set.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_FUNC_PTHREAD_SETNAME],
[
ACM_PUSH_VAL([$0],[CPPFLAGS],[$PTHREAD_CPPFLAGS])dnl
ACM_PUSH_VAL([$0],[LDFLAGS],[$PTHREAD_LDFLAGS])dnl

AC_CACHE_CHECK([for GNU pthread_setname_np],[mu_cv_func_gnu_pthread_setname_np],
               [AC_LINK_IFELSE([AC_LANG_PROGRAM([[ #include <pthread.h> ]],
                                                [[ pthread_setname_np(pthread_self(),"x"); ]]
                                               )],
                               [mu_cv_func_gnu_pthread_setname_np=yes],
                               [mu_cv_func_gnu_pthread_setname_np=no]
                              )
             ])

AS_IF([test "$mu_cv_func_gnu_pthread_setname_np" = yes],
      [AC_DEFINE([HAVE_PTHREAD_SETNAME_NP_GNU],[1],
                 [Have GNU style pthread_setname_np(pthread_t thread, const char *name)])],
      [AC_CACHE_CHECK([for MacOS pthread_setname_np],[mu_cv_func_mac_pthread_setname_np],
                      [AC_LINK_IFELSE([AC_LANG_PROGRAM([[ #include <pthread.h> ]],
                                                       [[ pthread_setname_np("x"); ]]
                                                      )],
                                      [mu_cv_func_mac_pthread_setname_np=yes],
                                      [mu_cv_func_mac_pthread_setname_np=no]
                                     )
                    ])
    ])

AS_IF([test "$mu_cv_func_mac_pthread_setname_np" = yes],
      [AC_DEFINE([HAVE_PTHREAD_SETNAME_NP_MAC],[1],
                 [Have MacOS style pthread_setname_np(const char *name)])
    ])

dnl OpenBSD and FreeBSD have pthread_set_name_np declared in pthread_np.h
AS_IF([test "$mu_cv_func_gnu_pthread_setname_np" != yes && test "$mu_cv_func_mac_pthread_setname_np" != yes],
      [AC_CHECK_FUNCS([pthread_set_name_np])
    ])

ACM_POP_VAR([$0],[LDFLAGS,CPPFLAGS])dnl
])


# ACM_CXX_FORCED_UNWIND
#
# Check if abi::__forced_unwind is supported.  Code built with GCC will unwind
# the stack by throwing a special exception of this type whenever a thread is
# cancelled.  Which was a great idiom until C++11 stuffed it all up by refusing
# to allow exceptions to pass through destructors by default - which means now
# it might just abruptly terminate the entire process instead ...  But aside
# from that, it's not supported by clang on every platform, and the ancient GCC
# 4.2.1 20070719 on OpenBSD 6.1 doesn't support it either.  So things which do
# need to support those will need to explicitly check for it.
#
# The impact of it not being present can be minimised by defining the missing
# type if needed with code like the example below.  But each use of it would
# still need to be checked to see if any other special handling is required in
# the case where rather than unwinding cleanly, some thread simple ceases to
# exist when it is cancelled.
#
# // If the thread cancellation exceptions are not supported, provide our own
# // definition of the exception type.  The main trick here is that we can't
# // put it directly into namespace abi if that is really an alias to some
# // other internal namespace name.  If it's an alias to some other name than
# // __cxxabiv1, then this will explode at build time and we'll need to add
# // a new configure test for other possible aliases, but right now that is
# // what is used on all platforms with the abi namespace so far.
# #if !HAVE_ABI_FORCED_UNWIND
#
#    #if HAVE_ABI_ALIAS_TO_CXXABIV1
#     namespace __cxxabiv1
#    #else
#     namespace abi
#    #endif
#     {
#         struct __forced_unwind {};
#     }
#
# #endif
#
# The future usability of this unwinding idiom is currently something of an
# open question while the GCC maintainers kick the can down the road, having
# made their own extension inherently dangerous by switching the default C++
# standard to gnu++14 where destructors are implicitly noexcept, and not
# showing a lot of enthusiasm for addressing the question of how to deal with
# that now.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_CXX_FORCED_UNWIND],
[
dnl The C++ stack unwinding exception for thread cancellation was a GNU extension
dnl and though it is supported on some platforms by clang too, it isn't supported
dnl on every platform that is using gcc either.  So first, test if we have it.
AC_CACHE_CHECK([for abi::__forced_unwind],[mu_cv_type_forced_unwind],
               [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[ #include <cxxabi.h> ]],
                                                   [[ void f(const abi::__forced_unwind&); ]]
                                                  )],
                               [mu_cv_type_forced_unwind=yes],
                               [mu_cv_type_forced_unwind=no]
                              )
             ])

dnl If we don't, test if we at least have the "abi" namespace defined.
AS_IF([test "$mu_cv_type_forced_unwind" = yes],
      [AC_DEFINE([HAVE_ABI_FORCED_UNWIND],[1],
                 [Have abi::__forced_unwind support])],
      [AC_CACHE_CHECK([for namespace abi],[mu_cv_namespace_abi],
                      [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[ #include <cxxabi.h> ]],
                                                          [[ using namespace abi; ]]
                                                         )],
                                         [mu_cv_namespace_abi=yes],
                                         [mu_cv_namespace_abi=no]
                                        )
                    ])
    ])

dnl If the "abi" namespace is defined, we still might not be able to place our
dnl own replacement type into it, since that is an illegal construct if "abi"
dnl is an alias to some other namespace.  Right now, on all platforms we know
dnl of, it is an alias to __cxxabiv1, so test for that, and use it if so.
dnl If it's not, then either "abi" is a real namespace and we can just use it
dnl directly (which is what we attempt if this test fails), or it's aliased to
dnl some other internal namespace name - in which case the build should fail
dnl and we'll need to add a new test for additional names if/when someone ever
dnl hits that.
AS_IF([test "$mu_cv_namespace_abi" = yes],
      [AC_CACHE_CHECK([for namespace abi alias to __cxxabiv1],[mu_cv_namespace_alias_abi],
                      [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[ #include <cxxabi.h>
                                                             namespace __cxxabiv1 { struct xx {}; }
                                                          ]],
                                                          [[ abi::xx x; ]]
                                                         )],
                                         [mu_cv_namespace_alias_abi=yes],
                                         [mu_cv_namespace_alias_abi=no]
                                        )
                    ])
    ])

AS_IF([test "$mu_cv_namespace_alias_abi" = yes],
      [AC_DEFINE([HAVE_ABI_ALIAS_TO_CXXABIV1],[1],
                 [Have namespace abi alias to __cxxabiv1])
    ])
])

