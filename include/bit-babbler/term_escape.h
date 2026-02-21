//  This file is distributed as part of the bit-babbler package.
//  Copyright 2014 - 2015,  Ron <ron@debian.org>

#ifndef _BB_TERM_ESCAPE_H
#define _BB_TERM_ESCAPE_H

#define BLACK               "\x1b[0;30m"
#define RED                 "\x1b[0;31m"
#define GREEN               "\x1b[0;32m"
#define YELLOW              "\x1b[0;33m"
#define BLUE                "\x1b[0;34m"
#define PURPLE              "\x1b[0;35m"
#define CYAN                "\x1b[0;36m"
#define WHITE               "\x1b[0;37m"

#define BOLD                "\x1b[1m"

#define BOLD_BLACK          "\x1b[1;30m"
#define BOLD_RED            "\x1b[1;31m"
#define BOLD_GREEN          "\x1b[1;32m"
#define BOLD_YELLOW         "\x1b[1;33m"
#define BOLD_BLUE           "\x1b[1;34m"
#define BOLD_PURPLE         "\x1b[1;35m"
#define BOLD_CYAN           "\x1b[1;36m"
#define BOLD_WHITE          "\x1b[1;37m"

#define MID_GREEN           "\x1b[38;5;40m"
#define MID_YELLOW          "\x1b[38;5;227m"
#define MID_ORANGE          "\x1b[38;5;214m"

#define DARK_RED            "\x1b[38;5;88m"

#define END_COLOUR          "\x1b[0m"


#define COLOUR_STR(c,s)     c s END_COLOUR

#define BLACK_STR(s)        COLOUR_STR(BLACK,s)
#define RED_STR(s)          COLOUR_STR(RED,s)
#define GREEN_STR(s)        COLOUR_STR(GREEN,s)
#define YELLOW_STR(s)       COLOUR_STR(YELLOW,s)
#define BLUE_STR(s)         COLOUR_STR(BLUE,s)
#define PURPLE_STR(s)       COLOUR_STR(PURPLE,s)
#define CYAN_STR(s)         COLOUR_STR(CYAN,s)
#define WHITE_STR(s)        COLOUR_STR(WHITE,s)

#define BOLD_BLACK_STR(s)   COLOUR_STR(BOLD_BLACK,s)
#define BOLD_RED_STR(s)     COLOUR_STR(BOLD_RED,s)
#define BOLD_GREEN_STR(s)   COLOUR_STR(BOLD_GREEN,s)
#define BOLD_YELLOW_STR(s)  COLOUR_STR(BOLD_YELLOW,s)
#define BOLD_BLUE_STR(s)    COLOUR_STR(BOLD_BLUE,s)
#define BOLD_PURPLE_STR(s)  COLOUR_STR(BOLD_PURPLE,s)
#define BOLD_CYAN_STR(s)    COLOUR_STR(BOLD_CYAN,s)
#define BOLD_WHITE_STR(s)   COLOUR_STR(BOLD_WHITE,s)


#define COLOUR_STR_IF(cond,col,s)   (cond) ? col s END_COLOUR : s


#endif  // _BB_TERM_ESCAPE_H

// vi:sts=4:sw=4:et:foldmethod=marker
