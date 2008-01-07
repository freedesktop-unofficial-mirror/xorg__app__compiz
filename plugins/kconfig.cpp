/*
 * Copyright © 2007 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <kglobal.h>
#include <kstandarddirs.h>
#include <kapplication.h>
#include <ksimpleconfig.h>
#include <qfile.h>

#include <compiz/core.h>
#include <compiz/c-object.h>

#define COMPIZ_KCONFIG_VERSION 20071123

#define COMPIZ_KCONFIG_RC "compizrc"

static KInstance *kInstance;

static int kconfigCorePrivateIndex;

typedef struct _KconfigCore {
    CompInterfaceData	base;
    KConfig		*config;
    CompTimeoutHandle   syncHandle;
    CompTimeoutHandle   reloadHandle;
    CompFileWatchHandle fileWatch;
} KconfigCore;

#define GET_KCONFIG_CORE(c)						   \
    ((KconfigCore *) (c)->data.base.privates[kconfigCorePrivateIndex].ptr)

#define KCONFIG_CORE(c)			   \
    KconfigCore *kc = GET_KCONFIG_CORE (c)


static CInterface kconfigCoreInterface[] = {
    C_INTERFACE (kconfig, Core, CompCoreVTable, _, _, _, _, _, _, _, _)
};

static QString
kconfigObjectPath (CompObject *object)
{
    QString objectName = QString (object->name);

    if (!object->parent || strcmp (object->parent->name, "core"))
	return objectName;

    return kconfigObjectPath (object->parent) + "_" + objectName;
}

static QString
kconfigGroup (CompObject *object,
	      const char *interface)
{
    QString path = kconfigObjectPath (object);

    /* include interface name when it's not part of type */
    if (compObjectInterfaceIsPartOfType (object, interface))
	return path;

    return path + "_" + QString (interface);
}

static CompBool
kconfigReadProp (CompObject *object,
		 KConfig    *config,
		 const char *interface,
		 const char *name,
		 int	    type)
{
    const CompPropertiesVTable *vTable = &object->vTable->properties;

    switch (type) {
    case COMP_TYPE_BOOLEAN:
	return (*vTable->setBool) (object,
				   interface,
				   name,
				   config->readBoolEntry (name),
				   NULL);
    case COMP_TYPE_INT32:
	return (*vTable->setInt) (object,
				  interface,
				  name,
				  config->readNumEntry (name),
				  NULL);
    case COMP_TYPE_DOUBLE:
	return (*vTable->setDouble) (object,
				     interface,
				     name,
				     config->readDoubleNumEntry (name),
				     NULL);
    case COMP_TYPE_STRING:
	return (*vTable->setString) (object,
				     interface,
				     name,
				     config->readEntry (name),
				     NULL);
    }

    return FALSE;
}

typedef struct _ReloadPropContext {
    CompCore   *core;
    const char *interface;
} ReloadPropContext;

static CompBool
kconfigReloadProp (CompObject *object,
		   const char *name,
		   int	      type,
		   void	      *closure)
{
    ReloadPropContext *pCtx = (ReloadPropContext *) closure;
    CompCore          *c = pCtx->core;

    KCONFIG_CORE (c);

    if (kc->config->hasKey (name))
	kconfigReadProp (object, kc->config, pCtx->interface, name, type);

    return TRUE;
}

typedef struct _ReloadInterfaceContext {
    CompCore   *core;
    CompObject *object;
} ReloadInterfaceContext;

static CompBool
kconfigReloadInterface (CompObject	     *object,
			const char	     *name,
			size_t		     offset,
			const CompObjectType *type,
			void		     *closure)
{
    ReloadPropContext ctx;
    CompCore          *c = (CompCore *) closure;

    KCONFIG_CORE (c);

    ctx.core      = c;
    ctx.interface = name;

    kc->config->setGroup (kconfigGroup (object, name));

    (*object->vTable->forEachProp) (object, name,
				    kconfigReloadProp,
				    (void *) &ctx);

    return TRUE;
}

static CompBool
kconfigReloadObjectTree (CompObject *object,
			 void	    *closure)
{
    (*object->vTable->forEachInterface) (object,
					 kconfigReloadInterface,
					 closure);

    (*object->vTable->forEachChildObject) (object,
					   kconfigReloadObjectTree,
					   closure);

    return TRUE;
}

static Bool
kconfigRcReload (void *closure)
{
    CompCore *c = (CompCore *) closure;

    KCONFIG_CORE (c);

    kc->config->reparseConfiguration ();

    kconfigReloadObjectTree (&c->u.base.u.base, closure);

    kc->reloadHandle = 0;

    return FALSE;
}

static Bool
kconfigRcSync (void *closure)
{
    KCONFIG_CORE (&core);

    kc->config->sync ();

    kc->syncHandle = 0;

    return FALSE;
}

