/*
 * camel-imapx-list-response.c
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

/**
 * SECTION: camel-imapx-list-response
 * @include: camel/camel.h
 * @short_description: Stores an IMAP LIST response
 *
 * #CamelIMAPXListResponse encapsulates an IMAP LIST response, which consists
 * of a set of mailbox attributes, a mailbox separator character, and the
 * mailbox name.  (Extended information for LIST responses, as described in
 * <ulink url="http://tools.ietf.org/html/rfc5258">RFC 5258</ulink>, to be
 * supported at a later date.)
 *
 * #CamelIMAPXListResponse can also convert the mailbox attributes to
 * a #CamelStoreInfoFlags / #CamelFolderInfoFlags value for use with
 * camel_store_get_folder_info().
 **/

#include "camel-imapx-list-response.h"

#define CAMEL_IMAPX_LIST_RESPONSE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), CAMEL_TYPE_IMAPX_LIST_RESPONSE, CamelIMAPXListResponsePrivate))

struct _CamelIMAPXListResponsePrivate {
	gchar *mailbox;
	gchar separator;
	GQueue attributes;
	GHashTable *extended_items;
};

G_DEFINE_TYPE (
	CamelIMAPXListResponse,
	camel_imapx_list_response,
	G_TYPE_OBJECT)

static void
imapx_list_response_finalize (GObject *object)
{
	CamelIMAPXListResponsePrivate *priv;

	priv = CAMEL_IMAPX_LIST_RESPONSE_GET_PRIVATE (object);

	g_free (priv->mailbox);

	/* Flag strings are interned, so don't free them. */
	g_queue_clear (&priv->attributes);

	g_hash_table_destroy (priv->extended_items);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (camel_imapx_list_response_parent_class)->
		finalize (object);
}

static void
camel_imapx_list_response_class_init (CamelIMAPXListResponseClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (
		class, sizeof (CamelIMAPXListResponsePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = imapx_list_response_finalize;
}

static void
camel_imapx_list_response_init (CamelIMAPXListResponse *response)
{
	GHashTable *extended_items;

	extended_items = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_variant_unref);

	response->priv = CAMEL_IMAPX_LIST_RESPONSE_GET_PRIVATE (response);
	response->priv->extended_items = extended_items;
}

/* Helper for camel_imapx_list_response_new() */
static GVariant *
imapx_list_response_parse_childinfo (CamelIMAPXStream *stream,
                                     CamelIMAPXListResponse *response,
                                     GCancellable *cancellable,
                                     GError **error)
{
	GVariantBuilder builder;
	GVariant *value;
	camel_imapx_token_t tok;
	guchar *token;
	guint len;

	g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);

	tok = camel_imapx_stream_token (
		stream, &token, &len, cancellable, error);
	if (tok == IMAPX_TOK_ERROR)
		goto fail;
	if (tok != '(') {
		g_set_error (
			error, CAMEL_IMAPX_ERROR, 1,
			"list childinfo: expecting ')'");
		goto fail;
	}

repeat:
	if (!camel_imapx_stream_astring (stream, &token, cancellable, error))
		goto fail;

	value = g_variant_new_string ((gchar *) token);
	g_variant_builder_add_value (&builder, value);

	tok = camel_imapx_stream_token (
		stream, &token, &len, cancellable, error);
	if (tok == IMAPX_TOK_ERROR)
		goto fail;
	if (tok != ')') {
		camel_imapx_stream_ungettoken (stream, tok, token, len);
		goto repeat;
	}

	return g_variant_builder_end (&builder);

fail:
	g_variant_builder_clear (&builder);

	return NULL;
}

/* Helper for camel_imapx_list_response_new() */
static GVariant *
imapx_list_response_parse_oldname (CamelIMAPXStream *stream,
                                   CamelIMAPXListResponse *response,
                                   GCancellable *cancellable,
                                   GError **error)
{
	camel_imapx_token_t tok;
	guchar *token;
	guint len;
	gchar *mailbox = NULL;

	tok = camel_imapx_stream_token (
		stream, &token, &len, cancellable, error);
	if (tok == IMAPX_TOK_ERROR)
		goto fail;
	if (tok != '(') {
		g_set_error (
			error, CAMEL_IMAPX_ERROR, 1,
			"list oldname: expecting ')'");
		goto fail;
	}

	mailbox = camel_imapx_parse_mailbox (stream, cancellable, error);
	if (mailbox == NULL)
		goto fail;

	tok = camel_imapx_stream_token (
		stream, &token, &len, cancellable, error);
	if (tok == IMAPX_TOK_ERROR)
		goto fail;
	if (tok != ')') {
		g_set_error (
			error, CAMEL_IMAPX_ERROR, 1,
			"list oldname: expecting ')'");
		goto fail;
	}

	return g_variant_new_string (mailbox);

fail:
	g_free (mailbox);

	return NULL;
}

