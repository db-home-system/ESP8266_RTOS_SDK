/**
 * @file common.h
 *
 * RadioLog
 *
 * Copyright (c) 2021 Daniele Basile <asterix24@gmail.com>
 */
#ifndef VERSTAG_H
#define VERSTAG_H

#define APP_NAME "RadioLogNode"
#define APP_DESCRIPTION ""
#define APP_AUTHOR "Daniele Basile"
#define APP_COPYRIGHT "Copyright 2021 Daniele Basile <asterix24@gmail.com>"

#define VERS_MAJOR 0
#define VERS_MINOR 1
#define VERS_REV   0
#define VERS_LETTER ""

#define __STRINGIZE(x) #x
#define _STRINGIZE(x) __STRINGIZE(x)

/** Build application version string (i.e.: "1.7.0") */
#define MAKE_VERS(maj,min,rev) _STRINGIZE(maj) "." _STRINGIZE(min) "." _STRINGIZE(rev) VERS_LETTER
#define VERS_TAG MAKE_VERS(VERS_MAJOR,VERS_MINOR,VERS_REV)

/** The revision string (contains VERS_TAG) */
extern const char vers_tag[];

#endif /* VERSTAG_H */
