#include "base/nugu_plugin.h"

static int on_load(void)
{
	return 0;
}

static void on_unload(NuguPlugin *p)
{
}

static int on_init(NuguPlugin *p)
{
	return 0;
}

/* Function defined to call directly from the application. */
NUGU_API_EXPORT int custom_add(int a, int b)
{
	return a + b;
}

NUGU_PLUGIN_DEFINE(test_nugu_custom, NUGU_PLUGIN_PRIORITY_DEFAULT, "1.0",
		   on_load, on_unload, on_init);
