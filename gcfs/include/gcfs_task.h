#ifndef GCFS_TASK_H
#define GCFS_TASK_H

#include "gcfs.h"
#include "gcfs_config_values.h"

#include <string>
#include <vector>
#include <map>

// Task Configuration
class GCFS_Task {

public:
													GCFS_Task(const char * sName);

public:
	std::string									m_sName;

	bool											m_bCompleted;

	
// Task Configuration Values
public:
	GCFS_ConfigInt 							m_iMemory;
	GCFS_ConfigInt 							m_iProcesses;
	GCFS_ConfigInt 							m_iTimeout;
	GCFS_ConfigChoice							m_iService;

// Mapping of confg values for dynamic access
	std::vector<GCFS_ConfigValue*>		m_vConfigValues;
	std::map<std::string, int> 			m_mConfigNameToIndex;

// File management

	typedef struct File_t{
		unsigned int		m_iInode;
		std::string			m_sPath;
		int					m_hFile;
		GCFS_Task*			m_pTask;
	} File;

	typedef std::map<std::string, File*> Files;
	
	File*												createDataFile(const char * name);
	File*												deleteDataFile(const char * name);
	Files::const_iterator 						getDataFiles();
	File*												createResultFile(const char * name);
	File*												deleteResultFile(const char * name);
	Files::const_iterator 						getResultFiles();
	
// Data and result files mapping from name to inode number
public:
	Files											m_mDataFiles;
	std::map<int, File*>						m_mInodeToDataFiles;
	Files											m_mResultFiles;
	std::map<int, File*>						m_mInodeToResultFiles;
	
};

class GCFS_TaskManager {
public:
											GCFS_TaskManager():m_uiFirstFileInode(-1){};
public:
// Manage tasks
	bool									addTask(const char * sName);
	bool									deleteTask(const char * sName);

	size_t								getTaskCount();

	GCFS_Task*							getTask(int iIndex);
	GCFS_Task*							getTask(const char * sName);
	int									getTaskIndex(const char * sName);
	
// Inode allocation and management
public:
	unsigned int						getInode(GCFS_Task::File *pFile);
	bool									putInode(int iInode);

	GCFS_Task::File*					getInodeFile(int iInode);
	
	unsigned int						m_uiFirstFileInode;

private:
	std::map<int, GCFS_Task::File*>	m_mInodesOwner;

private:

	std::vector<GCFS_Task*> 		m_vTasks;
	std::map<std::string, int> 	m_mTaskNames;
};

#endif // GCFS_TASK_H