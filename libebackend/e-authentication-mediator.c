/*
 * e-authentication-mediator.c
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
 * SECTION: e-authentication-mediator
 * @include: libebackend/libebackend.h
 * @short_description: Authenticator proxy for remote clients
 *
 * #EAuthenticationMediator runs on the registry D-Bus service.  It mediates
 * authentication attempts between the client requesting authentication and
 * the server-side #EAuthenticationSession interacting with the user and/or
 * secret service.  It implements the #ESourceAuthenticator interface and
 * securely transmits passwords to a remote #ESourceRegistry over D-Bus.
 **/

#include "e-authentication-mediator.h"

/* XXX Yeah, yeah... */
#define GCR_API_SUBJECT_TO_CHANGE

#include <config.h>
#include <glib/gi18n-lib.h>
#include <gcr/gcr-base.h>

/* Private D-Bus classes. */
#include <e-dbus-authenticator.h>

#define E_AUTHENTICATION_MEDIATOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_AUTHENTICATION_MEDIATOR, EAuthenticationMediatorPrivate))

/* How long should clients have to respond before timing out the
 * authentication session?  Need to balance allowing adequate time
 * without blocking other requests too long if a client gets stuck. */
#define INACTIVITY_TIMEOUT (2 * 60)  /* in seconds */

typedef struct _AsyncContext AsyncContext;

struct _EAuthenticationMediatorPrivate {
	GDBusConnection *connection;
	EDBusAuthenticator *interface;
	GcrSecretExchange *secret_exchange;
	gchar *object_path;
	gchar *sender;

	GMutex *shared_data_lock;

	GQueue try_password_queue;
	GQueue wait_for_client_queue;

	gboolean client_is_ready;
	gboolean client_cancelled;
	gboolean client_vanished;

	guint watcher_id;
};

struct _AsyncContext {
	/* These point into the EAuthenticationMediatorPrivate
	 * struct.  Do not free them in async_context_free(). */
	GMutex *shared_data_lock;
	GQueue *operation_queue;

	GCancellable *cancellable;
	gulong cancel_id;
	guint timeout_id;
};

enum {
	PROP_0,
	PROP_CONNECTION,
	PROP_OBJECT_PATH,
	PROP_SENDER
};

/* Forward Declarations */
static void	e_authentication_mediator_initable_init
				(GInitableIface *interface);
static void	e_authentication_mediator_interface_init
				(ESourceAuthenticatorInterface *interface);

G_DEFINE_TYPE_WITH_CODE (
	EAuthenticationMediator,
	e_authentication_mediator,
	G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (
		G_TYPE_INITABLE,
		e_authentication_mediator_initable_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_SOURCE_AUTHENTICATOR,
		e_authentication_mediator_interface_init))

static void
async_context_free (AsyncContext *async_context)
{
	if (async_context->cancellable != NULL) {
		g_cancellable_disconnect (
			async_context->cancellable,
			async_context->cancel_id);
		g_object_unref (async_context->cancellable);
	}

	if (async_context->timeout_id > 0)
		g_source_remove (async_context->timeout_id);

	g_slice_free (AsyncContext, async_context);
}

static void
authentication_mediator_name_vanished_cb (GDBusConnection *connection,
                                          const gchar *name,
                                          gpointer user_data)
{
	EAuthenticationMediator *mediator;
	GSimpleAsyncResult *simple;
	GQueue *queue;

	mediator = E_AUTHENTICATION_MEDIATOR (user_data);

	g_mutex_lock (mediator->priv->shared_data_lock);

	mediator->priv->client_vanished = TRUE;

	queue = &mediator->priv->try_password_queue;

	/* Notify any unfinished try_password() operations. */
	while ((simple = g_queue_pop_head (queue)) != NULL) {
		g_simple_async_result_set_error (
			simple, G_IO_ERROR, G_IO_ERROR_CANCELLED,
			"%s", _("Bus name vanished (client terminated?)"));
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
	}

	queue = &mediator->priv->wait_for_client_queue;

	/* Notify any unfinished wait_for_client() operations. */
	while ((simple = g_queue_pop_head (queue)) != NULL) {
		g_simple_async_result_set_error (
			simple, G_IO_ERROR, G_IO_ERROR_CANCELLED,
			"%s", _("Bus name vanished (client terminated?)"));
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
	}

	g_bus_unwatch_name (mediator->priv->watcher_id);
	mediator->priv->watcher_id = 0;

	g_mutex_unlock (mediator->priv->shared_data_lock);
}

