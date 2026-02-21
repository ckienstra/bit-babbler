//  This file is distributed as part of the bit-babbler package.
//  Copyright 2014 - 2015,  Ron <ron@debian.org>
//
// This file provides the implementation detail for bit-babbler/health-monitor.h
// which must be defined only once in an application.

#ifdef _BBIMPL_HEALTH_MONITOR_H
#error bit-babbler/impl/health-monitor.h must be included only once.
#endif

#define _BBIMPL_HEALTH_MONITOR_H

#include <bit-babbler/health-monitor.h>

namespace BitB
{
    Monitor::List       Monitor::ms_list;
    pthread_mutex_t     Monitor::ms_mutex = PTHREAD_MUTEX_INITIALIZER;
}

// vi:sts=4:sw=4:et:foldmethod=marker

