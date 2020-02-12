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

NUGU_PLUGIN_DEFINE("test_nugu", NUGU_PLUGIN_PRIORITY_DEFAULT, "1.0", on_load,
		   on_unload, on_init);