static void
authentication_mediator_cancelled_cb (GCancellable *cancellable,
                                      gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;

	simple = G_SIMPLE_ASYNC_RESULT (user_data);
	async_context = g_simple_async_result_get_op_res_gpointer (simple);

	g_mutex_lock (async_context->shared_data_lock);

	/* Because we called g_simple_async_result_set_check_cancellable(),
	 * g_simple_async_result_propagate_error() will automatically set a
	 * cancelled error so we don't need to explicitly set one here. */
	if (g_queue_remove (async_context->operation_queue, simple)) {
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
	}

	g_mutex_unlock (async_context->shared_data_lock);
}

static gboolean
authentication_mediator_timeout_cb (gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;

	simple = G_SIMPLE_ASYNC_RESULT (user_data);
	async_context = g_simple_async_result_get_op_res_gpointer (simple);

	g_mutex_lock (async_context->shared_data_lock);

	if (g_queue_remove (async_context->operation_queue, simple)) {
		g_simple_async_result_set_error (
			simple, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
			"%s", _("No response from client"));
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
	}

	g_mutex_unlock (async_context->shared_data_lock);

	return FALSE;
}

static gboolean
authentication_mediator_handle_ready (EDBusAuthenticator *interface,
                                      GDBusMethodInvocation *invocation,
                                      const gchar *encrypted_key,
                                      EAuthenticationMediator *mediator)
{
	GcrSecretExchange *secret_exchange;
	GSimpleAsyncResult *simple;
	GQueue *queue;

	g_mutex_lock (mediator->priv->shared_data_lock);

	mediator->priv->client_is_ready = TRUE;

	secret_exchange = mediator->priv->secret_exchange;
	gcr_secret_exchange_receive (secret_exchange, encrypted_key);

	queue = &mediator->priv->wait_for_client_queue;

	/* Notify any unfinished wait_for_client() operations. */
	while ((simple = g_queue_pop_head (queue)) != NULL) {
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
	}

	g_mutex_unlock (mediator->priv->shared_data_lock);

	e_dbus_authenticator_complete_ready (interface, invocation);

	return TRUE;
}

static gboolean
authentication_mediator_handle_cancel (EDBusAuthenticator *interface,
                                       GDBusMethodInvocation *invocation,
                                       EAuthenticationMediator *mediator)
{
	GSimpleAsyncResult *simple;
	GQueue *queue;

	g_mutex_lock (mediator->priv->shared_data_lock);

	mediator->priv->client_cancelled = TRUE;

	queue = &mediator->priv->try_password_queue;

	/* Notify any unfinished try_password() operations. */
	while ((simple = g_queue_pop_head (queue)) != NULL) {
		g_simple_async_result_set_error (
			simple, G_IO_ERROR, G_IO_ERROR_CANCELLED,
			"%s", _("Client cancelled the operation"));
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
	}

	queue = &mediator->priv->wait_for_client_queue;

	/* Notify any unfinished wait_for_client() operations. */
	while ((simple = g_queue_pop_head (queue)) != NULL) {
		g_simple_async_result_set_error (
			simple, G_IO_ERROR, G_IO_ERROR_CANCELLED,
			"%s", _("Client cancelled the operation"));
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
	}

	g_mutex_unlock (mediator->priv->shared_data_lock);

	e_dbus_authenticator_complete_cancel (interface, invocation);

	return TRUE;
}

