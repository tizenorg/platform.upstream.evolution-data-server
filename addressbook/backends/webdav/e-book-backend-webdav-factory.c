/* e-book-backend-webdav-factory.c - Webdav contact backend.
 *
 * Copyright (C) 2008 Matthias Braun <matze@braunis.de>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Braun <matze@braunis.de>
 */

#include <config.h>

#include "e-book-backend-webdav.h"

#define FACTORY_NAME "webdav"

typedef EBookBackendFactory EBookBackendWebdavFactory;
typedef EBookBackendFactoryClass EBookBackendWebdavFactoryClass;

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_book_backend_webdav_factory_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EBookBackendWebdavFactory,
	e_book_backend_webdav_factory,
	E_TYPE_BOOK_BACKEND_FACTORY)

static void
e_book_backend_webdav_factory_class_init (EBookBackendFactoryClass *class)
{
	class->factory_name = FACTORY_NAME;
	class->backend_type = E_TYPE_BOOK_BACKEND_WEBDAV;
}

static void
e_book_backend_webdav_factory_class_finalize (EBookBackendFactoryClass *class)
{
}

static void
e_book_backend_webdav_factory_init (EBookBackendFactory *factory)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_book_backend_webdav_factory_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}