/* Helper for camel_imapx_list_response_new() */
static gboolean
imapx_list_response_parse_extended_item (CamelIMAPXStream *stream,
                                         CamelIMAPXListResponse *response,
                                         GCancellable *cancellable,
                                         GError **error)
{
	guchar *token;
	gchar *item_tag;
	GVariant *item_value = NULL;
	gboolean success;

	/* Parse the extended item tag. */

	if (!camel_imapx_stream_astring (stream, &token, cancellable, error))
		return FALSE;

	item_tag = g_strdup ((gchar *) token);

	/* Parse the extended item value if we recognize the tag,
	 * otherwise skip to the end.  This is easier than trying
	 * to anticipate all possible extensions ahead of time.
	 *
	 * XXX If we had a real LALR(1) parser then we could at
	 *     least parse it into an abstract syntax tree, and
	 *     pick out the items we support.  Alas, our ad-hoc
	 *     IMAP parser makes this more difficult. */

	/* RFC 5258 "LIST-EXTENDED" */
	if (g_strcmp0 (item_tag, "CHILDINFO") == 0) {
		item_value = imapx_list_response_parse_childinfo (
			stream, response, cancellable, error);
		success = (item_value != NULL);

	/* RFC 5465 "NOTIFY" */
	} else if (g_strcmp0 (item_tag, "OLDNAME") == 0) {
		item_value = imapx_list_response_parse_oldname (
			stream, response, cancellable, error);
		success = (item_value != NULL);

	} else {
		success = camel_imapx_stream_skip_until (
			stream, ")", cancellable, error);
	}

	if (item_value != NULL) {
		/* Takes ownership of the item_tag string. */
		g_hash_table_insert (
			response->priv->extended_items,
			item_tag, g_variant_ref_sink (item_value));
	} else {
		g_free (item_tag);
	}

	return success;
}

/**
 * camel_imapx_list_response_new:
 * @stream: a #CamelIMAPXStream
 * @cancellable: a #GCancellable
 * @error: return location for a #GError, or %NULL
 *
 * Attempts to parse an IMAP LIST response from @stream and, if successful,
 * stores the response data in a new #CamelIMAPXListResponse.  If an error
 * occurs, the function sets @error and returns %NULL.
 *
 * Returns: a #CamelIMAPXListResponse, or %NULL
 *
 * Since: 3.10
 **/