static gboolean
authentication_mediator_handle_accepted (EDBusAuthenticator *interface,
                                         GDBusMethodInvocation *invocation,
                                         EAuthenticationMediator *mediator)
{
	GSimpleAsyncResult *simple;
	GQueue *queue;

	g_mutex_lock (mediator->priv->shared_data_lock);

	queue = &mediator->priv->try_password_queue;

	if (g_queue_is_empty (queue))
		g_warning ("%s: Unexpected 'accepted' signal", G_STRFUNC);

	/* Notify any unfinished try_password() operations. */
	while ((simple = g_queue_pop_head (queue)) != NULL) {
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
	}

	g_mutex_unlock (mediator->priv->shared_data_lock);

	e_dbus_authenticator_complete_accepted (interface, invocation);

	return TRUE;
}

static gboolean
authentication_mediator_handle_rejected (EDBusAuthenticator *interface,
                                         GDBusMethodInvocation *invocation,
                                         EAuthenticationMediator *mediator)
{
	GSimpleAsyncResult *simple;
	GQueue *queue;

	g_mutex_lock (mediator->priv->shared_data_lock);

	queue = &mediator->priv->try_password_queue;

	if (g_queue_is_empty (queue))
		g_warning ("%s: Unexpected 'rejected' signal", G_STRFUNC);

	/* Notify any unfinished try_password() operations. */
	while ((simple = g_queue_pop_head (queue)) != NULL) {
		g_simple_async_result_set_error (
			simple, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
			"%s", _("Client reports password was rejected"));
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
	}

	g_mutex_unlock (mediator->priv->shared_data_lock);

	e_dbus_authenticator_complete_rejected (interface, invocation);

	return TRUE;
}

static void
authentication_mediator_set_connection (EAuthenticationMediator *mediator,
                                        GDBusConnection *connection)
{
	g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
	g_return_if_fail (mediator->priv->connection == NULL);

	mediator->priv->connection = g_object_ref (connection);
}

static void
authentication_mediator_set_object_path (EAuthenticationMediator *mediator,
                                         const gchar *object_path)
{
	g_return_if_fail (object_path != NULL);
	g_return_if_fail (mediator->priv->object_path == NULL);

	mediator->priv->object_path = g_strdup (object_path);
}

static void
authentication_mediator_set_sender (EAuthenticationMediator *mediator,
                                    const gchar *sender)
{
	g_return_if_fail (sender != NULL);
	g_return_if_fail (mediator->priv->sender == NULL);

	mediator->priv->sender = g_strdup (sender);
}

