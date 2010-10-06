#include "gcfs.h"
#include "gcfs_fuse.h"
#include "gcfs_config.h"
#include "gcfs_task.h"

#include <stdio.h>

GCFS_Config g_sConfig;
GCFS_TaskManager g_sTasks;

int main(int argc, char *argv[]){

	g_sConfig.loadConfig();
	
	g_sConfig.AddService("condor", "test");

	GCFS_Permissions sDefPerm = {0755, 0, 0};
	g_sTasks.addTask("Test")->m_sPermissions = sDefPerm;
	g_sTasks.addTask("Test2")->m_sPermissions = sDefPerm;

	g_sConfig.m_sPermissions.m_iUid = getuid();
	g_sConfig.m_sPermissions.m_iGid = getgid();

	return init_fuse(argc, argv);
}