CamelIMAPXListResponse *
camel_imapx_list_response_new (CamelIMAPXStream *stream,
                               GCancellable *cancellable,
                               GError **error)
{
	CamelIMAPXListResponse *response;
	camel_imapx_token_t tok;
	guchar *token;
	guint len;
	const gchar *attribute;

	g_return_val_if_fail (CAMEL_IS_IMAPX_STREAM (stream), NULL);

	response = g_object_new (CAMEL_TYPE_IMAPX_LIST_RESPONSE, NULL);

	/* Parse attributes. */

	tok = camel_imapx_stream_token (
		stream, &token, &len, cancellable, error);
	if (tok == IMAPX_TOK_ERROR)
		goto fail;
	if (tok != '(') {
		g_set_error (
			error, CAMEL_IMAPX_ERROR, 1,
			"list: expecting '('");
		goto fail;
	}

	tok = camel_imapx_stream_token (
		stream, &token, &len, cancellable, error);
	while (tok == IMAPX_TOK_STRING || tok == IMAPX_TOK_TOKEN) {
		camel_imapx_list_response_add_attribute (
			response, (gchar *) token);
		tok = camel_imapx_stream_token (
			stream, &token, &len, cancellable, error);
	}

	if (tok == IMAPX_TOK_ERROR)
		goto fail;
	if (tok != ')') {
		g_set_error (
			error, CAMEL_IMAPX_ERROR, 1,
			"list: expecting ')'");
		goto fail;
	}

	/* Add implied attributes (see RFC 5258 section 3.4). */

	/* "\NoInferiors" implies "\HasNoChildren" */
	attribute = CAMEL_IMAPX_LIST_ATTR_NOINFERIORS;
	if (camel_imapx_list_response_has_attribute (response, attribute)) {
		attribute = CAMEL_IMAPX_LIST_ATTR_HASNOCHILDREN;
		camel_imapx_list_response_add_attribute (response, attribute);
	}

	/* "\NonExistent" implies "\NoSelect" */
	attribute = CAMEL_IMAPX_LIST_ATTR_NONEXISTENT;
	if (camel_imapx_list_response_has_attribute (response, attribute)) {
		attribute = CAMEL_IMAPX_LIST_ATTR_NOSELECT;
		camel_imapx_list_response_add_attribute (response, attribute);
	}

	/* Parse separator. */

	if (!camel_imapx_stream_nstring (stream, &token, cancellable, error))
		goto fail;

	if (token != NULL)
		response->priv->separator = *token;
	else
		response->priv->separator = '\0';

	/* Parse mailbox. */

	response->priv->mailbox =
		camel_imapx_parse_mailbox (stream, cancellable, error);
	if (response->priv->mailbox == NULL)
		goto fail;

	/* Parse extended info (optional). */

	tok = camel_imapx_stream_token (
		stream, &token, &len, cancellable, error);
	if (tok == IMAPX_TOK_ERROR)
		goto fail;
	if (tok == '(') {
		gboolean success;

extended_item_repeat:
		success = imapx_list_response_parse_extended_item (
			stream, response, cancellable, error);
		if (!success)
			goto fail;

		tok = camel_imapx_stream_token (
			stream, &token, &len, cancellable, error);
		if (tok == IMAPX_TOK_ERROR)
			goto fail;
		if (tok != ')') {
			camel_imapx_stream_ungettoken (
				stream, tok, token, len);
			goto extended_item_repeat;
		}

	} else if (tok == '\n') {
		camel_imapx_stream_ungettoken (stream, tok, token, len);

	} else {
		g_set_error (
			error, CAMEL_IMAPX_ERROR, 1,
			"list: expecting '(' or NEWLINE");
		goto fail;
	}

	return response;

fail:
	g_clear_object (&response);

	return NULL;
}

/**
 * camel_imapx_list_response_hash:
 * @response: a #CamelIMAPXListResponse
 *
 * Generates a hash value for @response based on the mailbox name.  This
 * function is intended for easily hashing a #CamelIMAPXListResponse to
 * add to a #GHashTable or similar data structure.
 *
 * Returns: a hash value for @response
 *
 * Since: 3.10
 **/
guint
camel_imapx_list_response_hash (CamelIMAPXListResponse *response)
{
	const gchar *mailbox;

	mailbox = camel_imapx_list_response_get_mailbox (response);

	return g_str_hash (mailbox);
}

/**
 * camel_imapx_list_response_equal:
 * @response_a: the first #CamelIMAPXListResponse
 * @response_b: the second #CamelIMAPXListResponse
 *
 * Checks two #CamelIMAPXListResponse instances for equality based on
 * their mailbox names.
 *
 * Returns: %TRUE if @response_a and @response_b are equal
 *
 * Since: 3.10
 **/
gboolean
camel_imapx_list_response_equal (CamelIMAPXListResponse *response_a,
                                 CamelIMAPXListResponse *response_b)
{
	const gchar *mailbox_a;
	const gchar *mailbox_b;

	mailbox_a = camel_imapx_list_response_get_mailbox (response_a);
	mailbox_b = camel_imapx_list_response_get_mailbox (response_b);

	return g_str_equal (mailbox_a, mailbox_b);
}

/**
 * camel_imapx_list_response_compare:
 * @response_a: the first #CamelIMAPXListResponse
 * @response_b: the second #CamelIMAPXListResponse
 *
 * Compares two #CamelIMAPXListResponse instances by their mailbox names.
 *
 * Returns: a negative value if @response_a compares before @response_b,
 *          zero if they compare equal, or a positive value if @response_a
 *          compares after @response_b
 *
 * Since: 3.10
 **/
gint
camel_imapx_list_response_compare (CamelIMAPXListResponse *response_a,
                                   CamelIMAPXListResponse *response_b)
{
	const gchar *mailbox_a;
	const gchar *mailbox_b;

	mailbox_a = camel_imapx_list_response_get_mailbox (response_a);
	mailbox_b = camel_imapx_list_response_get_mailbox (response_b);

	return g_strcmp0 (mailbox_a, mailbox_b);
}

