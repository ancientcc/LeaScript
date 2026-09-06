/* Handle list of needed message catalogs
   Copyright (C) 1995-1999, 2000-2001, 2003-2006 Free Software Foundation, Inc.
   Written by Ulrich Drepper <drepper@gnu.org>, 1995.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU Library General Public License as published
   by the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
   USA.  */

#include "config.h"

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#if defined HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "gettextP.h"
#include <libintl.h>

/* Handle multi-threaded applications.  */
# include "lock.h"

#include <SDL_stdinc.h>

/* @@ end of prolog @@ */
/* List of already loaded domains.  */
struct loaded_l10nfile *_nl_loaded_domains;


/* Return a data structure describing the message catalog described by
   the DOMAINNAME and CATEGORY parameters with respect to the currently
   established bindings.  */
struct loaded_l10nfile *
internal_function
_nl_find_domain (const char *dirname, char *locale,
		 const char *domainname, struct binding *domainbinding)
{
  struct loaded_l10nfile *retval;
  const char *language;
  const char *modifier;
  const char *territory;
  const char *codeset;
  const char *normalized_codeset;
  const char *alias_value;
  int mask;

  /* LOCALE can consist of up to four recognized parts for the XPG syntax:

		language[_territory][.codeset][@modifier]

     Beside the first part all of them are allowed to be missing.  If
     the full specified locale is not found, the less specific one are
     looked for.  The various parts will be stripped off according to
     the following order:
		(1) codeset
		(2) normalized codeset
		(3) territory
		(4) modifier
   */

  /* We need to protect modifying the _NL_LOADED_DOMAINS data.  */
  gl_rwlock_define_initialized (static, lock);
  gl_rwlock_rdlock (lock);

  /* If we have already tested for this locale entry there has to
     be one data set in the list of loaded domains.  */
  retval = _nl_make_l10nflist (&_nl_loaded_domains, dirname,
			       strlen (dirname) + 1, 0, locale, NULL, NULL,
			       NULL, NULL, domainname, 0);

  gl_rwlock_unlock (lock);

  if (retval != NULL)
    {
      /* We know something about this locale.  */
      int cnt;

      if (retval->decided <= 0)
	_nl_load_domain (retval, domainbinding);

      if (retval->data != NULL)
	return retval;

      for (cnt = 0; retval->successor[cnt] != NULL; ++cnt)
	{
	  if (retval->successor[cnt]->decided <= 0)
	    _nl_load_domain (retval->successor[cnt], domainbinding);

	  if (retval->successor[cnt]->data != NULL)
	    break;
	}

      return retval;
      /* NOTREACHED */
    }

  /* See whether the locale value is an alias.  If yes its value
     *overwrites* the alias name.  No test for the original value is
     done.  */
  alias_value = _nl_expand_alias (locale);
  if (alias_value != NULL)
    {
#if defined HAVE_STRDUP
      locale = strdup (alias_value);
      if (locale == NULL)
	return NULL;
#else
      size_t len = strlen (alias_value) + 1;
      locale = (char *) malloc (len);
      if (locale == NULL)
	return NULL;

      memcpy (locale, alias_value, len);
#endif
    }

  /* Now we determine the single parts of the locale name.  First
     look for the language.  Termination symbols are `_', '.', and `@'.  */
  mask = _nl_explode_name (locale, &language, &modifier, &territory,
			   &codeset, &normalized_codeset);

  /* We need to protect modifying the _NL_LOADED_DOMAINS data.  */
  gl_rwlock_wrlock (lock);

  /* Create all possible locale entries which might be interested in
     generalization.  */
  retval = _nl_make_l10nflist (&_nl_loaded_domains, dirname,
			       strlen (dirname) + 1, mask, language, territory,
			       codeset, normalized_codeset, modifier,
			       domainname, 1);

  gl_rwlock_unlock (lock);

  if (retval == NULL)
    /* This means we are out of core.  */
    return NULL;

  if (retval->decided <= 0)
    _nl_load_domain (retval, domainbinding);

  if (retval->data == NULL)
    {
      int cnt;
      for (cnt = 0; retval->successor[cnt] != NULL; ++cnt)
	{
	  if (retval->successor[cnt]->decided <= 0)
	    _nl_load_domain (retval->successor[cnt], domainbinding);
	  if (retval->successor[cnt]->data != NULL)
	    break;
	}
    }

  /* The room for an alias was dynamically allocated.  Free it now.  */
  if (alias_value != NULL)
    free (locale);

  /* The space for normalized_codeset is dynamically allocated.  Free it.  */
  if (mask & XPG_NORM_CODESET)
    free ((void *) normalized_codeset);

  return retval;
}

