dnl Makeup aclocal macros.
dnl
dnl Copyright 2003 - 2021, Ron <ron@debian.org>
dnl
dnl These macros are distributed under the terms of the GNU GPL version 2.
dnl
dnl As a special exception to the GPL, it may be distributed without
dnl modification as a part of a program using a makeup generated build
dnl system, under the same distribution terms as the program itself.


# ACM_LANG
#
# Expands to the currently set AC_LANG.  We use this wrapper in case _AC_LANG
# ever changes as the only place we can currently retrieve that value from.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_LANG], [m4_defn([_AC_LANG])])


# ACM_LANG_ABBREV
#
# Expands to the short signature of _AC_LANG which can be used in shell
# variable names, or in M4 macro names, for the currently set AC_LANG.
# We use this wrapper in case _AC_LANG_ABBREV ever changes as the only
# place we can currently retrieve that value from.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_LANG_ABBREV], [_AC_LANG_ABBREV])


# ACM_LANG_PREFIX
#
# Expands to the short (upper case) signature of _AC_LANG that is used to
# prefix environment variables like FLAGS, for the currently set AC_LANG.
# We use this wrapper in case _AC_LANG_PREFIX ever changes as the only
# place we can currently retrieve that value from.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_LANG_PREFIX], [_AC_LANG_PREFIX])


# ACM_LANG_COMPILER
#
# Expands to the currently set value for CC if AC_LANG is C, or CXX for C++,
# and so on.  We use this wrapper in case _AC_CC ever changes as the only
# place we can currently retrieve that value from.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_LANG_COMPILER], [$[]_AC_CC])


# ACM_TR_SH_LITERAL([LITERAL])
#
# Transform LITERAL into a valid shell variable name.  This is similar to
# AS_TR_SH except it is not polymorphic and operates strictly on the literal
# passed to it without expanding any shell variables or constructs.  We use
# the same remapping, as we want strings transformed by this to be the same
# as when they are passed to AS_VAR_PUSHDEF, including the oddball mapping
# of * and + to 'p', and emulating the shell transformation behaviour of
# stripping all backslash, single, and double quotes, before converting any
# other non-alphanumeric characters to an underscore.
#
# It's not clear whether doing an extra translit (to strip the quotes) is less
# optimal than stripping those using two replacements with m4_bpatsubsts, but
# we can't use m4_bpatsubst to do the non-alphanumeric replacement, as that
# needs to be overquoted to avoid expanding any macros in the literal, which
# means we'll transform the final outer quotes to underscores as well, adding
# extra leading and trailing characters that weren't in the original literal.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_TR_SH_LITERAL],
[dnl
m4_translit(m4_translit(m4_bpatsubst([[[[$1]]]],[\\\(.\)],[\1]),
                        ['"]),
  [*+[]]]m4_dquote(m4_defn([m4_cr_not_symbols2]))[,
  [pp[]]]m4_dquote(m4_for(,1,255,,[[_]]))[)])
])


# ACM_ADD_OPT([VARS],[ITEMS],[SEPARATOR])
#
# Append each of the comma-separated ITEMS to a SEPARATOR-separated list in
# each of the comma-separated VARS.  If SEPARATOR is not specified, it will
# default to a single space.  For example: ACM_ADD_OPT([foo,bar],[fee,fie])
# is equivalent to:
#   foo+=" fee fie"
#   bar+=" fee fie"
# But without the leading space if foo or bar were empty before this operation.
# And then ACM_ADD_OPT([foo],[foe,fum],[, ]) would result in foo containing the
# suffix string "fee fie, foe, fum".
# -----------------------------------------------------------------------------
AC_DEFUN([ACM_ADD_OPT],
[dnl
dnl The expansion of $3 here is double quoted if it's not empty, so that it can
dnl contain a comma for outputting comma-separated lists.
m4_pushdef([sep],[m4_default_quoted([$3],[ ])])dnl
m4_chomp(m4_foreach([var],[$1],
                    [m4_foreach([val],[$2],
                                [
    var="${var:+$var[]AS_ESCAPE(m4_dquote(m4_expand([sep])))}m4_expand([val])"[]dnl
                              ])dnl
                  ])dnl
        )dnl
m4_popdef([sep])dnl
])


# ACM_PUSH_VAR([SCOPE],[VARS],[NEW-VALUE])
#
# For each comma separated variable in VARS, preserve their current value in
# acm_save_SCOPE_VAR, and if NEW-VALUE is set, assign that value to it.  The
# previous value will be restored to each variable that ACM_POP_VAR is later
# called for using the SCOPE it was pushed to.  SCOPE may be any literal that
# can be used to form a valid shell variable name, but when called from in a
# macro, the name of the calling macro (ie. $0) is probably a sensible choice.
# This is not a stack, only the last pushed value for each SCOPE used can be
# restored.
# -----------------------------------------------------------------------------
AC_DEFUN([ACM_PUSH_VAR],
[dnl
m4_foreach([var],[$2],[dnl
acm_save_[]ACM_TR_SH_LITERAL([$1])_[]var=$var
m4_ifval([$3],[dnl
var=[$3]
])dnl
])dnl
])


# ACM_POP_VAR([SCOPE],[VARS])
#
# For each comma separated variable in VARS, restore the value that was saved
# by a previous call to ACM_PUSH_VAR. This is not a stack, only the last value
# which was pushed for SCOPE is saved.
# -----------------------------------------------------------------------------
AC_DEFUN([ACM_POP_VAR],
[dnl
m4_foreach([var],[$2],[dnl
var=$acm_save_[]ACM_TR_SH_LITERAL([$1])_[]var
])dnl
])


# ACM_PUSH_VAL([SCOPE],[VARS],[VALUES],[SEPARATOR])
#
# For each comma separated variable in VARS, preserve their current value in
# acm_save_SCOPE_VAR, then append each of the comma-separated VALUES to it as
# a SEPARATOR separated list.  If SEPARATOR is not specified, it will default
# to a single space.  The saved values can be restored for each variable if
# ACM_POP_VAR is later called using the SCOPE it was pushed to.  This is not
# a stack, only the last pushed value for each SCOPE used can be restored.
#
# This is a convenience macro that is equivalent to calling ACM_PUSH_VAR
# followed by ACM_ADD_OPT.
# -----------------------------------------------------------------------------
AC_DEFUN([ACM_PUSH_VAL],
[dnl
m4_foreach([var],[$2],[dnl
acm_save_[]ACM_TR_SH_LITERAL([$1])_[]var=$var
m4_ifval([$3],[dnl
ACM_ADD_OPT(var,[$3],[$4])
])dnl
])dnl
])


# ACM_REPUSH_VAL([SCOPE],[VARS],[VALUES],[SEPARATOR])
#
# For each comma separated variable in VARS, restore the previously pushed
# value from acm_save_SCOPE_VAR, then append each of the comma-separated VALUES
# to it as a SEPARATOR separated list.  If SEPARATOR is not specified, it will
# default to a single space.  The initially pushed value will be still restored
# for each variable that ACM_POP_VAR is later called for.  This is not a stack,
# only the initially pushed value can be restored.
#
# This is a convenience macro that is equivalent to calling ACM_POP_VAR
# followed by ACM_PUSH_VAL.
# -----------------------------------------------------------------------------
AC_DEFUN([ACM_REPUSH_VAL],
[dnl
m4_pushdef([sep],[m4_default_quoted([$4],[ ])])dnl
m4_pushdef([scope],m4_expand([acm_save_[]ACM_TR_SH_LITERAL([$1])_]))dnl
m4_foreach([var],[$2],[m4_ifval([$3],[dnl
var="${scope[]var:+$scope[]var[]AS_ESCAPE(m4_dquote(m4_expand([sep])))}m4_join(sep,$3)"
],[dnl
var=$scope[]var
])dnl
])dnl
m4_popdef([scope],[sep])dnl
])