/**
 * camel_imapx_list_response_get_mailbox:
 * @response: a #CamelIMAPXListResponse
 *
 * Returns the mailbox name for @response.
 *
 * Returns: the mailbox name
 *
 * Since: 3.10
 **/
const gchar *
camel_imapx_list_response_get_mailbox (CamelIMAPXListResponse *response)
{
	g_return_val_if_fail (CAMEL_IS_IMAPX_LIST_RESPONSE (response), NULL);

	return response->priv->mailbox;
}

/**
 * camel_imapx_list_response_get_separator:
 * @response: a #CamelIMAPXListResponse
 *
 * Returns the mailbox path separator character for @response.
 *
 * Returns: the mailbox path separator character
 *
 * Since: 3.10
 **/
gchar
camel_imapx_list_response_get_separator (CamelIMAPXListResponse *response)
{
	g_return_val_if_fail (CAMEL_IS_IMAPX_LIST_RESPONSE (response), '\0');

	return response->priv->separator;
}

/* Flag macros appear here in the reference manual,
 * so also documenting them here in the source code
 * since the documentation is so repetitive. */

/**
 * CAMEL_IMAPX_LIST_ATTR_NOINFERIORS:
 *
 * Refer to <ulink url="http://tools.ietf.org/html/rfc3501#section-7.2.2">
 * RFC 3501 section 7.2.2</ulink>.
 *
 * Since: 3.10
 **/

/**
 * CAMEL_IMAPX_LIST_ATTR_NOSELECT:
 *
 * Refer to <ulink url="http://tools.ietf.org/html/rfc3501#section-7.2.2">
 * RFC 3501 section 7.2.2</ulink>.
 *
 * Since: 3.10
 **/

/**
 * CAMEL_IMAPX_LIST_ATTR_MARKED:
 *
 * Refer to <ulink url="http://tools.ietf.org/html/rfc3501#section-7.2.2">
 * RFC 3501 section 7.2.2</ulink>.
 *
 * Since: 3.10
 **/

/**
 * CAMEL_IMAPX_LIST_ATTR_UNMARKED:
 *
 * Refer to <ulink url="http://tools.ietf.org/html/rfc3501#section-7.2.2">
 * RFC 3501 section 7.2.2</ulink>.
 *
 * Since: 3.10
 **/

/**
 * CAMEL_IMAPX_LIST_ATTR_NONEXISTENT:
 *
 * Refer to <ulink url="http://tools.ietf.org/html/rfc5258#section-3">
 * RFC 5258 section 3</ulink>.
 *
 * Since: 3.10
 **/

/**
 * CAMEL_IMAPX_LIST_ATTR_SUBSCRIBED:
 *
 * Refer to <ulink url="http://tools.ietf.org/html/rfc5258#section-3.1">
 * RFC 5258 section 3.1</ulink>.
 *
 * Since: 3.10
 **/

/**
 * CAMEL_IMAPX_LIST_ATTR_REMOTE:
 *
 * Refer to <ulink url="http://tools.ietf.org/html/rfc5258#section-3.1">
 * RFC 5258 section 3.1</ulink>.
 *
 * Since: 3.10
 **/

/**
 * CAMEL_IMAPX_LIST_ATTR_HASCHILDREN:
 *
 * Refer to <ulink url="http://tools.ietf.org/html/rfc5258#section-4">
 * RFC 5258 section 4</ulink>.
 *
 * Since: 3.10
 **/

/**
 * CAMEL_IMAPX_LIST_ATTR_HASNOCHILDREN:
 *
 * Refer to <ulink url="http://tools.ietf.org/html/rfc5258#section-4">
 * RFC 5258 section 4</ulink>.
 *
 * Since: 3.10
 **/

/**
 * camel_imapx_list_response_add_attribute:
 * @response: a #CamelIMAPXListResponse
 * @attribute: a mailbox attribute
 *
 * Adds a mailbox attribute to @response.  The @attribute should be one of
 * the LIST attribute macros defined above.
 *
 * Since: 3.10
 **/
void
camel_imapx_list_response_add_attribute (CamelIMAPXListResponse *response,
                                         const gchar *attribute)
{
	g_return_if_fail (CAMEL_IS_IMAPX_LIST_RESPONSE (response));
	g_return_if_fail (attribute != NULL);

	/* Avoid duplicates. */
	if (camel_imapx_list_response_has_attribute (response, attribute))
		return;

	g_queue_push_tail (
		&response->priv->attributes,
		(gpointer) g_intern_string (attribute));
}

