#include "gcfs.h"
#include "gcfs_fuse.h"
#include "gcfs_config.h"
#include "gcfs_task.h"

#include <stdio.h>

GCFS_Config g_sConfig;
GCFS_TaskManager g_sTaskManager;

int main(int argc, char *argv[])
{
	if(!g_sConfig.loadConfig())
		return -1;

	GCFS_Permissions sDefPerm = {0755, getuid(), getgid()};
   g_sTaskManager.Init();
   g_sTaskManager.m_pRootDirectory->mkdir("Test", sDefPerm);
   g_sTaskManager.m_pRootDirectory->mkdir("Test2", sDefPerm);

	return init_fuse(argc, argv);
}
