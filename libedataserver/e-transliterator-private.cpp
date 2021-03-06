/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2013 Intel Corporation
 *
g * This library is free software; you can redistribute it and/or modify it
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-transliterator-private.h"

/* C++ standard library */
#include <string>
#include <memory>

/* system headers */
#include <langinfo.h>
#include <locale.h>

/* ICU headers */
#include <unicode/translit.h>

using icu::Transliterator;

struct _ECxxTransliterator {
	Transliterator *priv;
};

/* Create an Transliterator for the source and target
 * language stripts
 */
ECxxTransliterator *
_e_transliterator_cxx_new (const gchar *transliterator_id)
{
	UErrorCode status = U_ZERO_ERROR;
	ECxxTransliterator *transliterator;

	g_return_val_if_fail (transliterator_id != NULL, NULL);

	transliterator = g_slice_new (ECxxTransliterator);
	transliterator->priv = Transliterator::createInstance (transliterator_id, UTRANS_FORWARD, status); 

	return transliterator;
}

/* Frees an ECxxTransliterator and it's associated resources
 */
void
_e_transliterator_cxx_free (ECxxTransliterator *transliterator)
{
	if (transliterator) {
		delete transliterator->priv;
		g_slice_free (ECxxTransliterator, transliterator);
	}
}

/* Transliterates 'str' and returns the new allocated result
 */
gchar *
_e_transliterator_cxx_transliterate (ECxxTransliterator  *transliterator,
				     const gchar         *str)
{
	UnicodeString transform;
	std::string sourceUTF8;
	std::string targetUTF8;

	g_return_val_if_fail (transliterator != NULL, NULL);
	g_return_val_if_fail (str != NULL, NULL);

	sourceUTF8 = str;
	transform = icu::UnicodeString::fromUTF8 (sourceUTF8);
	transliterator->priv->transliterate (transform);
	targetUTF8 = transform.toUTF8String (targetUTF8);

	return g_strdup (targetUTF8.c_str());
}