/**
 * camel_imapx_list_response_has_attribute:
 * @response: a #CamelIMAPXListResponse
 * @attribute: a mailbox attribute
 *
 * Returns whether @response includes the given mailbox attribute.  The
 * @attribute should be one of the LIST attribute macros defined above.
 *
 * Returns: %TRUE if @response has @attribute, or else %FALSE
 *
 * Since: 3.10
 **/
gboolean
camel_imapx_list_response_has_attribute (CamelIMAPXListResponse *response,
                                         const gchar *attribute)
{
	GList *match;

	g_return_val_if_fail (CAMEL_IS_IMAPX_LIST_RESPONSE (response), FALSE);
	g_return_val_if_fail (attribute != NULL, FALSE);

	match = g_queue_find_custom (
		&response->priv->attributes, attribute,
		(GCompareFunc) g_ascii_strcasecmp);

	return (match != NULL);
}

/**
 * camel_imapx_list_response_ref_extended_item:
 * @response: a #CamelIMAPXListResponse
 * @extended_item_tag: an extended item tag
 *
 * Returns the extended item value for @extended_item_tag as a #GVariant.
 * The type of the #GVariant depends on the extended item.  If no value
 * for @extended_item_tag exists, the function returns %NULL.
 *
 * The returned #GVariant is referenced for thread-safety and should
 * be unreferenced with g_variant_unref() when finished with it.
 *
 * Returns: a #GVariant, or %NULL
 *
 * Since: 3.10
 **/
GVariant *
camel_imapx_list_response_ref_extended_item (CamelIMAPXListResponse *response,
                                             const gchar *extended_item_tag)
{
	GHashTable *extended_items;
	GVariant *value;

	g_return_val_if_fail (CAMEL_IS_IMAPX_LIST_RESPONSE (response), NULL);
	g_return_val_if_fail (extended_item_tag != NULL, NULL);

	extended_items = response->priv->extended_items;

	value = g_hash_table_lookup (extended_items, extended_item_tag);

	return (value != NULL) ? g_variant_ref (value) : NULL;
}

/**
 * camel_imapx_list_response_get_summary_flags:
 * @response: a #CamelIMAPXListResponse
 *
 * Converts the relevant mailbox attribute flags in @response to a
 * #CamelStoreInfoFlags value.
 *
 * Returns: a #CamelStoreInfoFlags value
 *
 * Since: 3.10
 **/
CamelStoreInfoFlags
camel_imapx_list_response_get_summary_flags (CamelIMAPXListResponse *response)
{
	CamelStoreInfoFlags store_info_flags = 0;
	const gchar *flag;

	g_return_val_if_fail (CAMEL_IS_IMAPX_LIST_RESPONSE (response), 0);

	flag = CAMEL_IMAPX_LIST_ATTR_NOSELECT;
	if (camel_imapx_list_response_has_attribute (response, flag))
		store_info_flags |= CAMEL_STORE_INFO_FOLDER_NOSELECT;

	flag = CAMEL_IMAPX_LIST_ATTR_NOINFERIORS;
	if (camel_imapx_list_response_has_attribute (response, flag))
		store_info_flags |= CAMEL_STORE_INFO_FOLDER_NOINFERIORS;

	flag = CAMEL_IMAPX_LIST_ATTR_HASCHILDREN;
	if (camel_imapx_list_response_has_attribute (response, flag))
		store_info_flags |= CAMEL_STORE_INFO_FOLDER_CHILDREN;

	flag = CAMEL_IMAPX_LIST_ATTR_HASNOCHILDREN;
	if (camel_imapx_list_response_has_attribute (response, flag))
		store_info_flags |= CAMEL_STORE_INFO_FOLDER_NOCHILDREN;

	flag = CAMEL_IMAPX_LIST_ATTR_SUBSCRIBED;
	if (camel_imapx_list_response_has_attribute (response, flag))
		store_info_flags |= CAMEL_STORE_INFO_FOLDER_SUBSCRIBED;

	/* XXX Does "\Marked" mean CAMEL_STORE_INFO_FOLDER_FLAGGED?
	 *     Who the heck knows; the enum value is undocumented. */

	return store_info_flags;
}