# ACM_FOREACH([VAR],[LIST],[IF-LITERAL],[IF-NOT])
#
# Iterate over each of the comma-separated elements of LIST, assigning them to
# VAR.  If VAR is a literal then IF-LITERAL will be expanded.  If VAR is a
# shell expression, then for each of the comma-separated elements it expands to
# the expansion of IF-NOT will be executed.  If IF-NOT is unset, then it will
# use IF-LITERAL with each instance of VAR replaced by $VAR (so it will expand
# as a shell variable instead of an m4 macro) - which means you normally should
# not need to set IF-NOT unless there is something additionally special which
# should be done if looping in the shell.
#
# This allows macros to be written which behave the same at runtime regardless
# of whether the comma-separated list provided to them is literal text or the
# value of a shell variable.  Literal elements will be unrolled by m4, while
# runtime variables will be looped over in the shell, and both may be used in
# LIST together.  The following are all equivalent in their runtime behaviour:
#
# list="foo,bar"
# ACM_FOREACH([var],[foo,bar],[DO_STUFF(var)])
# ACM_FOREACH([var],[$list],[DO_STUFF(var)])
# ACM_FOREACH([var],[$list],[DO_STUFF(var)],[DO_STUFF($var)])
#
# And this is legal, but does the same stuff to each of foo and bar twice.
# ACM_FOREACH([var],[foo,$list,bar],[DO_STUFF(var)])
#
# Note that VAR normally should not be overquoted where it is used, most cases
# would want it expanded as the argument to IF-LITERAL, and not in some later
# expansion of whatever it is passed to.
# -----------------------------------------------------------------------------
dnl Recursion count for scoping ACM_FOREACH expansion of macros which then also
dnl use ACM_FOREACH again.
m4_define([_acm_foreach_nestlevel],[0])
AC_DEFUN([ACM_FOREACH],
[dnl
m4_pushdef([_acm_foreach_nestlevel],m4_incr(_acm_foreach_nestlevel))dnl
m4_pushdef([scope],[$0_[]_acm_foreach_nestlevel])dnl
dnl In the case of IF-LITERAL handling a non-literal expression, we overquote
dnl it to ensure that VAR is converted to $VAR in the text we were passed, not
dnl in whatever results from some subsequent expansion of macros in it. We can
dnl then safely expand any remaining macros in it after that is done.
m4_expand(m4_foreach([$1],[$2],[AS_LITERAL_IF([$1],[$3],[dnl
ACM_PUSH_VAR(scope,[IFS],[,])dnl
acm_[$1]_list="$1"
for [$1] in $acm_[$1]_list; do
    ACM_POP_VAR(scope,[IFS])dnl
    [$1]="${[$1][#]"${[$1]%%[[![:space:]]]*}"}"
    [$1]="${[$1]%"${[$1][##]*[[![:space:]]]}"}"
    m4_default_quoted([$4],m4_bpatsubst([[$3]],[$1],[$$1]))
done
ACM_POP_VAR(scope,[IFS])])]))dnl
m4_popdef([scope],[_acm_foreach_nestlevel])dnl
])


# ACM_FOREACH_W([VAR],[LIST],[IF-LITERAL],[IF-NOT])
#
# This macro is similar to ACM_FOREACH, except it expects the elements of LIST
# to be space-separated.  The corresponding example case would be:
#
# list="foo bar"
# ACM_FOREACH([var],[foo $list bar],[DO_STUFF(var)])
#
# Which again would do DO_STUFF to each of foo and bar twice, once as a literal
# expansion and again as an operation on the expansion of the shell variable.
# -----------------------------------------------------------------------------
AC_DEFUN([ACM_FOREACH_W],
[dnl
dnl In the case of IF-LITERAL handling a non-literal expression, we overquote
dnl it to ensure that VAR is converted to $VAR in the text we were passed, not
dnl in whatever results from some subsequent expansion of macros in it. We can
dnl then safely expand any remaining macros in it after that is done.
m4_expand(m4_foreach_w([$1],[$2],[AS_LITERAL_IF([$1],[$3],[
acm_[$1]_list="$1"
for [$1] in $acm_[$1]_list; do
    m4_default_quoted([$4],m4_bpatsubst([[$3]],[$1],[$$1]))
done])]))dnl
])


# ACM_FOREACH_REV_W([VAR],[LIST],[IF-LITERAL],[IF-NOT])
#
# This macro is similar to ACM_FOREACH_W, except it operates on each element of
# the space-separated LIST in the reverse order, acting on the right-most list
# elements first.
# -----------------------------------------------------------------------------
AC_DEFUN([ACM_FOREACH_REV_W],
[dnl
dnl In the case of IF-LITERAL handling a non-literal expression, we overquote
dnl it to ensure that VAR is converted to $VAR in the text we were passed, not
dnl in whatever results from some subsequent expansion of macros in it. We can
dnl then safely expand any remaining macros in it after that is done.
m4_expand(m4_foreach([$1],m4_dquote(m4_reverse(m4_unquote(m4_split(m4_normalize([$2]))))),
          [AS_LITERAL_IF([$1],[$3],[
acm_[$1]_list="$1"
acm_[$1]_rev_list=""
for [$1] in $acm_[$1]_list; do
    acm_[$1]_rev_list="$[$1] $acm_[$1]_rev_list"
done
for [$1] in $acm_[$1]_rev_list; do
    m4_default_quoted([$4],m4_bpatsubst([[$3]],[$1],[$$1]))
done])]))dnl
])


# ACM_IF_CONTAINS([VAR],[ITEM],[IF-ITEM],[IF-NOT-ITEM],[SEPARATOR])
#
# This macro tests if ITEM is included in the SEPARATOR-separated list that is
# the current value of VAR, and executes the commands in IF-ITEM if so, else
# the (optional) commands IF-NOT-ITEM.  If SEPARATOR is not explicitly passed
# then it defaults to checking for space-separated items.
# For example:
# ACM_IF_CONTAINS([FOO_LIBS],[bar],[TEST_FOR_LIBBAR])
#
# Will execute the expansion of TEST_FOR_LIBBAR if FOO_LIBS includes 'bar'.
# -----------------------------------------------------------------------------
AC_DEFUN([ACM_IF_CONTAINS],
[dnl
dnl The expansion of $5 here is double quoted if it's not empty, so that it can
dnl contain a comma for checking comma-separated lists.
dnl
dnl Note that we don't normally need to quote the variables in case statements,
dnl because its expansion won't be word-split, but we do need quotes around at
dnl least the separators in this case since it may contain shell metacharacters
dnl which would break things.  We prepend and append the separator to the list
dnl so that we can do word delimited matching including items at the beginning
dnl and end of the list.  We don't want to match with a partial item name here.
m4_pushdef([sep],[m4_default_quoted([$5],[ ])])dnl
AS_CASE(["AS_ESCAPE(m4_dquote(m4_expand([sep])))${$1}[]AS_ESCAPE(m4_dquote(m4_expand([sep])))"],
        [*"AS_ESCAPE(m4_dquote(m4_expand([sep])))$2[]AS_ESCAPE(m4_dquote(m4_expand([sep])))"*],
        [$3],
        [$4])dnl
m4_popdef([sep])dnl
])


# ACM_IF_VENDOR_BUILD([IF-VENDOR-BUILD],[IF-LOCAL-BUILD],[IF-OTHER])
#
# If this is a build intended for installation to the vendor-reserved locations
# on the filesystem (i.e. with --prefix=/usr, for a distro package or similar)
# then the expansion of IF-VENDOR-BUILD will be used.  If we are building for a
# local admin install (i.e. with --prefix=/usr/local) then IF-LOCAL-BUILD will
# be used.  For all other install prefixes, IF-OTHER will be used if provided,
# otherwise IF-LOCAL-BUILD will be used in that case too.
#
# Most things will never need to use this as the desired result can usually be
# obtained by simply using the $prefix variable and its derived values as they
# were intended to be.  This is a workaround for the things which break that
# assumption and need other special handling to make them behave sanely for
# local user builds.  Yeah, we're looking at you systemd.  Not in admiration.
# -----------------------------------------------------------------------------
AC_DEFUN([ACM_IF_VENDOR_BUILD],
[dnl
AS_IF([test "$prefix" = "/usr" ||
     { test "$prefix" = "NONE" && test "$ac_default_prefix" = "/usr" ; }],
      [$1],
 m4_ifval([$3],dnl
 [dnl
      [test "$prefix" = "/usr/local" ||
     { test "$prefix" = "NONE" && test "$ac_default_prefix" = "/usr/local" ; }],
      [$2],
      [$3]
 ],[dnl
      [$2]
 ])dnl
     )dnl
])


# ACM_EXPAND_VARS([OUTVAR],[EXP])
#
# Recursively expand shell expression EXP until no further expansion occurs
# and assign the resulting string to OUTVAR.  No checking is done to stop a
# pathological expression from expanding infinitely.  This is mostly intended
# for the simple case of expanding a variable which may be defined in terms
# of other variables, in the manner of the install directory variables which
# are usually relative to ${prefix} and other directory root variables.
# -----------------------------------------------------------------------------
AC_DEFUN([ACM_EXPAND_VARS],
[dnl
$1=$2
while test "$$1" != "$acm_tmp_$1"; do
    acm_tmp_$1=$$1
    eval $1=$$1
done
AS_UNSET([acm_tmp_$1])dnl
])


# ACM_EXPAND_DIR([OUTVAR],[INVAR],[DESCRIPTION])
#
# This is a specialisation of ACM_EXPAND_VARS for standard install directory
# variables which handles the case of ${prefix} and/or ${exec_prefix} not yet
# being set (since that normally happens near the very end of the generated
# configure script, just before config.status is created, unless values for
# them were passed explicitly by the caller).  In addition to fully expanding
# INVAR and assigning that to OUTVAR, this will declare OUTVAR itself to be a
# precious substitution variable, and define it in the config header.
# If the DESCRIPTION parameter is passed, it will be included as the precious
# variable description for ./configure --help, and as the macro description
# in the configuration header.  Otherwise a default description will be used
# to indicate it is the expansion of INVAR.
# -----------------------------------------------------------------------------
AC_DEFUN([ACM_EXPAND_DIR],
[dnl
ACM_PUSH_VAR([$0],[prefix,exec_prefix])dnl

AS_IF([test "$prefix" = "NONE"],[prefix=$ac_default_prefix])
AS_IF([test "$exec_prefix" = "NONE"],[exec_prefix=$prefix])

ACM_EXPAND_VARS([$1],[$2])dnl
m4_pushdef([desc],[m4_default_quoted([$3],[The fully expanded $2 path])])dnl
AC_ARG_VAR([$1],desc)
AC_DEFINE_UNQUOTED([$1],["$$1"],desc)
m4_popdef([desc])dnl
ACM_POP_VAR([$0],[prefix,exec_prefix])dnl
])


# ACM_PUSH_LANG_FOR_FLAGS([CALLER],[FLAGS_PREFIX])
#
# Call AC_LANG_PUSH for the language that FLAGS_PREFIX is used for.  Reporting
# an error in the CALLER if that can't be mapped to a language identifier.
# -----------------------------------------------------------------------------
AC_DEFUN([ACM_PUSH_LANG_FOR_FLAGS],
[dnl
m4_case([$2],
        [C],[AC_LANG_PUSH([C])],
        [CXX],[AC_LANG_PUSH([C++])],
        [m4_fatal([$1: unknown toolchain type '$2'])])dnl
])


# ACM_POP_LANG_FOR_FLAGS([CALLER],[FLAGS_PREFIX])
#
# Call AC_LANG_POP for the language that FLAGS_PREFIX is used for.  Reporting
# an error in the CALLER if that can't be mapped to a language identifier.
# -----------------------------------------------------------------------------
AC_DEFUN([ACM_POP_LANG_FOR_FLAGS],
[dnl
m4_case([$2],
        [C],[AC_LANG_POP([C])],
        [CXX],[AC_LANG_POP([C++])],
        [m4_fatal([$1: unknown toolchain type '$2'])])dnl
])


# ACM_ADD_UNIQUE([VARS],[ITEMS],[SEPARATOR])
#
# This macro does the same thing as ACM_ADD_OPT except it will only add each
# item if it isn't already present in the current list held by the variable.
# Unlike ACM_ADD_OPT, it will also expand any variables in the list of ITEMS
# rather than simply appending them as variables, so that it is the expanded
# content which is checked for uniqueness, not the variable name.
# -----------------------------------------------------------------------------
AC_DEFUN([ACM_ADD_UNIQUE],
[dnl
m4_chomp(m4_foreach([var],[$1],
                    [ACM_FOREACH([val],[$2],
                                 [
ACM_IF_CONTAINS([var],[val],
                [],
                [var="${var:+$var[]AS_ESCAPE(m4_dquote(m4_expand([sep])))}m4_expand([val])"],
                [$3])dnl
                               ])dnl
                  ])dnl
        )dnl
])


# ACM_ADD_LIBS([VARS],[LIBS])
#
# This macro is similar to ACM_ADD_UNIQUE except that it operates on a space-
# separated list of LIBS in reverse order (from right to left) prepending any
# unique new elements to the value of each of the (comma-separated) VARS, as
# a space-separated list.
#
# Typically, this behaviour is most useful for building up a concise list of
# libraries to link with, where "high-level" libraries (with dependencies on
# other libraries) must be specified to the left of all their dependencies,
# and where those libraries may share common lower-level dependencies.  Each
# of the shared dependencies will be included only once, in an order which is
# suitable for use with both static and dynamic linking.  For example:
#
# FOO_LIBS="foo m"
# BAR_LIBS="bar z m"
# ACM_ADD_LIBS([MY_LIBS],[mylib $BAR_LIBS $FOO_LIBS])
#
# Will result in MY_LIBS="mylib bar z foo m".  The list may also be built up
# progressively, so the same result would be obtained from:
# ACM_ADD_LIBS([MY_LIBS],[$FOO_LIBS])
# ACM_ADD_LIBS([MY_LIBS],[$BAR_LIBS])
# ACM_ADD_LIBS([MY_LIBS],[mylib])
#
# The only requirement is that each macro expansion must be fully specified,
# so that none of the earlier calls implicitly depend upon libraries which
# will only be listed in later additions - but ordering library tests to run
# with low level library checks being done before higher level ones should
# already be a fairly natural thing to do in most cases.
# -----------------------------------------------------------------------------
AC_DEFUN([ACM_ADD_LIBS],
[dnl
m4_chomp(m4_foreach([var],[$1],
                    [ACM_FOREACH_REV_W([val],[$2],
                                       [
ACM_IF_CONTAINS([var],[val],
                [],
                [var="m4_expand([val])${var:+ $var}"])dnl
                                     ])dnl
                  ])dnl
        )dnl
])


# SHUT_THE_FUP(AC_MACRO)
#
# Stealth mode for AC_ macros.  Exploits implementation so it's
# potentially fragile, but almost all failure modes do not
# affect the functional output, and those that do should be
# immediately obvious if they occur :)  Use it to wrap a macro
# that would output something you don't want on the console.
# Do NOT quote them.  Will honour all other nasty hack's disclaimers
# upon presentation.
# -------------------------------------------------------------------
AC_DEFUN([SHUT_THE_FUP],
[dnl
exec 6>/dev/null
$@dnl
exec 6>&1
])


# ACM_REQUIRE_LN_S
#
# This macro should be AC_REQUIRE'd by any macro that
# may need $LN_S to be defined before it is expanded.
# ---------------------------------------------------
AC_DEFUN([ACM_REQUIRE_LN_S],
[dnl
if test -z "$LN_S"; then
    SHUT_THE_FUP(AC_PROG_LN_S)
fi
])


# FIND_AND_LINK_IF_LOCAL([FILE][,DEST][,SOURCE_DIRS])
#
# If FILE does not exist in dir DEST look for it in the immediate
# parent directories and if found create a symlink to it.  Else
# try to find it in a space-separated list of SOURCE_DIRS (which
# default to a list of standard system locations) and then copy it.
# If DEST is not supplied explicitly, it will default to $srcdir,
# if DEST is supplied and does not exist, it will be created too.
# DEST is assumed to always be relative to $srcdir.
# ---------------------------------------------------------------
AC_DEFUN([FIND_AND_LINK_IF_LOCAL],
[dnl
AC_REQUIRE([ACM_REQUIRE_LN_S])dnl
dnl
if test -n "[$2]"; then
    _filedest=$srcdir/[$2]
    mkdir -p $_filedest
else
    _filedest=$srcdir
fi
if test -n "[$3]"; then
    _filesources="[$3]"
else
    _filesources="/usr/share/misc /usr/share/automake* /usr/share/libtool"
fi
if test ! -e "$_filedest/[$1]" ; then
    AC_MSG_CHECKING([for $_filedest/[$1]])

    ( cd $_filedest
        for d in ".." "../.." ; do
            if test -r "$d/[$1]" ; then
                AC_MSG_RESULT([linking from $d/[$1].])
                $LN_S "$d/[$1]" .
                break
            fi
        done
    )

    if test ! -e "$_filedest/[$1]" ; then
        for d in $_filesources;
	do
            if test -r "$d/[$1]" ; then
                AC_MSG_RESULT([copying from $d/[$1].])
                cp -a "$d/[$1]" "$_filedest/[$1]"
                break
            fi
        done
    fi

    if test ! -e "$_filedest/[$1]" ; then
        AC_MSG_ERROR([Failed to locate [$1].  Stopping.])
    fi
fi
])


# FIND_AND_COPY_UNLESS_LOCAL([FILE][,DEST])
#
# If FILE does not exist in dir DEST look for it in the immediate
# parent directories and if found do nothing.  Else try to find
# it in a standard system location and then copy it over.
# Note that we dereference symlinks here, which is probably what
# you want if you're using this macro..  maybe.
# If DEST is not supplied explicitly, it will default to $srcdir,
# if DEST is supplied and does not exist, it will be created too.
# ---------------------------------------------------------------
AC_DEFUN([FIND_AND_COPY_UNLESS_LOCAL],
[dnl
if test -n "[$2]"; then
    _filedest=[$2]
    mkdir -p $_filedest
else
    _filedest=$srcdir
fi
if test ! -e "$_filedest/[$1]" ; then
    AC_MSG_CHECKING([for $_filedest/[$1]])

    if (
        if (
            cd $_filedest
            for d in ".." "../.." ; do
                if test -r "$d/[$1]" ; then
                    AC_MSG_RESULT([leeching from $d/[$1].])
                    exit 1
                fi
            done
           ) ;
        then
            for d in "/usr/share/misc" "/usr/share/automake" "/usr/share/libtool" \
		     "/usr/share/automake-1.6" "/usr/share/automake-1.7"	  ; do
                if test -r "$d/[$1]" ; then
                    cp -aL "$d/[$1]" "$_filedest/[$1]"
                    AC_MSG_RESULT([copying from $d/[$1].])
                    exit 1
                fi
            done
        else
            exit 1
        fi
       ) ;
    then
        AC_MSG_ERROR([Failed to locate [$1].  Stopping.])
    fi
fi
])


# ACM_CONFIG_MAKEFILE(MAKEUP_GMAKE_DIR,[GLOBAL_VARIABLES])
#
# This macro instantiates a forwarding Makefile in the build directory
# and its corresponding Makefile.acsubst.  It is also used to define
# global variables that need to be available in config.status.
# --------------------------------------------------------------------
AC_DEFUN([ACM_CONFIG_MAKEFILE],
[dnl
dnl This 'before' is not strictly required, but since this macro
dnl will usually define globals needed before ACM_CONFIG_HEADER
dnl then it seems like a reasonable sanity check.  Feel free to
dnl remove or work around it if it is causing real problems.
AC_BEFORE([$0],[ACM_CONFIG_HEADER])dnl

m4_if([$#],0,[AC_MSG_ERROR([[ACM_CONFIG_MAKEFILE] must have at least one parameter])])

AC_CONFIG_COMMANDS([Makefile],
                   [
                    cat > Makefile <<EOF
# Makeup ACM_CONFIG_MAKEFILE generated makefile.
#
#  Copyright 2003 - 2021, Ron <ron@debian.org>
#
# This file is distributed under the terms of the GNU GPL version 2.
#
# As a special exception to the GPL, it may be distributed without
# modification as a part of a program using a makeup generated build
# system, under the same distribution terms as the program itself.

include Makefile.acsubst
include \$(MAKEUP_TOP_CONFIG)

ifneq (\$(strip \$(MAKEUP_VERBOSE)),)
  include \$(MAKEUP_GMAKE_DIR)/makefile.makeup
else
 -include \$(MAKEUP_GMAKE_DIR)/makefile.makeup
endif

EOF
                   ],[
                    [$2]
                   ])
AC_CONFIG_FILES([Makefile.acsubst:$1/makefile.acsubst])dnl
])


# ACM_DEFINE_PUBLIC(VARIABLE,[VALUE],[DESCRIPTION])
#
# Causes VARIABLE to be defined in a public config header, such as:
#
# /* DESCRIPTION */
# #ifndef VARIABLE
# #define VARIABLE VALUE
# #endif
#
# If VALUE is unspecified then VARIABLE will be defined to be empty.
# This macro should be used in conjunction with ACM_CONFIG_HEADER
# which defines the name of the config headers in which to output
# variables defined with this macro.
# ------------------------------------------------------------------
AC_DEFUN([ACM_DEFINE_PUBLIC],
[dnl
AC_BEFORE([$0],[ACM_CONFIG_HEADER])dnl

m4_if(m4_bregexp([$1],[^[A-Za-z_]+[A-Za-z0-9_]+$]),-1,
[AC_MSG_ERROR([Bad variable name '[$1]' supplied to [ACM_DEFINE_PUBLIC]])])

acm_public_macros="$acm_public_macros [$1]"
if test -z "$acm_public_macros_def"; then
    acm_public_macros_def="acm_public_macro_[$1]=\"[$2]\"; acm_public_macro_desc_[$1]=\"[$3]\""
else
    acm_public_macros_def="$acm_public_macros_def; acm_public_macro_[$1]=\"[$2]\"; acm_public_macro_desc_[$1]=\"[$3]\""
fi
])


# ACM_DEFINE_PUBLIC_STRING(VARIABLE,[VALUE],[DESCRIPTION])
#
# Causes VARIABLE to be defined as a literal string
# in a public config header, such as:
#
# /* DESCRIPTION */
# #ifndef VARIABLE
# #define VARIABLE "VALUE"
# #endif
#
# If VALUE is unspecified then VARIABLE will be defined to be empty.
# This macro should be used in conjunction with ACM_CONFIG_HEADER
# which defines the name of the config headers in which to output
# variables defined with this macro.
# ------------------------------------------------------------------
AC_DEFUN([ACM_DEFINE_PUBLIC_STRING],
[dnl
AC_BEFORE([$0],[ACM_CONFIG_HEADER])dnl

m4_if(m4_bregexp([$1],[^[A-Za-z_]+[A-Za-z0-9_]+$]),-1,
[AC_MSG_ERROR([Bad variable name '[$1]' supplied to [ACM_DEFINE_PUBLIC_STRING]])])

acm_public_strings="$acm_public_strings [$1]"
if test -z "$acm_public_strings_def"; then
    acm_public_strings_def="acm_public_string_[$1]=\"[$2]\"; acm_public_string_desc_[$1]=\"[$3]\""
else
    acm_public_strings_def="$acm_public_strings_def; acm_public_string_[$1]=\"[$2]\"; acm_public_string_desc_[$1]=\"[$3]\""
fi
])


# ACM_CONFIG_HEADER(NAME)
#
# This is an instantiating macro that should usually be included
# shortly before AC_OUTPUT.  If will create a config file containing
# the public symbols declared by ACM_DEFINE_PUBLIC
# -----------------------------------------------------------------
AC_DEFUN([ACM_CONFIG_HEADER],
[dnl
AC_REQUIRE([ACM_REQUIRE_LN_S])dnl
m4_if([$#],1,[],[AC_MSG_ERROR([[ACM_CONFIG_HEADER] must have only one parameter])])

dnl is there a way to disable this at m4 time if ACM_DEFINE_PUBLIC has never been used?

AC_CONFIG_COMMANDS([include/$1],
                   [
                    _SUBDIR="$(dirname [$1])/"
                    if test "$_SUBDIR" = "./"; then
                        _SUBDIR=
                    fi
                    _TEMPFILE="include/.tempfile"

                    _GUARD="_MAKEFILE_PLATFORM_$(echo $package_name | tr "a-z .-" "A-Z___")_CONF_H"

                    cat > $_TEMPFILE <<EOF
/* Makeup ${makeup_version} generated [$1] for ${package_name} ${package_version}
 * Do not edit this file directly, your changes will be lost.
 * Copyright 2003 - 2021, Ron <ron@debian.org>
 *
 * This file is distributed under the terms of the GNU GPL version 2.
 *
 * As a special exception to the GPL, it may be distributed without
 * modification as a part of a program using a makeup generated build
 * system, under the same distribution terms as the program itself.
 */

#ifndef ${_GUARD}
#define ${_GUARD}

// Guard for POSIX dependent code
#if defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))
    #if (EM_PLATFORM_POSIX != 1)
	#define EM_PLATFORM_POSIX 1
    #endif

    // Guard for Linux kernel dependent code
    #if defined(__linux__)
	#if linux == 1
	    #define SAVE_linux
	    #undef linux
	#elif defined(linux)
	    #warning Macro 'linux' is defined to a value other than 1
	#endif

	#if (EM_PLATFORM_LINUX != 1)
	    #define EM_PLATFORM_LINUX 1
	#endif
	#define EM_PLATFORM__ linux

    #else

	// Guard for BSD dependent code
	#include <sys/param.h>
	#if defined(BSD) || defined(__FreeBSD_kernel__)
	    #if (EM_PLATFORM_BSD != 1)
		#define EM_PLATFORM_BSD 1
	    #endif
	    #define EM_PLATFORM__ bsd
	#endif

	// Guard for MacOSX dependent code
	#if defined(__APPLE__) && defined(__MACH__) && (EM_PLATFORM_MAC != 1)
	    #define EM_PLATFORM_MAC 1
	#endif

    #endif
#endif

// Guard for Windows dependent code
#if defined(_WIN32)
    #if (EM_PLATFORM_MSW != 1)
        #define EM_PLATFORM_MSW 1
    #endif
    #define EM_PLATFORM__ msw
#endif

#ifndef EM_PLATFORM__
    #error Platform unrecognised.
#endif

// Feature override macro.
//
// You may define the value of this macro to specify a configuration
// other than the system default. 'd' will attempt to use a debug
// build, 'r' a release build.  Other flavour options may be defined
// by individual packages in their own configuration.
#ifndef EM_CONFIG_FLAVOUR
#define EM_CONFIG_FLAVOUR
#endif

#define EM_CAT(a,b) EM_CAT_(a,b)
#define EM_CAT_(a,b) a ## b

#define EM_CONFIG_HEADER <${__package_config_dir}EM_CAT(EM_PLATFORM__,EM_CONFIG_FLAVOUR)_${__package_config_public}>
#include EM_CONFIG_HEADER

#ifndef _${_GUARD}
#error Config header cannot be located
#endif

#undef EM_CAT
#undef EM_CAT_
#undef EM_PLATFORM__
#undef EM_CONFIG_HEADER
#ifdef SAVE_linux
    #define linux 1
    #undef SAVE_linux
#endif


// Compiler version tests.
//
// This macro will return false if the version of gcc in use
// is earlier than the specified major, minor limit, or if gcc
// is not being used.  Otherwise it will evaluate to be true.
// This will also be true for the clang compiler, for whatever
// GCC version it is pretending to be compatible with.
#if defined(__GNUC__) && defined(__GNUC_MINOR__)
 #define EM_COMPILER_GCC( major, minor )   ( ( __GNUC__ > (major) )         \\
            || ( __GNUC__ == (major) && __GNUC_MINOR__ >= (minor) ) )
#else
 #define EM_COMPILER_GCC( major, minor )  0
#endif

// As above, except for the clang compiler instead.
#if defined(__clang_major__) && defined(__clang_minor__)
 #define EM_COMPILER_CLANG( major, minor )  ( ( __clang_major__ > (major) ) \\
            || ( __clang_major__ == (major) && __clang_minor__ >= (minor) ) )
#else
 #define EM_COMPILER_CLANG( major, minor )  0
#endif

#endif  // ${_GUARD}

EOF

                    if diff --brief include/[$1] $_TEMPFILE > /dev/null 2>&1; then
                        AC_MSG_NOTICE([[$1] is unchanged])
                        rm $_TEMPFILE
                    else
                        mv $_TEMPFILE include/[$1]
                    fi

                    echo "/* Makeup generated $_SUBDIR$config_flavour */" > $_TEMPFILE
                    echo                                                >> $_TEMPFILE
                    echo "#ifndef _${_GUARD}"                           >> $_TEMPFILE
                    echo "#define _${_GUARD}"                           >> $_TEMPFILE
                    echo                                                >> $_TEMPFILE
                    for m in $acm_public_macros; do
                        eval echo "/\* \$acm_public_macro_desc_$m \*/"  >> $_TEMPFILE
                             echo "#ifndef $m"                          >> $_TEMPFILE
                        eval echo "\#define $m \$acm_public_macro_$m"   >> $_TEMPFILE
                             echo "#endif"                              >> $_TEMPFILE
                             echo                                       >> $_TEMPFILE
                    done

                    for s in $acm_public_strings; do
                        eval echo "/\* \$acm_public_string_desc_$s \*/" >> $_TEMPFILE
                             echo "#ifndef $s"                          >> $_TEMPFILE
                        eval echo "\#define $s \\\"\$acm_public_string_$s\\\"" >> $_TEMPFILE
                             echo "#endif"                              >> $_TEMPFILE
                             echo                                       >> $_TEMPFILE
                    done
                    echo "#endif  // _${_GUARD}"                        >> $_TEMPFILE

                    if diff --brief include/$_SUBDIR$config_flavour $_TEMPFILE > /dev/null 2>&1; then
                        AC_MSG_NOTICE([$_SUBDIR$config_flavour is unchanged])
                        rm $_TEMPFILE
                    else
                        mv $_TEMPFILE include/$_SUBDIR$config_flavour
                    fi

                    ( cd include/$_SUBDIR
                      if test ! -e $config_platform; then
                        $LN_S $config_flavour $config_platform
                      fi
                    )
                   ],[
                    acm_public_macros="$acm_public_macros"
                    $acm_public_macros_def
                    acm_public_strings="$acm_public_strings"
                    $acm_public_strings_def
                    LN_S="$LN_S"
                    config_platform="$MAKEUP_PLATFORM_HEADER"
                    config_flavour="$MAKEUP_FLAVOUR_HEADER"
                   ])
])


# ACM_CPP_PUSH_POP_DIAGNOSTIC_MACROS
#
# Define a set of CPP macros for local tweaking of 'GCC diagnostic' settings
# around Special Snowflake code.  Mostly these are used for suppressing some
# incorrect diagnostic, but they can also be used to locally add additional
# diagnostics around code that might require that.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_CPP_PUSH_POP_DIAGNOSTIC_MACROS],
[dnl
AC_MSG_NOTICE([Including EM_PUSH/POP_DIAGNOSTIC preprocessor macros ...])
AH_VERBATIM([DIAGNOSTIC_PUSH_POP],
[
/* Safely stringify a pragma option (which may already include quotes) */
#define EM_PRAGMA(p) _Pragma (#p)


/* Save the current diagnostic settings, and try to add or modify some
   diagnostic Option to have the given Action.  Action may be one of:
   error, warning, or ignored, setting the new disposition of Option.

   If Option is not recognised by the compiler this request will be
   silently ignored.  Clang-6 stopped using -Wunknown-pragmas (which was
   implied by -Wpragmas) for these, so we need to silence another option
   for it (which in turn still needs -Wpragmas, as GCC of course doesn't
   support clang's new -Wunknown-warning-option. Cooperation is hard ...
 */
#define EM_TRY_PUSH_DIAGNOSTIC( Action, Option )	        \
 EM_PRAGMA(GCC diagnostic push)				        \
 EM_PRAGMA(GCC diagnostic ignored "-Wpragmas")		        \
 EM_PRAGMA(GCC diagnostic ignored "-Wunknown-warning-option")	\
 EM_PRAGMA(GCC diagnostic Action Option)

/* Save the current diagnostic settings, and try to add or modify some
   diagnostic Option to have the given Action.  Action may be one of:
   error, warning, or ignored, setting the new disposition of Option.

   If Option is not recognised by the compiler this request may itself
   generate a compile time diagnostic warning or error depending on
   the compiler defaults and command line options used.
 */
#define EM_PUSH_DIAGNOSTIC( Action, Option )		\
 EM_PRAGMA(GCC diagnostic push)				\
 EM_PRAGMA(GCC diagnostic Action Option)

/* Set the Action for additional diagnostic Options.  The current settings
   are not pushed, so a call to EM_POP_DIAGNOSTIC will revert all changes
   made since the last time EM_PUSH_DIAGNOSTIC was used.
 */
#define EM_MORE_DIAGNOSTIC( Action, Option )		\
 EM_PRAGMA(GCC diagnostic Action Option)
]
m4_foreach([action],[[IGNORE,ignored],[WARN,warning],[ERROR,error]],
[
/* Equivalent to EM_TRY_PUSH_DIAGNOSTIC( m4_shift(action), Option ) */
@%:@define EM_TRY_PUSH_DIAGNOSTIC_[]m4_car(action)( Option )		\
 EM_TRY_PUSH_DIAGNOSTIC( m4_shift(action), Option )

/* Equivalent to EM_PUSH_DIAGNOSTIC( m4_shift(action), Option ) */
@%:@define EM_PUSH_DIAGNOSTIC_[]m4_car(action)( Option )		\
 EM_PUSH_DIAGNOSTIC( m4_shift(action), Option )

/* Equivalent to EM_MORE_DIAGNOSTIC( m4_shift(action), Option ) */
@%:@define EM_MORE_DIAGNOSTIC_[]m4_car(action)( Option )		\
 EM_MORE_DIAGNOSTIC( m4_shift(action), Option )

])dnl
[
/* Restore the diagnostic state to what it was before the last time it was
   pushed.  If there is no corresponding push the command-line options are
   restored.
 */
#define EM_POP_DIAGNOSTIC				\
 EM_PRAGMA(GCC diagnostic pop)
])dnl
])


# __ACM_ADD_COMPILER_OPTION([FLAGS_PREFIX],[OPTION])
#
# Implementation of _ACM_ADD_COMPILER_OPTION for doing the individual tests of
# each of the specified OPTIONS. The correct default language should already be
# set, so here we test if the OPTION is supported, caching the result of that
# test in AS_TR_SH'ified mu_cv_${FLAGS_PREFIX}_flag_${OPTION}, and appending
# any supported options to ${FLAGS_PREFIX}FLAGS.
# ------------------------------------------------------------------------------
AC_DEFUN([__ACM_ADD_COMPILER_OPTION],
[dnl
ACM_PUSH_VAL([$0],[$1FLAGS],[$2])dnl
AS_VAR_PUSHDEF([cachevar],[mu_cv_$1_flag_$2])dnl
dnl We need to special case C => $CC here, but CXX => $CXX can be implicit.
AC_CACHE_CHECK([if m4_case([$1],[C],[$CC],[$$1]) supports $2],[cachevar],
               [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]],[[]])],
                                  [AS_VAR_SET([cachevar],[yes])],
                                  [AS_VAR_SET([cachevar],[no])]
                                 )
              ])