static void
authentication_mediator_set_property (GObject *object,
                                      guint property_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CONNECTION:
			authentication_mediator_set_connection (
				E_AUTHENTICATION_MEDIATOR (object),
				g_value_get_object (value));
			return;

		case PROP_OBJECT_PATH:
			authentication_mediator_set_object_path (
				E_AUTHENTICATION_MEDIATOR (object),
				g_value_get_string (value));
			return;

		case PROP_SENDER:
			authentication_mediator_set_sender (
				E_AUTHENTICATION_MEDIATOR (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
authentication_mediator_get_property (GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CONNECTION:
			g_value_set_object (
				value,
				e_authentication_mediator_get_connection (
				E_AUTHENTICATION_MEDIATOR (object)));
			return;

		case PROP_OBJECT_PATH:
			g_value_set_string (
				value,
				e_authentication_mediator_get_object_path (
				E_AUTHENTICATION_MEDIATOR (object)));
			return;

		case PROP_SENDER:
			g_value_set_string (
				value,
				e_authentication_mediator_get_sender (
				E_AUTHENTICATION_MEDIATOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
authentication_mediator_dispose (GObject *object)
{
	EAuthenticationMediatorPrivate *priv;
	GQueue *queue;

	priv = E_AUTHENTICATION_MEDIATOR_GET_PRIVATE (object);

	if (priv->connection != NULL) {
		g_object_unref (priv->connection);
		priv->connection = NULL;
	}

	if (priv->interface != NULL) {
		g_object_unref (priv->interface);
		priv->interface = NULL;
	}

	if (priv->secret_exchange != NULL) {
		g_object_unref (priv->secret_exchange);
		priv->secret_exchange = NULL;
	}

	queue = &priv->wait_for_client_queue;

	while (!g_queue_is_empty (queue))
		g_object_unref (g_queue_pop_head (queue));

	if (priv->watcher_id > 0) {
		g_bus_unwatch_name (priv->watcher_id);
		priv->watcher_id = 0;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_authentication_mediator_parent_class)->
		dispose (object);
}

static void
authentication_mediator_finalize (GObject *object)
{
	EAuthenticationMediatorPrivate *priv;

	priv = E_AUTHENTICATION_MEDIATOR_GET_PRIVATE (object);

	g_mutex_free (priv->shared_data_lock);

	g_free (priv->object_path);
	g_free (priv->sender);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_authentication_mediator_parent_class)->
		finalize (object);
}

static void
authentication_mediator_constructed (GObject *object)
{
	EAuthenticationMediator *mediator;
	GDBusConnection *connection;
	const gchar *sender;

	mediator = E_AUTHENTICATION_MEDIATOR (object);
	connection = e_authentication_mediator_get_connection (mediator);
	sender = e_authentication_mediator_get_sender (mediator);

	/* This should notify us if the client process terminates. */
	mediator->priv->watcher_id =
		g_bus_watch_name_on_connection (
			connection, sender,
			G_BUS_NAME_WATCHER_FLAGS_NONE,
			NULL,
			authentication_mediator_name_vanished_cb,
			mediator, (GDestroyNotify) NULL);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_authentication_mediator_parent_class)->
		constructed (object);
}

static gboolean
authentication_mediator_initable_init (GInitable *initable,
                                       GCancellable *cancellable,
                                       GError **error)
{
	EAuthenticationMediator *mediator;
	GDBusInterfaceSkeleton *interface;
	GDBusConnection *connection;
	const gchar *object_path;

	mediator = E_AUTHENTICATION_MEDIATOR (initable);

	interface = G_DBUS_INTERFACE_SKELETON (mediator->priv->interface);
	connection = e_authentication_mediator_get_connection (mediator);
	object_path = e_authentication_mediator_get_object_path (mediator);

	return g_dbus_interface_skeleton_export (
		interface, connection, object_path, error);
}

static ESourceAuthenticationResult
authentication_mediator_try_password_sync (ESourceAuthenticator *auth,
                                           const GString *password,
                                           GCancellable *cancellable,
                                           GError **error)
{
	ESourceAuthenticationResult auth_result;
	GAsyncResult *async_result;
	EAsyncClosure *closure;

	closure = e_async_closure_new ();

	e_source_authenticator_try_password (
		auth, password, cancellable,
		e_async_closure_callback, closure);

	async_result = e_async_closure_wait (closure);

	auth_result = e_source_authenticator_try_password_finish (
		auth, async_result, error);

	e_async_closure_free (closure);

	return auth_result;
}

static void
authentication_mediator_try_password (ESourceAuthenticator *auth,
                                      const GString *password,
                                      GCancellable *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
	EAuthenticationMediator *mediator;
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;

	mediator = E_AUTHENTICATION_MEDIATOR (auth);

	async_context = g_slice_new0 (AsyncContext);
	async_context->shared_data_lock = mediator->priv->shared_data_lock;
	async_context->operation_queue = &mediator->priv->try_password_queue;

	simple = g_simple_async_result_new (
		G_OBJECT (mediator), callback, user_data,
		authentication_mediator_try_password);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_set_op_res_gpointer (
		simple, async_context, (GDestroyNotify) async_context_free);

	if (G_IS_CANCELLABLE (cancellable)) {
		async_context->cancellable = g_object_ref (cancellable);
		async_context->cancel_id = g_cancellable_connect (
			async_context->cancellable,
			G_CALLBACK (authentication_mediator_cancelled_cb),
			simple, (GDestroyNotify) NULL);
	}

	async_context->timeout_id = g_timeout_add_seconds (
		INACTIVITY_TIMEOUT,
		authentication_mediator_timeout_cb, simple);

	g_mutex_lock (mediator->priv->shared_data_lock);

	if (mediator->priv->client_cancelled) {
		g_simple_async_result_set_error (
			simple, G_IO_ERROR, G_IO_ERROR_CANCELLED,
			"%s", _("Client cancelled the operation"));
		g_simple_async_result_complete_in_idle (simple);

	} else if (mediator->priv->client_vanished) {
		g_simple_async_result_set_error (
			simple, G_IO_ERROR, G_IO_ERROR_CANCELLED,
			"%s", _("Bus name vanished (client terminated?)"));
		g_simple_async_result_complete_in_idle (simple);

	} else {
		gchar *encrypted_secret;

		g_queue_push_tail (
			async_context->operation_queue,
			g_object_ref (simple));

		encrypted_secret = gcr_secret_exchange_send (
			mediator->priv->secret_exchange, password->str, -1);

		e_dbus_authenticator_emit_authenticate (
			mediator->priv->interface, encrypted_secret);

		g_free (encrypted_secret);
	}

	g_mutex_unlock (mediator->priv->shared_data_lock);

	g_object_unref (simple);
}

static ESourceAuthenticationResult
authentication_mediator_try_password_finish (ESourceAuthenticator *auth,
                                             GAsyncResult *result,
                                             GError **error)
{
	GSimpleAsyncResult *simple;
	GError *local_error = NULL;

	simple = G_SIMPLE_ASYNC_RESULT (result);

	if (!g_simple_async_result_propagate_error (simple, &local_error))
		return E_SOURCE_AUTHENTICATION_ACCEPTED;

	if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED)) {
		g_clear_error (&local_error);
		return E_SOURCE_AUTHENTICATION_REJECTED;
	}

	g_propagate_error (error, local_error);

	return E_SOURCE_AUTHENTICATION_ERROR;
}

static void
e_authentication_mediator_class_init (EAuthenticationMediatorClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EAuthenticationMediatorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = authentication_mediator_set_property;
	object_class->get_property = authentication_mediator_get_property;
	object_class->dispose = authentication_mediator_dispose;
	object_class->finalize = authentication_mediator_finalize;
	object_class->constructed = authentication_mediator_constructed;

	g_object_class_install_property (
		object_class,
		PROP_CONNECTION,
		g_param_spec_object (
			"connection",
			"Connection",
			"The GDBusConnection on which to "
			"export the authenticator interface",
			G_TYPE_DBUS_CONNECTION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_OBJECT_PATH,
		g_param_spec_string (
			"object-path",
			"Object Path",
			"The object path at which to "
			"export the authenticator interface",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SENDER,
		g_param_spec_string (
			"sender",
			"Sender",
			"Unique bus name of the process that "
			"initiated the authentication session",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_authentication_mediator_initable_init (GInitableIface *interface)
{
	interface->init = authentication_mediator_initable_init;
}

static void
e_authentication_mediator_interface_init (ESourceAuthenticatorInterface *interface)
{
	interface->try_password_sync = authentication_mediator_try_password_sync;
	interface->try_password = authentication_mediator_try_password;
	interface->try_password_finish = authentication_mediator_try_password_finish;
}

static void
e_authentication_mediator_init (EAuthenticationMediator *mediator)
{
	mediator->priv = E_AUTHENTICATION_MEDIATOR_GET_PRIVATE (mediator);

	mediator->priv->interface = e_dbus_authenticator_skeleton_new ();
	mediator->priv->secret_exchange = gcr_secret_exchange_new (NULL);

	mediator->priv->shared_data_lock = g_mutex_new ();

	g_dbus_interface_skeleton_set_flags (
		G_DBUS_INTERFACE_SKELETON (mediator->priv->interface),
		G_DBUS_INTERFACE_SKELETON_FLAGS_HANDLE_METHOD_INVOCATIONS_IN_THREAD);

	g_signal_connect (
		mediator->priv->interface, "handle-ready",
		G_CALLBACK (authentication_mediator_handle_ready), mediator);

	g_signal_connect (
		mediator->priv->interface, "handle-cancel",
		G_CALLBACK (authentication_mediator_handle_cancel), mediator);

	g_signal_connect (
		mediator->priv->interface, "handle-accepted",
		G_CALLBACK (authentication_mediator_handle_accepted), mediator);

	g_signal_connect (
		mediator->priv->interface, "handle-rejected",
		G_CALLBACK (authentication_mediator_handle_rejected), mediator);
}

/**
 * e_authentication_mediator_new:
 * @connection: a #GDBusConnection
 * @object_path: object path of the authentication session
 * @sender: bus name of the client requesting authentication
 * @error: return location for a #GError, or %NULL
 *
 * Creates a new #EAuthenticationMediator and exports the Authenticator
 * D-Bus interface on @connection at @object_path.  If the Authenticator
 * interface fails to export, the function sets @error and returns %NULL.
 *
 * #EAuthenticationMediator watches the bus name of the client requesting
 * authentication, given by @sender.  If it sees the bus name vanish, it
 * cancels the authentication session so the next authentication session
 * can begin without delay.
 *
 * Returns: an #EAuthenticationMediator, or %NULL on error
 *
 * Since: 3.6
 **/
ESourceAuthenticator *
e_authentication_mediator_new (GDBusConnection *connection,
                               const gchar *object_path,
                               const gchar *sender,
                               GError **error)
{
	g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
	g_return_val_if_fail (object_path != NULL, NULL);
	g_return_val_if_fail (sender != NULL, NULL);

	return g_initable_new (
		E_TYPE_AUTHENTICATION_MEDIATOR, NULL, error,
		"connection", connection,
		"object-path", object_path,
		"sender", sender, NULL);
}

/**
 * e_authentication_mediator_get_connection:
 * @mediator: an #EAuthenticationMediator
 *
 * Returns the #GDBusConnection on which the Authenticator D-Bus interface
 * is exported.
 *
 * Returns: the #GDBusConnection
 *
 * Since: 3.6
 **/
GDBusConnection *
e_authentication_mediator_get_connection (EAuthenticationMediator *mediator)
{
	g_return_val_if_fail (E_IS_AUTHENTICATION_MEDIATOR (mediator), NULL);

	return mediator->priv->connection;
}

/**
 * e_authentication_mediator_get_object_path:
 * @mediator: an #EAuthenticationMediator
 *
 * Returns the object path at which the Authenticator D-Bus interface is
 * exported.
 *
 * Returns: the object path
 *
 * Since: 3.6
 **/
const gchar *
e_authentication_mediator_get_object_path (EAuthenticationMediator *mediator)
{
	g_return_val_if_fail (E_IS_AUTHENTICATION_MEDIATOR (mediator), NULL);

	return mediator->priv->object_path;
}

/**
 * e_authentication_mediator_get_sender:
 * @mediator: an #EAuthenticationMediator
 *
 * Returns the authentication client's unique bus name.
 *
 * Returns: the client's bus name
 *
 * Since: 3.6
 **/
const gchar *
e_authentication_mediator_get_sender (EAuthenticationMediator *mediator)
{
	g_return_val_if_fail (E_IS_AUTHENTICATION_MEDIATOR (mediator), NULL);

	return mediator->priv->sender;
}

/**
 * e_authentication_mediator_wait_for_client_sync:
 * @mediator: an #EAuthenticationMediator
 * @cancellable: optional #GCancellable object, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Waits for the authentication client to indicate it is ready to begin
 * authentication attempts.  Call this function to synchronize with the
 * client before initiating any authentication attempts through @mediator.
 *
 * If the authentication client's bus name vanishes or the client fails
 * to signal it is ready before a timer expires, the function sets @error
 * and returns %FALSE.
 *
 * Returns: %TRUE if the client is ready, %FALSE if an error occurred
 *
 * Since: 3.6
 **/
gboolean
e_authentication_mediator_wait_for_client_sync (EAuthenticationMediator *mediator,
                                                GCancellable *cancellable,
                                                GError **error)
{
	EAsyncClosure *closure;
	GAsyncResult *result;
	gboolean success;

	g_return_val_if_fail (E_IS_AUTHENTICATION_MEDIATOR (mediator), FALSE);

	closure = e_async_closure_new ();

	e_authentication_mediator_wait_for_client (
		mediator, cancellable, e_async_closure_callback, closure);

	result = e_async_closure_wait (closure);

	success = e_authentication_mediator_wait_for_client_finish (
		mediator, result, error);

	e_async_closure_free (closure);

	return success;
}

/**
 * e_authentication_mediator_wait_for_client:
 * @mediator: an #EAuthenticationMediator
 * @cancellable: optional #GCancellable object, or %NULL
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: data to pass to the callback function
 *
 * Asynchronously waits for the authentication client to indicate it
 * is ready to being authentication attempts.  Call this function to
 * synchronize with the client before initiating any authentication
 * attempts through @mediator.
 *
 * When the operation is finished, @callback will be called.  You can then
 * call e_authentication_mediator_wait_for_client_finished() to get the
 * result of the operation.
 *
 * Since: 3.6
 **/
void
e_authentication_mediator_wait_for_client (EAuthenticationMediator *mediator,
                                           GCancellable *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;

	g_return_if_fail (E_IS_AUTHENTICATION_MEDIATOR (mediator));

	async_context = g_slice_new0 (AsyncContext);
	async_context->shared_data_lock = mediator->priv->shared_data_lock;
	async_context->operation_queue = &mediator->priv->wait_for_client_queue;

	simple = g_simple_async_result_new (
		G_OBJECT (mediator), callback, user_data,
		e_authentication_mediator_wait_for_client);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_set_op_res_gpointer (
		simple, async_context, (GDestroyNotify) async_context_free);

	if (G_IS_CANCELLABLE (cancellable)) {
		async_context->cancellable = g_object_ref (cancellable);
		async_context->cancel_id = g_cancellable_connect (
			cancellable,
			G_CALLBACK (authentication_mediator_cancelled_cb),
			simple, (GDestroyNotify) NULL);
	}

	async_context->timeout_id = g_timeout_add_seconds (
		INACTIVITY_TIMEOUT, authentication_mediator_timeout_cb, simple);

	g_mutex_lock (mediator->priv->shared_data_lock);

	if (mediator->priv->client_is_ready) {
		g_simple_async_result_complete_in_idle (simple);

	} else if (mediator->priv->client_cancelled) {
		g_simple_async_result_set_error (
			simple, G_IO_ERROR, G_IO_ERROR_CANCELLED,
			"%s", _("Client cancelled the operation"));
		g_simple_async_result_complete_in_idle (simple);

	} else if (mediator->priv->client_vanished) {
		g_simple_async_result_set_error (
			simple, G_IO_ERROR, G_IO_ERROR_CANCELLED,
			"%s", _("Bus name vanished (client terminated?)"));
		g_simple_async_result_complete_in_idle (simple);

	} else {
		g_queue_push_tail (
			async_context->operation_queue,
			g_object_ref (simple));
	}

	g_mutex_unlock (mediator->priv->shared_data_lock);

	g_object_unref (simple);
}

/**
 * e_authentication_mediator_wait_for_client_finish:
 * @mediator: an #EAuthenticationMediator
 * @result: a #GAsyncResult
 * @error: return location for a #GError, or %NULL
 *
 * Finishes the operation started with
 * e_authentication_mediator_wait_for_client().
 *
 * If the authentication client's bus name vanishes or the client fails
 * to signal it is ready before a timer expires, the function sets @error
 * and returns %FALSE.
 *
 * Returns: %TRUE if the client is ready, %FALSE if an error occurred
 *
 * Since: 3.6
 **/
gboolean
e_authentication_mediator_wait_for_client_finish (EAuthenticationMediator *mediator,
                                                  GAsyncResult *result,
                                                  GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (mediator),
		e_authentication_mediator_wait_for_client), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	/* Assume success unless a GError is set. */
	return !g_simple_async_result_propagate_error (simple, error);
}

/**
 * e_authentication_mediator_dismiss:
 * @mediator: an #EAuthenticationMediator
 *
 * Signals to the authentication client that the user declined to provide a
 * password when prompted and that the authentication session has terminated.
 * This is also called when a server-side error has occurred, but the client
 * doesn't need to know the difference.
 *
 * Since: 3.6
 **/
void
e_authentication_mediator_dismiss (EAuthenticationMediator *mediator)
{
	g_return_if_fail (E_IS_AUTHENTICATION_MEDIATOR (mediator));

	e_dbus_authenticator_emit_dismissed (mediator->priv->interface);
}
