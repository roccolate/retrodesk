#ifndef RETRODESK_CORE_COMPAT_H
#define RETRODESK_CORE_COMPAT_H

/* MSVC exposes the POSIX strdup function as _strdup. Keep the compatibility
   mapping in one internal header so core/app sources remain portable C11. */
#if defined(_MSC_VER) && !defined(strdup)
#define strdup _strdup
#endif

#endif