AS_VAR_IF([cachevar],[yes],[],[ACM_POP_VAR([$0],[$1FLAGS])])dnl
AS_VAR_POPDEF([cachevar])dnl
])


# _ACM_ADD_COMPILER_OPTION([FLAGS_PREFIX],[OPTIONS])
#
# Implementation of ACM_ADD_COMPILER_OPTION for doing the individual tests with
# each of the specified FLAGS_PREFIXES. This will temporarily switch the default
# language based on FLAGS_PREFIX, then test if each of the comma-separated list
# of OPTIONS is supported for that language.
# ------------------------------------------------------------------------------
AC_DEFUN([_ACM_ADD_COMPILER_OPTION],
[dnl
ACM_PUSH_LANG_FOR_FLAGS([$0],[$1])
ACM_FOREACH([compiler_opt],[$2],[_$0([$1],m4_dquote(m4_expand([compiler_opt])))])
ACM_POP_LANG_FOR_FLAGS([$0],[$1])
])


# ACM_ADD_COMPILER_OPTION([FLAGS_PREFIXES],[OPTIONS])
#
# For each combination of the comma-separated FLAGS_PREFIXES and OPTIONS check
# if the corresponding compiler supports that option, and if it does add it to
# the *FLAGS for that language for use when compiling subsequent source.
#
# Currently supported values for FLAGS_PREFIXES are C and CXX (use [C,CXX] to
# test and set options for both the C and C++ compiler).  The FLAGS_PREFIXES
# must be a literal string - but the OPTIONS may be either literals, a shell
# expression which expands to a comma-separated list, or a mixture of both.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_ADD_COMPILER_OPTION],
[dnl
dnl We can't use ACM_FOREACH here, because AC_LANG_PUSH only takes literals.
m4_foreach([lang],[$1],[_$0(lang,[$2])])
])