static void
kconfigPropChanged (CompCore   *c,
		    CompObject *object,
		    const char *signal,
		    const char *signature,
		    va_list    args)
{
    const char *interface;
    const char *name;
    va_list    ap;

    KCONFIG_CORE (c);

    va_copy (ap, args);

    interface = va_arg (ap, const char *);
    name      = va_arg (ap, const char *);

    kc->config->setGroup (kconfigGroup (object, interface));

    if (signature[2] == 'b' && strcmp (signal, "boolChanged") == 0)
	kc->config->writeEntry (name, va_arg (ap, CompBool));
    else if (signature[2] == 'i' && strcmp (signal, "intChanged") == 0)
	kc->config->writeEntry (name, va_arg (ap, int32_t));
    else if (signature[2] == 'd' && strcmp (signal, "doubleChanged") == 0)
	kc->config->writeEntry (name, va_arg (ap, double));
    else if (signature[2] == 's' && strcmp (signal, "stringChanged") == 0)
	kc->config->writeEntry (name, va_arg (ap, const char *));

    va_end (ap);

    if (!kc->syncHandle)
	kc->syncHandle = compAddTimeout (0, kconfigRcSync, 0);
}

static void
kconfigHandleSignal (CompObject *object,
		     void	*data,
		     ...)
{
    CompObject *source;
    const char *interface;
    const char *name;
    const char *signature;
    va_list    ap, args;

    CORE (object);

    va_start (ap, data);

    source    = va_arg (ap, CompObject *);
    interface = va_arg (ap, const char *);
    name      = va_arg (ap, const char *);
    signature = va_arg (ap, const char *);
    args      = va_arg (ap, va_list);

    va_end (ap);

    if (strcmp (interface, "object") == 0)
    {
	if (strcmp (name,      "childObjectAdded") == 0 &&
	    strcmp (signature, "o")		   == 0)
	{
	    va_copy (ap, args);
	    kconfigReloadObjectTree (va_arg (ap, CompObject *), (void *) c);
	    va_end (ap);
	}
    }
    else if (strcmp (interface, "properties") == 0)
    {
	if (strncmp (signature, "ss", 2) == 0)
	    kconfigPropChanged (c, source, name, signature, args);
    }
}

static void
kconfigRcChanged (const char *name,
		  void	     *closure)
{
    if (strcmp (name, COMPIZ_KCONFIG_RC) == 0)
    {
	KCONFIG_CORE (&core);

	if (!kc->reloadHandle)
	    kc->reloadHandle = compAddTimeout (0, kconfigRcReload, closure);
    }
}

static CompBool
kconfigInitCore (CompObject *object)
{
    QString dir;

    KCONFIG_CORE (GET_CORE (object));

    kc->config = new KConfig (COMPIZ_KCONFIG_RC);
    if (!kc->config)
	return FALSE;

    kc->reloadHandle = compAddTimeout (0, kconfigRcReload, 0);
    kc->syncHandle   = 0;
    kc->fileWatch    = 0;

    dir = KGlobal::dirs ()->saveLocation ("config", QString::null, false);

    if (QFile::exists (dir))
	kc->fileWatch = addFileWatch (dir.ascii (), ~0, kconfigRcChanged, 0);

    return TRUE;
}

static void
kconfigFiniCore (CompObject *object)
{
    KCONFIG_CORE (GET_CORE (object));

    if (kc->reloadHandle)
	compRemoveTimeout (kc->reloadHandle);

    if (kc->syncHandle)
    {
	compRemoveTimeout (kc->syncHandle);
	kconfigRcSync (0);
    }

    if (kc->fileWatch)
	removeFileWatch (kc->fileWatch);

    delete kc->config;
}

static void
kconfigGetProp (CompObject   *object,
	      unsigned int what,
	      void	   *value)
{
    static const CMetadata tmpl = {
	kconfigCoreInterface,
	N_ELEMENTS (kconfigCoreInterface),
	kconfigInitCore,
	kconfigFiniCore,
	NULL, NULL,
	COMPIZ_KCONFIG_VERSION
    };

    CORE (object);

    cGetInterfaceProp (&GET_KCONFIG_CORE (c)->base, &tmpl, what, value);
}

static const CompCoreVTable kconfigCoreObjectVTable = {
    { { NULL, kconfigGetProp } }
};

static CObjectPrivate kconfigObj[] = {
    C_OBJECT_PRIVATE (CORE_TYPE_NAME, kconfig, Core, KconfigCore, X, X)
};

static CompBool
kconfigInsert (CompObject *parent,
	       CompBranch *branch)
{
    if (!cObjectInitPrivates (branch, kconfigObj, N_ELEMENTS (kconfigObj)))
	return FALSE;

    return TRUE;
}

static void
kconfigRemove (CompObject *parent,
	       CompBranch *branch)
{
    cObjectFiniPrivates (branch, kconfigObj, N_ELEMENTS (kconfigObj));
}

static Bool
kconfigInit (CompFactory *factory)
{
    kInstance = new KInstance ("compiz-kconfig");
    if (!kInstance)
	return FALSE;

    if (!cObjectAllocPrivateIndices (factory, kconfigObj,
				     N_ELEMENTS (kconfigObj)))
    {
	delete kInstance;
	return FALSE;
    }

    return TRUE;
}

static void
kconfigFini (CompFactory *factory)
{
    cObjectFreePrivateIndices (factory, kconfigObj, N_ELEMENTS (kconfigObj));
    delete kInstance;
}

CompPluginVTable kconfigVTable = {
    "kconfig",
    0, /* GetMetadata */
    kconfigInit,
    kconfigFini,
    0, /* InitObject */
    0, /* FiniObject */
    0, /* GetObjectOptions */
    0, /* SetObjectOption */
    kconfigInsert,
    kconfigRemove
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &kconfigVTable;
}
