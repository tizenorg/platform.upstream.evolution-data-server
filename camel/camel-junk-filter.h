/*
 * camel-junk-filter.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#if !defined (__CAMEL_H_INSIDE__) && !defined (CAMEL_COMPILATION)
#error "Only <camel/camel.h> can be included directly."
#endif

#ifndef CAMEL_JUNK_FILTER_H
#define CAMEL_JUNK_FILTER_H

#include <camel/camel-mime-message.h>

/* Standard GObject macros */
#define CAMEL_TYPE_JUNK_FILTER \
	(camel_junk_filter_get_type ())
#define CAMEL_JUNK_FILTER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_JUNK_FILTER, CamelJunkFilter))
#define CAMEL_JUNK_FILTER_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_JUNK_FILTER, CamelJunkFilterInterface))
#define CAMEL_IS_JUNK_FILTER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_JUNK_FILTER))
#define CAMEL_IS_JUNK_FILTER_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_JUNK_FILTER))
#define CAMEL_JUNK_FILTER_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), CAMEL_TYPE_JUNK_FILTER, CamelJunkFilterInterface))

G_BEGIN_DECLS

typedef struct _CamelJunkFilter CamelJunkFilter;
typedef struct _CamelJunkFilterInterface CamelJunkFilterInterface;

typedef enum {
	CAMEL_JUNK_STATUS_INCONCLUSIVE,
	CAMEL_JUNK_STATUS_MESSAGE_IS_JUNK,
	CAMEL_JUNK_STATUS_MESSAGE_IS_NOT_JUNK
} CamelJunkStatus;

struct _CamelJunkFilterInterface {
	GTypeInterface parent_interface;

	/* Required Methods */
	gboolean	(*classify)		(CamelJunkFilter *junk_filter,
						 CamelMimeMessage *message,
						 CamelJunkStatus *status,
						 GCancellable *cancellable,
						 GError **error);
	gboolean	(*learn_junk)		(CamelJunkFilter *junk_filter,
						 CamelMimeMessage *message,
						 GCancellable *cancellable,
						 GError **error);
	gboolean	(*learn_not_junk)	(CamelJunkFilter *junk_filter,
						 CamelMimeMessage *message,
						 GCancellable *cancellable,
						 GError **error);

	/* Optional Methods */
	gboolean	(*synchronize)		(CamelJunkFilter *junk_filter,
						 GCancellable *cancellable,
						 GError **error);
};

GType		camel_junk_filter_get_type	(void) G_GNUC_CONST;
gboolean	camel_junk_filter_classify	(CamelJunkFilter *junk_filter,
						 CamelMimeMessage *message,
						 CamelJunkStatus *status,
						 GCancellable *cancellable,
						 GError **error);
gboolean	camel_junk_filter_learn_junk	(CamelJunkFilter *junk_filter,
						 CamelMimeMessage *message,
						 GCancellable *cancellable,
						 GError **error);
gboolean	camel_junk_filter_learn_not_junk
						(CamelJunkFilter *junk_filter,
						 CamelMimeMessage *message,
						 GCancellable *cancellable,
						 GError **error);
gboolean	camel_junk_filter_synchronize	(CamelJunkFilter *junk_filter,
						 GCancellable *cancellable,
						 GError **error);

G_END_DECLS

#endif /* CAMEL_JUNK_FILTER_H */