# _ACM_COMPILER_WERROR_UNKNOWN_WARNING_OPTION([FLAGS_PREFIX],[COMPILER_VAR])
#
# Check if the compiler considers unknown warning options to be an error
# by default, or if it needs an explicit extra option passed to do so.
# This macro is an implementation detail required by ACM_ADD_COMPILER_WARNING.
#
# Testing whether a warning option is supported can be tricky.  By default
# GCC will consider -Wfoo to be an error if 'foo' is an unknown warning,
# but it will not even emit a diagnostic for -Wno-foo unless some other
# diagnostic message is also triggered, in which case it will merely warn
# that an unrecognised option is also present.
#
# With the clang toolchain the behaviour in both cases is controlled by an
# explicit option: -Wunknown-warning-option, which is enabled by default.
# If that option is negated then no diagnostic is output, otherwise unknown
# warning options of either polarity will simply emit a warning.  If we want
# to test whether a warning option is supported then we need to explicitly
# add unknown-warning-option to the -Werror set to provoke a test failure
# if it is not.
#
# The FLAGS_PREFIX determines which language will be tested and so which of
# the *FLAGS variables the test options will be added to.  Supported values
# are currently C and CXX.
#
# The COMPILER_VAR is only used to report the toolchain being tested, so for
# C it should be [$CC] and for C++ it should be [$CXX].  This parameter is
# passed as a convenience, since there is no strictly consistent rule which
# maps all the related identifiers for a language together, and the caller
# should already know it, so we don't need extra logic here to look it up.
#
# The output variable ACM_${FLAGS_PREFIX}_WARNINGFAIL will be set to either
# an empty string or the additional option(s) which need to be set in the
# relevant *FLAGS when testing whether some warning option is supported.
#
# This macro shouldn't normally be invoked directly, instead the language
# specific wrappers which don't need options (but whose name can still be
# constructed from other macros) should be AC_REQUIRE'd before the output
# variable is needed for the first time.
# ----------------------------------------------------------------------------
AC_DEFUN([_ACM_COMPILER_WERROR_UNKNOWN_WARNING_OPTION],
[dnl
ACM_PUSH_LANG_FOR_FLAGS([$0],[$1])dnl
ACM_PUSH_VAL([$0],[$1FLAGS],[-Womg-wtf-not-an-option])dnl
ACM_$1_WARNINGFAIL=""

AC_CACHE_CHECK([if $2 unknown warning options are errors],[mu_cv_$1_flag_uwo],
               [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]],[[]])],
                                  [mu_cv_$1_flag_uwo=no],
                                  [mu_cv_$1_flag_uwo=yes]
                                 )
              ])
AS_IF([test "$mu_cv_$1_flag_uwo" = no],[
      ACM_REPUSH_VAL([$0],[$1FLAGS],[-Werror=unknown-warning-option,-Womg-wtf-not-an-option])
      AC_CACHE_CHECK([if $2 supports -Werror=unknown-warning-option],[mu_cv_$1_flag_werror_uwo],
                     [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]],[[]])],
                                        [mu_cv_$1_flag_werror_uwo=no],
                                        [mu_cv_$1_flag_werror_uwo=yes]
                                       )
                    ])
      dnl It should be safe to fail open here.  If we haven't figured out how to make the
      dnl compiler fail when passed an unknown warning option, then it should be relatively
      dnl safe to let tests default to passing them anyway.  At best, they will actually
      dnl work as intended, and at worst it might make a lot of noise spitting out non-fatal
      dnl warning diagnostics about not liking them - but it shouldn't break the build.
      dnl We bark a warning here so that this test can be improved further if that occurs,
      dnl and err on the side of including rather than excluding extra warnings.
      AS_IF([test "$mu_cv_$1_flag_werror_uwo" = yes],
            [ACM_$1_WARNINGFAIL="-Werror=unknown-warning-option"],
            [AC_MSG_WARN([Don't know how to make $2 fail with unknown warning options,])
             AC_MSG_WARN([so later tests may (wrongly) decide to pass them to it anyway.])])
     ])
ACM_POP_VAR([$0],[$1FLAGS])dnl
ACM_POP_LANG_FOR_FLAGS([$0],[$1])dnl
])


# _ACM_C_WERROR_UNKNOWN_WARNING_OPTION
#
# C language wrapper for _ACM_COMPILER_WERROR_UNKNOWN_WARNING_OPTION
# which can be AC_REQUIRE'd.
# -------------------------------------------------------------------
AC_DEFUN([_ACM_C_WERROR_UNKNOWN_WARNING_OPTION],
[dnl
_ACM_COMPILER_WERROR_UNKNOWN_WARNING_OPTION([C],[$CC])
])


# _ACM_CXX_WERROR_UNKNOWN_WARNING_OPTION
#
# C++ language wrapper for _ACM_COMPILER_WERROR_UNKNOWN_WARNING_OPTION
# which can be AC_REQUIRE'd.
# ---------------------------------------------------------------------
AC_DEFUN([_ACM_CXX_WERROR_UNKNOWN_WARNING_OPTION],
[dnl
_ACM_COMPILER_WERROR_UNKNOWN_WARNING_OPTION([CXX],[$CXX])
])