// Lock variable to protect the global data in the gettext implementation.
gl_rwlock_define (extern, _nl_state_lock attribute_hidden)

void unload_l10nflist(const char* dirname)
{
	struct loaded_l10nfile* l10nfile;
	struct loaded_l10nfile* prev_binding = NULL;
	int unloaded = 1;

	// Some sanity checks.
	if (dirname == NULL || dirname[0] == '\0') {
		return;
	}

	// MUST called by unbindtextdomain(...). there has called gl_rwlock_wrlock(..)
	// gl_rwlock_wrlock (_nl_state_lock);

	while (unloaded) {
		unloaded = 0;
		for (l10nfile = _nl_loaded_domains; l10nfile != NULL; l10nfile = l10nfile->next) {
			if (strstr(l10nfile->filename, dirname) != NULL) {
				// We found it!
				if (prev_binding != NULL) {
					prev_binding->next = l10nfile->next;
				} else {
					_nl_loaded_domains = l10nfile->next;
				}
				unloaded = 1;
				break;
			}
			prev_binding = l10nfile;
		}

		if (l10nfile != NULL) {
			if (l10nfile->data != NULL) {
				struct loaded_domain *domaindata = (struct loaded_domain *)l10nfile->data;
				if (domaindata->data != NULL) {
					free(domaindata->data);
				}
				if (domaindata->malloced != NULL) {
					free(domaindata->malloced);
				}
				free(domaindata);
			}
			free (l10nfile);
		}
	}

	// gl_rwlock_unlock (_nl_state_lock);
}

SDL_IntlLoaded_l10nfile* SDL_IntlGetLoaded_l10nfiles(int* count_ptr)
{
	struct loaded_l10nfile* l10nfile;
	int count = 0;
	int at = 0;

	gl_rwlock_rdlock (_nl_state_lock);

    for (l10nfile = _nl_loaded_domains; l10nfile != NULL; l10nfile = l10nfile->next) {
		count ++;
	}

	SDL_IntlLoaded_l10nfile* result = NULL;
	if (count != 0) {
		result = SDL_malloc(sizeof(SDL_IntlLoaded_l10nfile) * count);
		SDL_memset(result, 0, sizeof(SDL_IntlLoaded_l10nfile) * count);
	}

	for (l10nfile = _nl_loaded_domains; l10nfile != NULL && at < count; l10nfile = l10nfile->next, at ++) {
		SDL_IntlLoaded_l10nfile* to = result + at;
		SDL_strlcpy(to->filename, l10nfile->filename, sizeof(to->filename));
		to->decided = l10nfile->decided;
		if (l10nfile->data != NULL) {
			struct loaded_domain *domaindata = (struct loaded_domain *)l10nfile->data;
			to->mmap_size = domaindata->mmap_size;

		} else {
			to->mmap_size = -1;
		}

		int cnt;
		for (cnt = 0; l10nfile->successor[cnt] != NULL; ++ cnt) {
			to->successor_count ++;
			// if (retval->successor[cnt]->decided <= 0)
			//	_nl_load_domain (retval->successor[cnt], domainbinding);
			// if (retval->successor[cnt]->data != NULL)
			//	break;
		}
	}

	gl_rwlock_unlock (_nl_state_lock);

	if (count_ptr != NULL) {
		*count_ptr = at;
	}
	return result;
}


