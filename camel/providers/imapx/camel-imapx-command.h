/*
 * camel-imapx-command.h
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

#ifndef CAMEL_IMAPX_COMMAND_H
#define CAMEL_IMAPX_COMMAND_H

#include <camel.h>

#include "camel-imapx-server.h"
#include "camel-imapx-utils.h"

G_BEGIN_DECLS

/* Avoid a circular reference. */
struct _CamelIMAPXJob;

typedef struct _CamelIMAPXCommand CamelIMAPXCommand;
typedef struct _CamelIMAPXCommandPart CamelIMAPXCommandPart;

typedef gboolean
		(*CamelIMAPXCommandFunc)	(CamelIMAPXServer *is,
						 CamelIMAPXCommand *ic,
						 GError **error);

typedef enum {
	CAMEL_IMAPX_COMMAND_SIMPLE = 0,
	CAMEL_IMAPX_COMMAND_DATAWRAPPER,
	CAMEL_IMAPX_COMMAND_STREAM,
	CAMEL_IMAPX_COMMAND_AUTH,
	CAMEL_IMAPX_COMMAND_FILE,
	CAMEL_IMAPX_COMMAND_STRING,
	CAMEL_IMAPX_COMMAND_MASK = 0xff,

	/* Continuation with LITERAL+ */
	CAMEL_IMAPX_COMMAND_LITERAL_PLUS = 1 << 14,

	/* Does this command expect continuation? */
	CAMEL_IMAPX_COMMAND_CONTINUATION = 1 << 15

} CamelIMAPXCommandPartType;

struct _CamelIMAPXCommandPart {
	CamelIMAPXCommandPart *next;
	CamelIMAPXCommandPart *prev;

	gint data_size;
	gchar *data;

	CamelIMAPXCommandPartType type;

	gint ob_size;
	gpointer ob;
};

struct _CamelIMAPXCommand {
	CamelIMAPXCommand *next, *prev;

	CamelIMAPXServer *is;
	gint pri;

	/* Command name/type (e.g. FETCH) */
	const gchar *name;

	/* Folder to select */
	CamelFolder *select;

	/* Status for command, indicates it is complete if != NULL. */
	struct _status_info *status;

	guint32 tag;

	CamelDList parts;
	CamelIMAPXCommandPart *current;

	/* Responsible for free'ing the command. */
	CamelIMAPXCommandFunc complete;
	struct _CamelIMAPXJob *job;
};

CamelIMAPXCommand *
		camel_imapx_command_new		(CamelIMAPXServer *is,
						 const gchar *name,
						 CamelFolder *select,
						 const gchar *format,
						 ...);
CamelIMAPXCommand *
		camel_imapx_command_ref		(CamelIMAPXCommand *ic);
void		camel_imapx_command_unref	(CamelIMAPXCommand *ic);
void		camel_imapx_command_add		(CamelIMAPXCommand *ic,
						 const gchar *format,
						 ...);
void		camel_imapx_command_addv	(CamelIMAPXCommand *ic,
						 const gchar *format,
						 va_list ap);
void		camel_imapx_command_add_part	(CamelIMAPXCommand *ic,
						 CamelIMAPXCommandPartType type,
						 gpointer data);
void		camel_imapx_command_close	(CamelIMAPXCommand *ic);
void		camel_imapx_command_wait	(CamelIMAPXCommand *ic);
void		camel_imapx_command_done	(CamelIMAPXCommand *ic);
gboolean	camel_imapx_command_set_error_if_failed
						(CamelIMAPXCommand *ic,
						 GError **error);

G_END_DECLS

#endif /* CAMEL_IMAPX_COMMAND_H */