# __ACM_ADD_COMPILER_WARNING([FLAGS_PREFIX],[WARNING_OPTION])
#
# Implementation of _ACM_ADD_COMPILER_WARNING for doing the individual tests of
# each of the specified WARNING_OPTIONS. The correct default language should be
# already set, so here we test if -W${WARNING_OPTION} is supported, caching the
# result of that test in mu_cv_${FLAGS_PREFIX}_flag_${CACHE_VAR_SUFFIX}, and
# appending any supported warning options to ${FLAGS_PREFIX}FLAGS.
# ------------------------------------------------------------------------------
AC_DEFUN([__ACM_ADD_COMPILER_WARNING],
[dnl
ACM_PUSH_VAL([$0],[$1FLAGS],[$ACM_$1_WARNINGFAIL])dnl
__ACM_ADD_COMPILER_OPTION([$1],[-W$2])
AS_VAR_PUSHDEF([cachevar],[mu_cv_$1_flag_-W$2])dnl
AS_VAR_IF([cachevar],[yes],[ACM_REPUSH_VAL([$0],[$1FLAGS],[-W$2])],
                           [ACM_POP_VAR([$0],[$1FLAGS])])
AS_VAR_POPDEF([cachevar])dnl
])


# _ACM_ADD_COMPILER_WARNING([FLAGS_PREFIX],[WARNING_OPTIONS])
#
# Implementation of ACM_ADD_COMPILER_WARNING for doing the individual tests with
# each of the specified FLAGS_PREFIXES. This will temporarily switch the default
# language based on FLAGS_PREFIX, then test if each of the comma-separated list
# of WARNING_OPTIONS is supported for that language.
# ------------------------------------------------------------------------------
AC_DEFUN([_ACM_ADD_COMPILER_WARNING],
[
dnl We do this check before the AC_REQUIRE below, because the most likely cause
dnl of this failing is a typo in user code invoking ACM_ADD_COMPILER_WARNING
dnl and this gives a more user friendly warning pointing to the correct place
dnl when the m4 is being processed by aclocal/autom4te, rather than having that
dnl propagate deeper into the implementation detail before being caught.
dnl
dnl But it will not change the default toolchain for the invocation of the
dnl requirement, since that gets expanded outside of the scope of the push/pop
dnl used here, so it will need to do this itself as well to be run with the
dnl correct toolchain.
ACM_PUSH_LANG_FOR_FLAGS([$0],[$1])dnl
AC_REQUIRE([_ACM_$1_WERROR_UNKNOWN_WARNING_OPTION])dnl
ACM_FOREACH([warning_opt],[$2],[_$0([$1],m4_dquote(m4_expand([warning_opt])))])
ACM_POP_LANG_FOR_FLAGS([$0],[$1])dnl
])


# ACM_ADD_COMPILER_WARNING([FLAGS_PREFIXES],[WARNING_OPTIONS])
#
# This is a specialisation of ACM_ADD_COMPILER_OPTION which handles the extra
# hoops we need to jump through to test whether particular warning options are
# actually supported or not (as opposed to ignoring unknown warning options).
#
# For each combination of the comma-separated FLAGS_PREFIXES and WARNING_OPTIONS
# check if the corresponding compiler supports -W${WARNING_OPTION} and if it does
# add it to the *FLAGS for that language for use when compiling subsequent source.
#
# Currently supported values for FLAGS_PREFIXES are C and CXX (use [C,CXX] to
# test and set options for both the C and C++ compiler).  The FLAGS_PREFIXES
# must be a literal string - but the OPTIONS may be either literals, a shell
# expression which expands to a comma-separated list, or a mixture of both.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_ADD_COMPILER_WARNING],
[dnl
dnl We can't use ACM_FOREACH here, because AC_LANG_PUSH only takes literals.
m4_foreach([lang],[$1],[_$0(lang,[$2])])
])


# __ACM_ADD_COMPILER_WARNING_QUIETLY([FLAGS_PREFIX],[WARNING_OPTION])
#
# Implementation of ACM_ADD_COMPILER_WARNING_QUIETLY which does the real work
# of checking the cache var for some FLAGS_PREFIX and WARNING_OPTION pair, and
# delegating to ACM_ADD_COMPILER_WARNING if it is not already set.
# ----------------------------------------------------------------------------
AC_DEFUN([__ACM_ADD_COMPILER_WARNING_QUIETLY],
[dnl
dnl There is no AS_VAR_CASE so it's nested IF instead of AS_CASEing a temp var.
AS_VAR_PUSHDEF([cachevar],[mu_cv_$1_flag_-W$2])dnl
AS_VAR_IF([cachevar],[yes],[ACM_ADD_OPT([$1FLAGS],[-W$2])],
                     [AS_VAR_IF([cachevar],[no],[],
                                           [ACM_ADD_COMPILER_WARNING([$1],[$2])])])dnl
AS_VAR_POPDEF([cachevar])
])


# _ACM_ADD_COMPILER_WARNING_QUIETLY([FLAGS_PREFIX],[WARNING_OPTIONS])
#
# Implementation of ACM_ADD_COMPILER_WARNING_QUIETLY for doing the individual
# tests with each of the specified WARNING_OPTIONS for a given FLAGS_PREFIX.
# ----------------------------------------------------------------------------
AC_DEFUN([_ACM_ADD_COMPILER_WARNING_QUIETLY],
[dnl
ACM_FOREACH([warning_opt],[$2],[_$0([$1],m4_dquote(m4_expand([warning_opt])))])dnl
])


# ACM_ADD_COMPILER_WARNING_QUIETLY([WARNING_OPTIONS],[FLAGS_PREFIXES])
#
# This is a specialisation of ACM_ADD_COMPILER_WARNING which short circuits
# the normal AC_CACHE_CHECK to first test the cache variable directly.  There
# are some tests (such as for attribute support) where we need to temporarily
# add warning (and -Werror=) options to get a correct result about whether
# what we are testing is supported or not, and sometimes the warning options
# that we need to add are not universally supported and also need to be tested
# themselves before being used.
#
# This macro avoids littering the configure output with repeated reports about
# 'checking if $COMPILER supports $WARNING_OPTION... (cached)'
# for each real option test that we want to perform.  It will only output the
# check for the supplementary WARNING_OPTIONS the first time it is performed,
# and after that it will just silently add (or not) the WARNING_OPTIONS to the
# desired set of FLAGS variables.
#
# If the cache variable for an option is not already set to 'yes' or 'no',
# this macro behaves the same as a direct call to ACM_ADD_COMPILER_WARNING.
#
# If FLAGS_PREFIXES are not specified explicitly, it will default to using the
# normal compiler {C,CXX,etc}FLAGS for the currently AC_LANG_PUSH'ed language.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_ADD_COMPILER_WARNING_QUIETLY],
[dnl
dnl We can't use ACM_FOREACH here, because AC_LANG_PUSH only takes literals.
m4_foreach([lang],m4_default_quoted([$2],[ACM_LANG_PREFIX]),[_$0(lang,[$1])])dnl
])


# _ACM_ADD_LINKER_OPTION([OPTION])
#
# Implementation of ACM_ADD_LINKER_OPTION for doing the individual tests for
# each OPTION.  The result is cached in AS_TR_SH'ified mu_cv_ldflag_${OPTION}
# and supported options are appended to LDFLAGS.
# ----------------------------------------------------------------------------
AC_DEFUN([_ACM_ADD_LINKER_OPTION],
[
ACM_PUSH_VAL([$0],[LDFLAGS],[$1])dnl
AS_VAR_PUSHDEF([cachevar],[mu_cv_ldflag_$1])dnl
AC_CACHE_CHECK([if linker supports $1],[cachevar],
               [AC_LINK_IFELSE([AC_LANG_PROGRAM([[]],[[]])],
                               [AS_VAR_SET([cachevar],[yes])],
                               [AS_VAR_SET([cachevar],[no])]
                              )
              ])
AS_VAR_IF([cachevar],[yes],[],[ACM_POP_VAR([$0],[LDFLAGS])])dnl
AS_VAR_POPDEF([cachevar])dnl
])


# ACM_ADD_LINKER_OPTION([OPTIONS])
#
# For each of the comma-separated OPTIONS, check if the linker supports that
# option (using the current AC_LANG toolchain) and if it does then add it to
# LDFLAGS for use when compiling subsequent source.  The OPTIONS may be either
# literals, a shell expression which expands to a comma-separated list, or a
# mixture of both.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_ADD_LINKER_OPTION],
[dnl
ACM_FOREACH([linker_opt],[$1],[_$0(m4_dquote(m4_expand([linker_opt])))])
])


# ACM_ADD_COMPILE_LINK_OPTION([FLAGS_PREFIXES],[OPTIONS])
#
# For each combination of the comma-separated FLAGS_PREFIXES and OPTIONS check
# if the corresponding compiler and linker supports that option.  If so, then
# it will be added to both ${FLAGS_PREFIX}FLAGS and LDFLAGS.  The result of
# compiler tests will be cached in mu_cv_${FLAGS_PREFIX}_flag_${OPTION}, and
# the linker test, if performed, in mu_cv_ldflag_${OPTION}, converted to valid
# shell variable names by AS_TR_SH.  The linker test will only be performed
# using the toolchain of the first of the FLAGS_PREFIXES, and only if the test
# with that compiler succeeded.
#
# This macro is mostly useful for the case where some option needs to be passed
# to both the compiler and linker to operate correctly.
#
# Currently supported values for FLAGS_PREFIXES are C and CXX (use [C,CXX] to
# test and set options for both the C and C++ compiler).  The FLAGS_PREFIXES
# must be a literal string - but the OPTIONS may be either literals, a shell
# expression which expands to a comma-separated list, or a mixture of both.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_ADD_COMPILE_LINK_OPTION],
[dnl
m4_pushdef([flagvars],m4_combine([,],[$1],[],[FLAGS]))dnl
ACM_FOREACH([compile_link_opt],[$2],
            [ACM_PUSH_VAR([$0],[flagvars])dnl
             ACM_ADD_COMPILER_OPTION([$1],[compile_link_opt])dnl
             AS_VAR_PUSHDEF([compvar],[mu_cv_[]m4_car($1)[]_flag_[]compile_link_opt])dnl
             AS_VAR_IF([compvar],[yes],
                       [ACM_PUSH_LANG_FOR_FLAGS([$0],m4_car($1))dnl
                        _ACM_ADD_LINKER_OPTION(m4_dquote(m4_expand([compile_link_opt])))
                        AS_VAR_PUSHDEF([linkvar],[mu_cv_ldflag_[]compile_link_opt])dnl
                        AS_VAR_IF([linkvar],[no],[ACM_POP_VAR([$0],[flagvars])])
                        AS_VAR_POPDEF([linkvar])
                        ACM_POP_LANG_FOR_FLAGS([$0],m4_car($1))dnl
                       ],
                       [ACM_POP_VAR([$0],[flagvars])]dnl
                       )
             AS_VAR_POPDEF([compvar])dnl
            ])
m4_popdef([flagvars])dnl
])


# _ACM_ADD_SANITIZER([SANITIZER])
#
# Implementation of ACM_ADD_SANITIZER for doing the individual tests of each
# requested SANITIZER.  It will test both the C and C++ toolchains and update
# LDFLAGS if it can be enabled.  Right now, that does assume that if it works
# for one it will work for the other too, because we don't have separate C or
# C++ LDFLAGS for projects which use both.
#
# This doesn't handle the case of the thread sanitiser with older toolchains,
# which require explicit additional options to build the position independent
# executables it needs to work.  With newer compilers, enabling it should do
# that automatically, if they don't already default to using PIE anyway.  We
# can't easily add those options here, because we don't have option variables
# that are specific to libraries or executables, and we need to use different
# options for this depending on which we are building.  If we ever really do
# need this with older toolchains we can look at splitting those further too.
# ----------------------------------------------------------------------------
AC_DEFUN([_ACM_ADD_SANITIZER],
[
ACM_ADD_COMPILER_OPTION([C,CXX],[-fsanitize=$1])

dnl If we added it to C/CXXFLAGS, we need to add it to LDFLAGS too.
dnl And we special case for the sanitisers where we know they need,
dnl or would benefit from, some additional compiler options.
AS_VAR_PUSHDEF([cachevar],[mu_cv_C_flag_-fsanitize=$1])dnl
AS_VAR_IF([cachevar],[yes],[
    ACM_ADD_OPT([LDFLAGS],[-fsanitize=$1])
    AS_CASE([$1],
            [address|memory|undefined],
            [ACM_ADD_UNIQUE([CFLAGS,CXXFLAGS],[-fno-omit-frame-pointer])]
           )dnl
])dnl
AS_VAR_POPDEF([cachevar])dnl
])


# ACM_ADD_SANITIZER([SANITIZERS])
#
# Test which of the comma-separated list of SANITIZERS are supported, and add
# the necessary compile and link time options to enable them.  You should list
# them in order of preference, as some of them may preclude the use of others.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_ADD_SANITIZER],
[
ACM_FOREACH([san_type],[$1],[_$0(san_type)])
])


