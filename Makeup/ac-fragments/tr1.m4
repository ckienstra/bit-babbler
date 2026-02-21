dnl Makeup aclocal macros for C++03 TR1 compatibility support.
dnl
dnl Copyright 2017 - 2018, Ron <ron@debian.org>
dnl
dnl These macros are distributed under the terms of the GNU GPL version 2.
dnl
dnl As a special exception to the GPL, it may be distributed without
dnl modification as a part of a program using a makeup generated build
dnl system, under the same distribution terms as the program itself.


# ACM_TR1_TYPE_TRAITS
#
# Check what support we have for type_traits.  Prior to C++11 they were part
# of the tr1 extensions.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_TR1_TYPE_TRAITS],
[
AC_CHECK_HEADERS([type_traits],[],
                 [AC_CHECK_HEADERS([tr1/type_traits])])
])


# ACM_TR1_UNORDERED_MAP
#
# Check what support we have for a hashed map type.  We should almost never
# need to fall back as far as the __gnu_cxx::hash_map anymore, but we are
# still at that awkward stage in the transition between the tr1 implementation
# and the standardised in C++11 one.  For bonus fun, in some cases, like clang
# in FreeBSD 10.3, the tr1 header is provided (as a symlink to the "standard"
# one), but the type is not actually included in the std::tr1 namespace at all.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_TR1_UNORDERED_MAP],
[
AC_CHECK_HEADERS([unordered_map],[],
                 [AC_CHECK_HEADERS([tr1/unordered_map],[],
	                           [AC_CHECK_HEADERS([ext/hash_map])])])
])


# ACM_TR1_UNORDERED_SET
#
# Check what support we have for a hashed set type.  We should almost never
# need to fall back as far as the __gnu_cxx::hash_set anymore, but we are
# still at that awkward stage in the transition between the tr1 implementation
# and the standardised in C++11 one.  For bonus fun, in some cases, like clang
# in FreeBSD 10.3, the tr1 header is provided (as a symlink to the "standard"
# one), but the type is not actually included in the std::tr1 namespace at all.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_TR1_UNORDERED_SET],
[
AC_CHECK_HEADERS([unordered_set],[],
                 [AC_CHECK_HEADERS([tr1/unordered_set],[],
	                           [AC_CHECK_HEADERS([ext/hash_set])])])
])


# ACM_TR1_HASH
#
# Check which hash type we need to use for unordered_map and unordered_set.
# For the hash template type in the the header <{tr1/,}functional> we need to
# do a bit more work because the <functional> header won't just error out in
# the same way as unordered_{map,set} do above when the C++ standard in use is
# too early.  It will just silently not define the hash type we need. So check
# that it is actually available explicitly.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_TR1_HASH],
[
AC_CACHE_CHECK([for std::hash],[mu_cv_type_std_hash],
               [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <functional>]],
                                                   [[using std::hash;]])
                                  ],
                                  [mu_cv_type_std_hash=yes],
                                  [mu_cv_type_std_hash=no]
                                 )
             ])

AS_IF([test "$mu_cv_type_std_hash" = yes],
      [AC_DEFINE([HAVE_STD_HASH],[1],
                 [The system provides std::hash in the header <functional>])],
      [AC_CACHE_CHECK([for std::tr1::hash],[mu_cv_type_tr1_hash],
                      [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <tr1/functional>]],
                                                          [[using std::tr1::hash;]])
                                         ],
                                         [mu_cv_type_tr1_hash=yes],
                                         [mu_cv_type_tr1_hash=no]
                                        )
                    ])
    ])

AS_IF([test "$mu_cv_type_tr1_hash" = yes],
      [AC_DEFINE([HAVE_TR1_HASH],[1],
                 [The system provides std::tr1::hash in the header <tr1/functional>])
    ])
])

