/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2013 Intel Corporation
 *
 * This library is free software you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Tristan Van Berkom <tristanvb@openismus.com>
 */

#if !defined (__LIBEDATASERVER_H_INSIDE__) && !defined (LIBEDATASERVER_COMPILATION)
#error "Only <libedataserver/libedataserver.h> should be included directly."
#endif

#ifndef E_TRANSLITERATOR_PRIVATE_H
#define E_TRANSLITERATOR_PRIVATE_H

#include <glib-object.h>

G_BEGIN_DECLS

#if __GNUC__ >= 4
#  define E_TRANSLITERATOR_LOCAL __attribute__ ((visibility ("hidden")))
#else
#  define E_TRANSLITERATOR_LOCAL
#endif

/**
 * ECxxTransliterator:
 *
 * A private opaque type describing an alphabetic index
 *
 * Since: 3.12
 **/
typedef struct _ECxxTransliterator ECxxTransliterator;

/* defined in e-transliterator-private.cpp, and used by by e-collator.c */
E_TRANSLITERATOR_LOCAL ECxxTransliterator *_e_transliterator_cxx_new             (const gchar        *transliterator_id);
E_TRANSLITERATOR_LOCAL void                _e_transliterator_cxx_free            (ECxxTransliterator *transliterator);
E_TRANSLITERATOR_LOCAL gchar              *_e_transliterator_cxx_transliterate   (ECxxTransliterator *transliterator,
										  const gchar        *str);

G_END_DECLS

#endif /* E_TRANSLITERATOR_PRIVATE_H */