# ACM_SUPPRESS_LSAN([ID])
#
# There are some autoconf tests which leak memory when run, and so will fail
# if the LeakSanitiser is active (the AM_ICONV and AC_FUNC_MMAP macros are
# known offenders, and there are probably more).  This macro can be used to
# temporarily disable the LSan checking while running those tests.  The ID is
# a local identifier to use when saving the previous state, so that calls to
# this may be nested if needed.  It should only contain characters that are
# safe to use in a shell variable name.  To restore the original state again
# use ACM_RESTORE_LSAN with the same ID as was passed here.  If nested, the
# IDs must be restored in the reverse order that they were suppressed in.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_SUPPRESS_LSAN],
[dnl
AS_VAR_PUSHDEF([asan],[mu_cv_C_flag_-fsanitize=address])dnl
AS_VAR_PUSHDEF([lsan],[mu_cv_C_flag_-fsanitize=leak])dnl

mu_lsan_enabled=no
AS_VAR_IF([asan],[yes],[mu_lsan_enabled=yes])
AS_VAR_IF([lsan],[yes],[mu_lsan_enabled=yes])

AS_IF([test "$mu_lsan_enabled" = yes],[
    AS_VAR_SET_IF([mu_suppress_lsan_$1],
                  [AC_MSG_ERROR([LSan suppression for '$1' is already active])])

    AC_MSG_NOTICE([Disabling LSan for $1 ...])
    AS_IF([test -n "$ASAN_OPTIONS"],[
            AS_VAR_SET([mu_suppress_lsan_$1],[$ASAN_OPTIONS])
            export ASAN_OPTIONS="$ASAN_OPTIONS:detect_leaks=0"
          ],[
            AS_VAR_SET([mu_suppress_lsan_$1],[yes])
            export ASAN_OPTIONS="detect_leaks=0"
          ])
])

AS_VAR_POPDEF([lsan])dnl
AS_VAR_POPDEF([asan])dnl
])


# ACM_DEFINE_HAVE_SANITIZERS
#
# Provides EM_HAVE_ASAN and EM_HAVE_TSAN when those sanitisers are enabled.
# We probably could run configure time checks for this, but in theory it's
# easy enough to do this at compile time, so just squirt out the relevant
# macro tests to the config header.  This version will still work if the
# user just runs make with a different environment rather than reconfiguring
# with --enable-san or similar.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_DEFINE_HAVE_SANITIZERS],
[
AC_MSG_NOTICE([Including EM_HAVE_ASAN/TSAN preprocessor macros ...])
AH_VERBATIM([EM_HAVE_ASAN_TSAN],
[
#ifdef __has_feature
    #if __has_feature(address_sanitizer)
        #define EM_HAVE_ASAN 1
    #endif
#elif defined(__SANITIZE_ADDRESS__)
    /* This macro is defined if building with the -fsanitize=address option. */
    #define EM_HAVE_ASAN 1
#endif

#ifdef __has_feature
    #if __has_feature(thread_sanitizer)
        #define EM_HAVE_TSAN 1
    #endif
#elif defined(__SANITIZE_THREAD__)
    /* This macro is defined if building with the -fsanitize=thread option. */
    #define EM_HAVE_TSAN 1
#endif
])dnl
])


# ACM_RESTORE_LSAN([ID])
#
# Restore the LSan state which existed prior to ACM_SUPPRESS_LSAN([ID]) being
# called.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_RESTORE_LSAN],
[dnl
AS_VAR_SET_IF([mu_suppress_lsan_$1],[
                AC_MSG_NOTICE([Restoring LSan after $1 ...])
                AS_UNSET([ASAN_OPTIONS])
                AS_VAR_IF([mu_suppress_lsan_$1],[yes],[],
                          [AS_VAR_COPY([ASAN_OPTIONS],[mu_suppress_lsan_$1])
                           export ASAN_OPTIONS
                          ])
                AS_UNSET([mu_suppress_lsan_$1])
              ],[
                AS_IF([test "$mu_lsan_enabled" = yes],
                      [AC_MSG_ERROR([LSan restore requested, but suppression for '$1' is not active])])
              ])
])


# ACM_CHECK_ATTRIBUTE([TYPE],[ATTRIBUTE],[TEST-PROGRAM],[ACTION],[ACTION-OPTIONS...])
#
# Test if the current AC_LANG compiler supports ATTRIBUTE.  The attribute TYPE
# (function, variable, type, etc.) is used for informative messages, variable
# and CPP macro names, and by higer level macros for the selection of default
# options.  The ATTRIBUTE string should be the literal to use within the double
# parenthesis of an attibute declaration ie. __attribute__((ATTRIBUTE)).  For
# attributes with arguments of their own, the arguments used in the ATTRIBUTE
# may be placeholder names of the form which could be substituted by a
# function-like macro in user code.
#
# The TEST-PROGRAM should be created by AC_LANG_PROGRAM or be of the same form
# as if it was, and should make use of the ATTRIBUTE in the prologue or body
# as appropriate to test compiler support for it.  An ATTRIBUTE with arguments
# must use some representative values for those arguments in the test code.
#
# If an ACTION macro is passed, it will be expanded with 'cachevar' holding
# the (cached) compile test result, ATTR_ID holding an uppercased version of
# the ATTRIBUTE string with any non-alphanumeric characters transformed to be
# shell and CPP macro safe, and all of $@ passed to it verbatim as arguments.
# Any further ACTION-OPTIONS arguments are simply passed through to the ACTION
# macro, this macro does nothing with or to those.
#
# User code generally won't use this macro directly unless it has some very
# special case to handle.  The more specialised ACM_HAVE_* and ACM_DEFINE_*
# macros for each attribute type are more probably what you want as (unlike
# this macro which just tests for support) they define CPP macros that user
# code can test or employ directly to use any supported attributes.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_CHECK_ATTRIBUTE],
[dnl
dnl helper macro for building the extra_warnings list.
m4_pushdef([add_extra_warning],[m4_append([extra_warnings],]m4_dquote($[]1)[,[,])])dnl
dnl
dnl GCC 8.3.0 needs -Werror=attributes to make warnings about any unrecognised
dnl attribute fail at compilation time.
m4_pushdef([extra_warnings],[error=attributes])dnl
dnl
dnl But Clang 7.0.1 also needs -Werror=unknown-sanitizers to fail on unknown
dnl parameters to the no_sanitize() attribute.  And strictly it appears to need
dnl -Wunknown-attributes for other unknowns, but that is enabled by default and
dnl -Wattributes there implies -Wignored-attributes and -Wunknown-attributes.
dnl
dnl At the time of writing, no_sanitize* attributes are only applicable as a
dnl function attribute, but that may not always stay true, and this test is
dnl eliminated at macro expansion time if it's not needed, so leaving it here
dnl for now seems cheap, reasonable, and a small bet on possibly future proof.
m4_bmatch([$2],[no_sanitize.*],[add_extra_warning([error=unknown-sanitizers])])dnl
dnl
dnl Clang versions which don't support statement attributes (for fallthrough)
dnl complain about them in this way:
dnl warning: declaration does not declare anything [-Wmissing-declarations]
m4_if([$1],[statement],[add_extra_warning([error=missing-declarations])])dnl
dnl
ACM_PUSH_VAR([$0],[ACM_LANG_PREFIX[]FLAGS])dnl
ACM_ADD_COMPILER_WARNING_QUIETLY([extra_warnings])dnl

m4_popdef([extra_warnings],[add_extra_warning])dnl
dnl AS_VAR_PUSHDEF will mangle ATTRIBUTE with AS_TR_SH to make it shell-safe.
dnl We do a similar transform to the uppercased ATTR_ID for the CPP macro
dnl name, first stripping backslash, single, and double quotes, then replacing
dnl any other remaining non-alphanumeric characters with underscores.
dnl
dnl We don't use AS_TR_CPP for this as it has a very conservative opinion on
dnl what a 'literal' is in this context, and many valid expected characters
dnl in an attribute will cause it to select a runtime shell transform instead
dnl of an m4 literal expansion, but ATTRIBUTE should always be a literal here.
m4_pushdef([ATTR_ID],[m4_toupper(ACM_TR_SH_LITERAL($2))])dnl
AS_VAR_PUSHDEF([cachevar],[mu_cv_[]ACM_LANG_ABBREV[]_$1_attr_$2])dnl
AC_CACHE_CHECK([if ACM_LANG_COMPILER supports $1 attribute (($2))],[cachevar],
               [AC_COMPILE_IFELSE([$3],
                                  [AS_VAR_SET([cachevar],[yes])],
                                  [AS_VAR_SET([cachevar],[no])])
               ])
m4_ifval([$4],[$4($@)])dnl
ACM_POP_VAR([$0],[ACM_LANG_PREFIX[]FLAGS])dnl
AS_VAR_POPDEF([cachevar])dnl
m4_popdef([ATTR_ID])dnl
])


# _acm_attribute_types, _ACM_ATTRIBUTE_TYPES
#
# The list of expected attribute types that we specialise macros for.
# for each attribute type in this list we define:
#
#   ACM_CHECK_[TYPE]_ATTRIBUTE
#   ACM_HAVE_[TYPE]_ATTRIBUTE
#   ACM_DEFINE_[TYPE]_ATTRIBUTE
#
# Any additions to this list should also define a suitable default test for
# that type of attribute in an _acm_check_[type]_attr_prologue macro below
# (and/or _acm_check_[type]_attr_body as needed).
# ----------------------------------------------------------------------------
m4_define([_acm_attribute_types],[function,type,variable,label,enum,statement])
m4_define([_ACM_ATTRIBUTE_TYPES],m4_toupper(m4_dquote(_acm_attribute_types)))


# _acm_check_[type]_attr_prologue([ATTRIBUTE])
#
# ACM_CHECK_ATTRIBUTE TEST-PROGRAM prologue snippets for the default test used
# by each attribute TYPE.  It may not be suitable for all possible attributes
# of that TYPE, but it should Just Work for as many of the most commonly used
# ones as possible.
#
# These will be used if the TEST-PROLOGUE argument is not explicitly specified
# when the ACM_CHECK_[TYPE]_ATTRIBUTE macros are expanded.  If no prologue
# snippet is defined for the TYPE in question, the prologue (before main() in
# the test code) will be empty by default.
# ----------------------------------------------------------------------------
dnl ----- Test for function attributes ---------------------------------------
m4_define([_acm_check_function_attr_prologue],
[dnl
dnl We need the void parameter declaration for C, or the compiler will not consider
dnl this a function prototype, which can be significant for testing some attributes
dnl including those like 'format' which needs to check the function arguments.
AC_LANG_CASE([C],[[int f(void) __attribute__(($1));]],
                 [[int f() __attribute__(($1));]])dnl
])

dnl ----- Test for type attributes -------------------------------------------
m4_define([_acm_check_type_attr_prologue],
          [[struct S { int i; } __attribute__(($1));]])

dnl ----- Test for variable attributes ---------------------------------------
m4_define([_acm_check_variable_attr_prologue],
          [[char c[4] __attribute__(($1));]])

dnl ----- Test for label attributes ------------------------------------------
m4_define([_acm_check_label_attr_prologue],
[dnl
void f(int i) {
  Again:
  __attribute__(($1));
  while (i++ < 10) goto Again;
}dnl
])

dnl ----- Test for enumerator attributes -------------------------------------
m4_define([_acm_check_enum_attr_prologue],
[dnl
enum E {
  A __attribute__(($1)),
  B __attribute__(($1))
};dnl
])

dnl ----- Test for null statement attributes ---------------------------------
m4_define([_acm_check_statement_attr_prologue],
[dnl
int f(int i);
int f(int i) {
  switch(i) {
  case 1:
        __attribute__(($1));
  case 2:
        return i;
  }
  return 0;
}dnl
])

# _acm_check_[type]_attr_body([ATTRIBUTE])
#
# ACM_CHECK_ATTRIBUTE TEST-PROGRAM body snippets for the default test used by
# each attribute TYPE.  It may not be suitable for all possible attributes of
# that TYPE, but it should Just Work for as many of the most commonly used
# ones as possible.
#
# These will be used if the TEST-BODY argument is not explicitly specified
# when the ACM_CHECK_[TYPE]_ATTRIBUTE macros are expanded.  If no body snippet
# is defined for the TYPE in question, the body (inside main() in the test
# code) will be empty by default.
# ----------------------------------------------------------------------------
dnl In practice we don't need any body code for the default type tests (yet).
dnl A test program 'body' for 'statement' attributes can be defined like this:
dnl m4_define([_acm_check_statement_attr_body],[[f(10);]])


# ACM_CHECK_[TYPE]_ATTRIBUTE([ATTRIBUTE],[TEST-PROLOGUE],[TEST-BODY],[ACTION],[ACTION-OPTIONS...])
#
# Specialisation of ACM_CHECK_ATTRIBUTE for each attribute TYPE that attrtype
# loops over here.  Imports the (overridable) default test prologue and body
# for each type if an _acm_check_[type]_attr_{prologue,body} macro is defined.
#
# Defines the following specialised convenience macros:
#
# ACM_CHECK_FUNCTION_ATTRIBUTE
# ACM_CHECK_TYPE_ATTRIBUTE
# ACM_CHECK_VARIABLE_ATTRIBUTE
# ACM_CHECK_LABEL_ATTRIBUTE
# ACM_CHECK_ENUM_ATTRIBUTE
# ACM_CHECK_STATEMENT_ATTRIBUTE
#
# Like ACM_CHECK_ATTRIBUTE, user code generally won't need to use these macros
# directly and will use the more specialised ACM_HAVE_* and ACM_DEFINE_* which
# provide an ACTION to define CPP macros in the configuration header.
# ----------------------------------------------------------------------------
m4_foreach([attrtype],[_acm_attribute_types],[dnl
AC_DEFUN([ACM_CHECK_]m4_toupper(attrtype)[_ATTRIBUTE],
[dnl
m4_pushdef([attrtest],[_acm_check_]]attrtype[[_attr])dnl
m4_pushdef([prologue],[m4_ifdef(attrtest[_prologue],attrtest[_prologue($1)])])dnl
m4_pushdef([body],    [m4_ifdef(attrtest[_body],    attrtest[_body($1)])])dnl
ACM_CHECK_ATTRIBUTE(]attrtype[,[$1],
                    [AC_LANG_PROGRAM([m4_default([$2],[prologue])],
                                     [m4_default([$3],[body])])dnl
                    ],[$4],m4_shift(m4_shift3($@)))dnl
m4_popdef([body],[prologue],[attrtest])dnl
])
])


# ACM_CHECK_FORMAT_ATTRIBUTE([FORMAT-STYLE],[ARG1],[ACTION],[ACTION-OPTIONS...])
#
# Specialisation of ACM_CHECK_ATTRIBUTE for function attributes that annotate
# format strings which can and should be sanity checked.  The FORMAT-STYLE is
# one of what GCC calls ARCHETYPES - printf, scanf, strftime, strfmon - which
# determine how the format string itself is interpreted.
#
# The ARG1 parameter is the value to use for the 3rd (FIRST-TO-CHECK) argument
# of the format() attribute.  It indicates which argument in the test function
# contains the first value to be formatted (which for the test that is defined
# by this macro whould be 2), but must be 0 in the case of strftime, or if the
# function takes a va_list instead of the explicit arguments (like vprintf).
# If ARG1 is set to 0, then we use an open coded 0 in the attribute definition
# instead of the function-like macro parameter name arg1.
#
# The ACTION macro, and ACTION-OPTIONS are passed verbatim to the expansion of
# ACM_CHECK_ATTRIBUTE.
#
# This macro assumes that the __gnu_* format conventions are being used when
# compiling for windows instead of the __ms_* ones - as sane code that builds
# with mingw these days will use __USE_MINGW_ANSI_STDIO which is enabled when
# _GNU_SOURCE is defined and provides an implementation which supports the
# full set of POSIX format characters, not just the sub-standard horror show
# from msvcrt.dll ...  We could make that configurable via an extra parameter
# here, or maybe some --enable flag to choose the implementation, but worry
# about that if the GNU compatible version is ever not the only sane choice.
# There will surely be other insanity to deal with if that ever is the case.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_CHECK_FORMAT_ATTRIBUTE],
[dnl
m4_pushdef([prologue],
           [void p(const char*,...) __attribute__((format (__]$[]1[__, 1, m4_default([$2],[2]))));])dnl
AS_CASE([$host],
        [*-*-cygwin* | *-*-mingw32*],[dnl
ACM_CHECK_ATTRIBUTE([function],[format (__gnu_$1__,fmt,]m4_if([$2],[0],[0],[arg1])[)],
                    [AC_LANG_PROGRAM([prologue([gnu_$1])],[[]])],
                    [$3],m4_shift3($@))dnl
        ],[dnl
ACM_CHECK_ATTRIBUTE([function],[format (__$1__,fmt,]m4_if([$2],[0],[0],[arg1])[)],
                    [AC_LANG_PROGRAM([prologue([$1])],[[]])],
                    [$3],m4_shift3($@))dnl
        ])dnl
m4_popdef([prologue])dnl
])


# _ACM_HAVE_ATTRIBUTE([TYPE],[ATTRIBUTE],[TEST-PROGRAM],[this macro],[CPP-MACRO])
#
# ACM_CHECK_[TYPE]_ATTRIBUTE ACTION macro used by ACM_HAVE_[TYPE]_ATTRIBUTE.
# If the CPP-MACRO argument is passed, that macro will be AC_DEFINE'd with
# the numeric value 1 if ATTRIBUTE is supported as used by TEST-PROGRAM.
#
# If CPP-MACRO is not explicitly named, a HAVE_ macro name will be constructed
# using the AC_LANG_PREFIX for the current test language, the attribute TYPE,
# and the sanitised-for-CPP ATTRIBUTE string (ATTR_ID):
#
#   HAVE_[ACM_LANG_PREFIX]_[ATTR_TYPE]_ATTRIBUTE_[ATTR_ID]
#
# That macro can be used as a conditional guard by user code which includes
# the autoheader generated configuration header.
# ----------------------------------------------------------------------------
AC_DEFUN([_ACM_HAVE_ATTRIBUTE],
[dnl
m4_pushdef([ATTR_TYPE],[m4_toupper([$1])])dnl
AS_VAR_IF([cachevar],[yes],
          [AC_DEFINE(m4_default([$5],[HAVE_[]ACM_LANG_PREFIX[]_[]ATTR_TYPE[]_ATTRIBUTE_[]ATTR_ID]),
                     [1], [Have support for $1 attribute '$2'])])
m4_popdef([ATTR_TYPE])dnl
])


# _ACM_DEFINE_ATTRIBUTE([TYPE],[ATTRIBUTE],[TEST-PROGRAM],[this macro],[CPP-MACRO],[ELSE-DEF])
#
# ACM_CHECK_[TYPE]_ATTRIBUTE ACTION macro used by ACM_DEFINE_[TYPE]_ATTRIBUTE.
# If the CPP-MACRO argument is passed, that macro will be AC_DEFINE_UNQUOTED
# using the ATTRIBUTE literal if it is supported as used by TEST-PROGRAM.
#
# If CPP-MACRO is not explicitly named, a macro name will be constructed using
# the AC_LANG_PREFIX for the current test language, the attribute TYPE, and the
# sanitised-for-CPP ATTRIBUTE string (ATTR_ID):
#
#   EM_[ACM_LANG_PREFIX]_[ATTR_TYPE]_ATTRIBUTE_[ATTR_ID]
#
# That macro can be used in user code which includes the autoheader generated
# configuration header in place of the __attribute((ATTRIBUTE)) literal string
# anywhere the attribute itself would be used.
#
# If the attribute itself takes 'variable' arguments, then a function-like
# CPP-MACRO can be defined with the same argument names used in the ATTRIBUTE
# definition, but the TEST-PROGRAM must use the attribute with representative
# real values used in place of the function-like macro argument names.
# For example:
#
#   ACM_DEFINE_VARIABLE_ATTRIBUTE([aligned(n)],
#                                 [EM_ALIGNED(n)],
#                                 [[char c[4] __attribute__((aligned(4)));]])
#
# ----------------------------------------------------------------------------
AC_DEFUN([_ACM_DEFINE_ATTRIBUTE],
[dnl
m4_pushdef([ATTR_TYPE],[m4_toupper([$1])])dnl
AS_VAR_IF([cachevar],[yes],
          [acm_attr_defn="AS_ESCAPE([__attribute__(($2))])"],
          [acm_attr_defn="AS_ESCAPE([$6])"])dnl
AC_DEFINE_UNQUOTED(m4_default([$5],[EM_[]ACM_LANG_PREFIX[]_[]ATTR_TYPE[]_ATTR_[]ATTR_ID]),
                   [$acm_attr_defn], [Macro for $1 attribute '$2'])dnl
m4_popdef([ATTR_TYPE])dnl
])


# ACM_HAVE_[TYPE]_ATTRIBUTE([ATTRIBUTE],[CPP-MACRO],[TEST-PROLOGUE],[TEST-BODY])
# ACM_DEFINE_[TYPE]_ATTRIBUTE([ATTRIBUTE],[CPP-MACRO],[TEST-PROLOGUE],[TEST-BODY],[ELSE-DEF])
#
# Defines the following specialised convenience macros:
#
# ACM_HAVE_FUNCTION_ATTRIBUTE,  ACM_DEFINE_FUNCTION_ATTRIBUTE
# ACM_HAVE_TYPE_ATTRIBUTE,      ACM_DEFINE_TYPE_ATTRIBUTE
# ACM_HAVE_VARIABLE_ATTRIBUTE,  ACM_DEFINE_VARIABLE_ATTRIBUTE
# ACM_HAVE_LABEL_ATTRIBUTE,     ACM_DEFINE_LABEL_ATTRIBUTE
# ACM_HAVE_ENUM_ATTRIBUTE,      ACM_DEFINE_ENUM_ATTRIBUTE
# ACM_HAVE_STATEMENT_ATTRIBUTE, ACM_DEFINE_STATEMENT_ATTRIBUTE
#
# The ACM_HAVE_* macros define a HAVE_* guard macro which can be used to wrap
# code that is conditional on the ATTRIBUTE being supported.
#
# The ACM_DEFINE_* macros define a (possibly function-like) symbol macro which
# may be used in place of the __attribute__((ATTRIBUTE)) literal when simply
# omitting it if it is not supported is an acceptable choice.
#
# If the CPP-MACRO name is not explicitly specified, then a constructed name
# will be used based on the current AC_LANG, the attribute TYPE, and the
# ATTRIBUTE name itself.
#
# If TEST-PROLOGUE or TEST-BODY are not explicitly specified, then the default
# test for the given attribute TYPE will be used.
#
# If ELSE-DEF is provided, then the CPP-MACRO will be defined with that value
# instead of an empty definition if ATTRIBUTE is not supported.
#
# For example:
#   ACM_HAVE_FUNCTION_ATTRIBUTE([no_sanitize("unsigned-integer-overflow")])
#   ACM_DEFINE_FUNCTION_ATTRIBUTE([unused])
#   ACM_DEFINE_TYPE_ATTRIBUTE([packed],[EM_PACKED])
#   ACM_DEFINE_STATEMENT_ATTRIBUTE([fallthrough],[EM_FALLTHROUGH],[],[],
#                                                [do {} while(0)])
# ----------------------------------------------------------------------------
m4_foreach([act],[HAVE,DEFINE],
           [m4_foreach([type],[_ACM_ATTRIBUTE_TYPES],
                       [AC_DEFUN([ACM_]act[_]type[_ATTRIBUTE],
                       [ACM_CHECK_]type[_ATTRIBUTE([$1],[$3],[$4],[_ACM_]]act[[_ATTRIBUTE],[$2],[$5])])
           ])
])


# _ACM_DEFINE_FORMAT_ATTRIBUTE([FORMAT-STYLE],[ARG1],[CPP-MACRO])
#
# ACM_CHECK_FORMAT_ATTRIBUTE ACTION macro used to implement the specialised
# ACM_DEFINE_[STYLE]_FORMAT_ATTRIBUTE macros.
#
# The ARG1 value is used as the FIRST-TO-CHECK parameter in the TEST-PROLOGUE.
# If ARG1 is set to 0, then we use an open coded 0 in the attribute definition
# instead of the function-like mcro parameter name arg1, and we only accept a
# single fmt parameter in the macro
#
# If that test succeeds, a function-like CPP macro is defined of form:
#
# EM_[STYLE]_FORMAT( fmt, arg1 ) __attribute__((format (__style__, fmt, arg1)))
#
# with an empty definition if the test failed to indicate support.
#
# If CPP-MACRO is explicitly specified, then that name will be used for the
# function-like CPP macro instead of EM_[STYLE]_FORMAT.
# ----------------------------------------------------------------------------
AC_DEFUN([_ACM_DEFINE_FORMAT_ATTRIBUTE],
[dnl
m4_pushdef([macro_name],m4_default([$3],[EM_]m4_toupper([$1])[_FORMAT]))dnl
ACM_CHECK_FORMAT_ATTRIBUTE([$1],[$2],[_ACM_DEFINE_ATTRIBUTE],
                           [macro_name[]( fmt[]m4_if([$2],[0],[],[, arg1]) )])dnl
m4_popdef([macro_name])dnl
])


# ACM_DEFINE_[FORMAT-STYLE]_FORMAT_ATTRIBUTE([CPP-MACRO])
#
# Defines the following specialised convenience macros:
#
# ACM_DEFINE_PRINTF_FORMAT_ATTRIBUTE
# ACM_DEFINE_SCANF_FORMAT_ATTRIBUTE
# ACM_DEFINE_STRFTIME_FORMAT_ATTRIBUTE
# ACM_DEFINE_STRFMON_FORMAT_ATTRIBUTE
#
# Which test support for the respective format attributes, and if it is
# available define function-like macros to enable their use, for example:
#
#  EM_PRINTF_FORMAT( fmt, arg1 ) __attribute__((format (__printf__,fmt,arg1)))
#  EM_SCANF_FORMAT( fmt, arg1 )  __attribute__((format (__scanf__,fmt,arg1)))
#  EM_STRFTIME_FORMAT( fmt )     __attribute__((format (__strftime__,fmt,0)))
#  EM_STRFMON_FORMAT( fmt )      __attribute__((format (__strfmon__,fmt,arg1)))
#
# If CPP-MACRO is explicitly specified, then that name will be used for the
# function-like CPP macro instead of EM_[STYLE]_FORMAT.
# ----------------------------------------------------------------------------
m4_foreach([fmt],[[printf,2],[scanf,2],[strfmon,2],[strftime,0]],
           [AC_DEFUN([ACM_DEFINE_]m4_toupper(m4_car(fmt))[_FORMAT_ATTRIBUTE],
                     [_ACM_DEFINE_FORMAT_ATTRIBUTE(]m4_dquote(fmt)[,[$1])])
])


# ACM_ADD_MISSING_DEP([NAME])
#
# Add NAME to the list of missing dependencies which a later call to
# ACM_CHECKPOINT_MISSING_DEPS should check and report on.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_ADD_MISSING_DEP],
[dnl
ACM_ADD_OPT([mu_missing_deps],[$1],[, ])dnl
])


# _ACM_CHECKPOINT_MISSING_DEPS([CHECK-VAR])
#
# Implementation of ACM_CHECKPOINT_MISSING_DEPS
# ----------------------------------------------------------------------------
AC_DEFUN([_ACM_CHECKPOINT_MISSING_DEPS],
[dnl
AS_IF([test -n "$$1"],
      [AC_MSG_NOTICE([])
       AC_MSG_NOTICE([Some required dependencies were not found.])
       AC_MSG_NOTICE([Please install: $$1])
       AC_MSG_NOTICE([])
       AC_MSG_ERROR([Cannot continue until this is resolved.])
      ]
     )
])


# ACM_CHECKPOINT_MISSING_DEPS([CHECK-VAR])
#
# Test if CHECK-VAR is empty, and if it is not then error out, reporting what
# it contains.  It is expected to contain cached information about the missing
# dependencies which didn't cause configure to abort until as complete a list
# as possible of all the missing dependencies could be obtained.
#
# This avoids the annoying whack-a-mole routine of installing something to fix
# a missing dependency, only to then have it fail again with yet another one.
# If CHECK-VAR is not explicitly specified, then mu_missing_deps will be used,
# which is where ACM_ADD_MISSING_DEP will accumulate them.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_CHECKPOINT_MISSING_DEPS],
[dnl
_$0(m4_default([$1],[mu_missing_deps]))
])


# ACM_LONG_OPTIONS([OPTIONS])
#
# Expands a comma-separated list of OPTIONS to a space-separated one with each
# option appended to a '--' for use as long option arguments to some command.
# This macro is just syntactic sugar to keep the point of use easily readable.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_LONG_OPTIONS],[m4_foreach([opt],[$1],[--opt ])])


# ACM_PKG_CONFIG([MIN-VERSION])
#
# This is a safe and portable wrapper to check if pkg-config is available, and
# optionally, that it is no older than MIN-VERSION. It avoids the problem with
# the normal circular dependency where the macro to check for pkg-config is in
# the same package as pkg-config is, potentially making everything fall apart
# horribly and confusingly if it is not.
#
# If the pkg.m4 test macro is available, then we'll use it to set PKG_CONFIG,
# otherwise we'll just note that it's not and leave that unset.  By design it
# is not an error for pkg-config to not be available, nor is it a hard error
# for the version constraint to fail even if some version is available.  Most
# things can have safe and sane fallbacks if pkg-config can't be queried, so
# it is up to the caller to fail out if they do have some hard requirement on
# it that can't be satisfied in any other way.  But usually that's not really
# what most things should do, because most .pc files have almost zero entropy.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_PKG_CONFIG],
[dnl
m4_ifdef([PKG_PROG_PKG_CONFIG],
         [PKG_PROG_PKG_CONFIG([$1])],
         [AC_MSG_NOTICE([pkg-config pkg.m4 is not installed])])
])


# ACM_PKG_CONFIG_GET([LOCAL-VARIABLE],[MODULE],[OPTIONS],[DEFAULT],[FILTER])
#
# If LOCAL-VARIABLE is not already set, either from the environment, or on the
# command-line of configure, or by some prior action of the configure script,
# then try to obtain a value for it from the pkg-config .pc for MODULE, with a
# query defined by the comma-separated list of pkg-config OPTIONS.
#
# If pkg-config is not available, or if the query fails, then set it to the
# given DEFAULT value.
#
# If the optional FILTER parameter is passed, then if the result is obtained
# from pkg-config, that macro will also be expanded with LOCAL-VARIABLE passed
# as a parameter after its value has been assigned to what pkg-config returned.
# This allows it to be modified if needed, before the result is reported and
# subsequently used.  If the value is obtained from the environment or the
# DEFAULT is used, then the FILTER parameter is ignored.
#
# For example:
# ACM_PKG_CONFIG_GET([FOO_FLAGS],[foo],[cflags,libs],[-lfoo])
#
# Will try to set FOO_FLAGS using `pkg-config --cflags --libs foo` if possible
# else fall back to a default of using '-lfoo'.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_PKG_CONFIG_GET],
[dnl
AC_REQUIRE([ACM_PKG_CONFIG])
AC_MSG_CHECKING([for $1])
AS_IF([test -n "$$1"],
      [AC_MSG_RESULT(['$$1' (from environment)])],

      [test -z "$PKG_CONFIG"],
      [AC_MSG_RESULT(['$4' (default value)])
       $1="$4"
      ],

      [$1=$($PKG_CONFIG ACM_LONG_OPTIONS([$3])"$2" 2>/dev/null)],
      [m4_ifval([$5],[$5([$1])])dnl
       AC_MSG_RESULT(['$$1' (from $2.pc $($PKG_CONFIG --modversion $2 2>/dev/null))])],

      [AC_MSG_RESULT(['$4' (default value)])
       AC_MSG_WARN(['pkg-config ACM_LONG_OPTIONS([$3])$2' failed])
       $1="$4"
      ]dnl
     )dnl
])


# ACM_PKG_CONFIG_GET_VAR([LOCAL-VARIABLE],[MODULE],[CONFIG-VARIABLE],[DEFAULT])
#
# If LOCAL-VARIABLE is not already set, either from the environment, or on the
# command-line of configure, or by some prior action of the configure script,
# then try to obtain a value for it from the variable CONFIG-VARIABLE set in
# the pkg-config .pc for MODULE.
#
# If pkg-config is not available, or if the query fails, then set it to the
# given DEFAULT value.
#
# For example:
# ACM_PKG_CONFIG_GET_VAR([FOO_DIR],[foo],[foodir],[/tmp/foo])
#
# Will try to set FOO_DIR using `pkg-config --variable=foodir foo` if possible
# else fall back to a default of using '/tmp/foo'.
#
# This is syntactic sugar equivalent to:
# ACM_PKG_CONFIG_GET([FOO_DIR],[foo],[variable="foodir"],[/tmp/foo])
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_PKG_CONFIG_GET_VAR],
[dnl
ACM_PKG_CONFIG_GET([$1],[$2],[variable="$3"],[$4])
])


# ACM_PC_LIBS([LOCAL-VARIABLE])
#
# This is a filter macro used by ACM_PKG_CONFIG_GET_LIBS to strip the leading
# -l from the list of libraries returned by pkg-config to put them into the
# form expected by makeup *_LIBS variables where only the library name is used.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_PC_LIBS],
[dnl
$1=$(echo "$$1" | [sed 's/^-l//; s/[[:space:]]-l/ /g'])
])

# ACM_PKG_CONFIG_GET_LIBS([LOCAL-VARIABLE],[MODULE],[DEFAULT],[STATIC-EXTRA])
#
# This is a specialisation of ACM_PKG_CONFIG_GET for fetching the --libs-only-l
# list of libraries to link with, but with the -l prefixes removed to give them
# to us in the form that makup expects for *_LIBS.
#
# If LOCAL-VARIABLE is not already set, either from the environment, or on the
# command-line of configure, or by some prior action of the configure script,
# then try to obtain a value for it from the pkg-config .pc for MODULE.  If the
# mu_cv_enable_shared cache variable is set to 'no' then the query will include
# the --static option to pkg-config to also obtain the Libs.private needed for
# static linking.
#
# If pkg-config is not available, or if the query fails, then set it to the
# given DEFAULT value.  The STATIC-EXTRA value will be appended to that in the
# case where Libs.private would have been included as described above.
# If a DEFAULT value is not passed then the value of MODULE will be used.
#
# For example:
# ACM_PKG_CONFIG_GET_LIBS([FOO_LIBS],[libfoo],[foo],[bar z m])
#
# Will try to set FOO_LIBS to the list of libraries that are returned by
# `pkg-config --libs-only-l libfoo` if possible, else fall back to a default
# of using 'foo'.  Or in the case where static linking is configured, it will
# try `pkg-config --static --libs-only-l libfoo` falling back to 'foo bar z m'
# so that libbar, libz, and libm are also included when static linking.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_PKG_CONFIG_GET_LIBS],
[dnl
AS_IF([test "$mu_cv_enable_shared" = no],
      [ACM_PKG_CONFIG_GET([$1],[$2],[static,libs-only-l],m4_join([ ],m4_default([$3],[$2]),[$4]),[ACM_PC_LIBS])],
      [ACM_PKG_CONFIG_GET([$1],[$2],[libs-only-l],m4_default([$3],[$2]),[ACM_PC_LIBS])])
])


# ACM_PC_LDFLAGS([LOCAL-VARIABLE])
#
# This is a filter macro used by ACM_PKG_CONFIG_GET_LDFLAGS to strip all words
# beginning with '-l' from the string returned by pkg-config leaving only the
# options which should be passed as *_LDFLAGS without any of the libraries.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_PC_LDFLAGS],
[dnl
$1=$(echo "$$1" | [sed 's/^\(-l[^[:space:]]\+[[:space:]]*\)\+//; s/[[:space:]]\+-l[^[:space:]]\+//g'])
])

# ACM_PKG_CONFIG_GET_LDFLAGS([LOCAL-VARIABLE],[MODULE],[DEFAULT],[STATIC-EXTRA])
#
# This is a specialisation of ACM_PKG_CONFIG_GET to work around the fact that
# --libs-only-L does not return all of the LDFLAGS specified for linking, only
# the linker search paths, and there is no way to get just those aside from
# obtaining the entire --libs output and filtering out the libraries from it.
#
# If LOCAL-VARIABLE is not already set, either from the environment, or on the
# command-line of configure, or by some prior action of the configure script,
# then try to obtain a value for it from the pkg-config .pc for MODULE.  If the
# mu_cv_enable_shared cache variable is set to 'no' then the query will include
# the --static option to pkg-config to also obtain the Libs.private needed for
# static linking.
#
# If pkg-config is not available, or if the query fails, then set it to the
# given DEFAULT value.  The STATIC-EXTRA value will be appended to that in the
# case where Libs.private would have been included as described above.
#
# For example:
# ACM_PKG_CONFIG_GET_LIBS([FOO_LDFLAGS],[foo],[-rdynamic],[-pthread])
#
# Will try to set FOO_LDFLAGS to the non-library options that are returned by
# `pkg-config --libs foo` if possible, else fall back to a default of using
# '-rdynamic'.  Or for the case where static linking is configured, it will
# try `pkg-config --static --libs foo`, falling back to '-rdynamic -pthread'
# as the *_LDFLAGS needed for static linking.
# ----------------------------------------------------------------------------
AC_DEFUN([ACM_PKG_CONFIG_GET_LDFLAGS],
[dnl
AS_IF([test "$mu_cv_enable_shared" = no],
      [ACM_PKG_CONFIG_GET([$1],[$2],[static,libs],m4_join([ ],[$3],[$4]),[ACM_PC_LDFLAGS])],
      [ACM_PKG_CONFIG_GET([$1],[$2],[libs],[$3],[ACM_PC_LDFLAGS])])
])

