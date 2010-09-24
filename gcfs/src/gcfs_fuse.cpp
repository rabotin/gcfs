/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

*/

#define FUSE_USE_VERSION 26

#include <fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <gcfs.h>
#include <gcfs_fuse.h>
#include <gcfs_task_config.h>

static int gcfs_stat(fuse_ino_t ino, struct stat *stbuf)
{
	stbuf->st_ino = ino;
	int iIndex = (ino - 2) % GCFS_FUSE_INODES_PER_TASK;

	if(ino > GCFS_FUSE_INODES_PER_TASK * g_sTasks.GetCount())
		return -1;
	
	if(ino == FUSE_ROOT_ID) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}
	else if(iIndex < GCFS_DIR_LAST) // Task dir
	{
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink =2;
	}
	else if(iIndex >= GCFS_DIR_LAST)
	{
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
	}
	else
	{
		printf("Error stat inode: %d\n", (int)ino);
		return -1;
	}

	return 0;
}

static void gcfs_getattr(fuse_req_t req, fuse_ino_t ino,
			     struct fuse_file_info *fi)
{
	struct stat stbuf = {0};

	(void) fi;

	if (gcfs_stat(ino, &stbuf) == -1)
		fuse_reply_err(req, ENOENT);
	else
		fuse_reply_attr(req, &stbuf, 1.0);
}

static void gcfs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	struct fuse_entry_param e = {0};
	int iParentIndex = (parent - 2) % GCFS_FUSE_INODES_PER_TASK;
	int iTaskIndex = (parent - 2) / GCFS_FUSE_INODES_PER_TASK;

	if ((parent == FUSE_ROOT_ID) && (g_sTasks.Get(name) != NULL))
	{
		e.ino = GCFS_DIRINODE(g_sTasks.GetIndex(name), GCFS_DIR_TASK);
	}
	else if(iParentIndex == GCFS_DIR_TASK && strcmp(name, "config")==0)
	{
		e.ino = GCFS_DIRINODE(iTaskIndex, GCFS_DIR_CONFIG);
	}
	else if(iParentIndex == GCFS_DIR_TASK && strcmp(name, "data")==0)
	{
		e.ino = GCFS_DIRINODE(iTaskIndex, GCFS_DIR_DATA);
	}
	else if(iParentIndex == GCFS_DIR_TASK && g_sTasks.Get(iTaskIndex)->m_bCompleted && strcmp(name, "result")==0)
	{
		e.ino = GCFS_DIRINODE(iTaskIndex, GCFS_DIR_RESULT);
	}
	else if(iParentIndex == GCFS_DIR_TASK && strcmp(name, "control")==0)
	{
		e.ino = GCFS_CONTROLINODE(iTaskIndex, 0);
	}
	else if(iParentIndex == GCFS_DIR_CONFIG && g_sTasks.Get(iTaskIndex)->m_mNameToIndex.find(name) != g_sTasks.Get(iTaskIndex)->m_mNameToIndex.end()){
		e.ino = GCFS_CONFIGINODE(iTaskIndex, g_sTasks.Get(iTaskIndex)->m_mNameToIndex[name]);
	}
	else
	{
		printf("Error lookup: %s, parent: %d\n", name, (int)parent);
		fuse_reply_err(req, ENOENT);
		return;
	}

	e.attr_timeout = 1.0;
	e.entry_timeout = 1.0;
	gcfs_stat(e.ino, &e.attr);
	fuse_reply_entry(req, &e);
}

static void dirbuf_add(fuse_req_t req, std::string &buff , const char *name,
		       fuse_ino_t ino)
{
	struct stat stbuf = {0};

	stbuf.st_ino = ino;

	size_t bufSize = buff.size();

	size_t size = fuse_add_direntry(req, NULL, 0, name, NULL, 0);
	buff.resize(bufSize+size);
	fuse_add_direntry(req, (char*)buff.c_str() + bufSize, size, name, &stbuf, bufSize+size);

}

#define min(x, y) ((x) < (y) ? (x) : (y))

static int reply_buf_limited(fuse_req_t req, std::string &buff, off_t off, size_t maxsize)
{
	if (off < buff.size())
		return fuse_reply_buf(req, buff.c_str() + off,
				      min(buff.size() - off, maxsize));
	else
		return fuse_reply_buf(req, NULL, 0);
}

static void gcfs_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
			     off_t off, struct fuse_file_info *fi)
{
	(void) fi;
	int iParentIndex = (ino - 2) % GCFS_FUSE_INODES_PER_TASK;
	int iTaskIndex = (ino - 2) / GCFS_FUSE_INODES_PER_TASK;

	std::string buff;
	dirbuf_add(req, buff, ".", ino);
	
	if (ino == FUSE_ROOT_ID)
	{
		for(int iIndex = 0; iIndex < g_sTasks.GetCount(); iIndex++)
			dirbuf_add(req, buff, g_sTasks.Get(iIndex)->m_sName.c_str(), GCFS_DIRINODE(iIndex, GCFS_DIR_TASK));
	}
	else switch(iParentIndex)
	{
		case GCFS_DIR_TASK:
		{
			dirbuf_add(req, buff, "..", FUSE_ROOT_ID);
			dirbuf_add(req, buff, "config", GCFS_DIRINODE(iTaskIndex, GCFS_DIR_CONFIG));
			dirbuf_add(req, buff, "data", GCFS_DIRINODE(iTaskIndex, GCFS_DIR_CONFIG));
			if(g_sTasks.Get(iTaskIndex)->m_bCompleted)
				dirbuf_add(req, buff, "result", GCFS_DIRINODE(iTaskIndex, GCFS_DIR_CONFIG));
			dirbuf_add(req, buff, "control", GCFS_CONFIGINODE(iTaskIndex, 0));
			break;
		}
		
		case GCFS_DIR_CONFIG:
		{
			dirbuf_add(req, buff, "..", GCFS_DIRINODE(iTaskIndex, GCFS_DIR_TASK));
			for(int iIndex = 0; iIndex < g_sTasks.Get(iTaskIndex)->m_vIndexToName.size(); iIndex++)
				dirbuf_add(req, buff, g_sTasks.Get(iTaskIndex)->m_vIndexToName[iIndex]->m_sName, GCFS_CONFIGINODE(iTaskIndex, iIndex));
			break;
		}

		case GCFS_DIR_DATA:
		{
			dirbuf_add(req, buff, "..", GCFS_DIRINODE(iTaskIndex, GCFS_DIR_TASK));
			/*for(int iIndex = 0; iIndex < g_sTasks.Get(iTaskIndex)->m_vIndexToName.size(); iIndex++)
				dirbuf_add(req, &b, g_sTasks.Get(iTaskIndex)->m_vIndexToName[iIndex]->m_sName, GCFS_CONFIGINODE(iTaskIndex, iIndex));*/
			break;
		}

		case GCFS_DIR_RESULT:
		{
			dirbuf_add(req, buff, "..", GCFS_DIRINODE(iTaskIndex, GCFS_DIR_TASK));
			/*for(int iIndex = 0; iIndex < g_sTasks.Get(iTaskIndex)->m_vIndexToName.size(); iIndex++)
				dirbuf_add(req, &b, g_sTasks.Get(iTaskIndex)->m_vIndexToName[iIndex]->m_sName, GCFS_CONFIGINODE(iTaskIndex, iIndex));*/
			break;
		}
		
		default:
			fuse_reply_err(req, ENOTDIR);
			return;
	}
	
	reply_buf_limited(req, buff, off, size);
}

static void gcfs_open(fuse_req_t req, fuse_ino_t ino,
			  struct fuse_file_info *fi)
{
	int iIndex = (ino - 2) % GCFS_FUSE_INODES_PER_TASK;
	int iTaskIndex = (ino - 2) / GCFS_FUSE_INODES_PER_TASK;
	
	if (iIndex < GCFS_DIR_LAST)
		fuse_reply_err(req, EISDIR);
	else if ((fi->flags & 3) != O_RDONLY)
		fuse_reply_err(req, EACCES);
	else if(iIndex < GCFS_CONFIGINODE(0,0))
	{
		fuse_reply_open(req, fi);
	}
}

static void gcfs_read(fuse_req_t req, fuse_ino_t ino, size_t size,
			  off_t off, struct fuse_file_info *fi)
{
	(void) fi;

	assert(ino == 2);
	//reply_buf_limited(req, gcfs_str, strlen(gcfs_str), off, size);
}

static void gcfs_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name,
		       mode_t mode)
{
	int iParentIndex = (parent - 2) % GCFS_FUSE_INODES_PER_TASK;
	int iTaskIndex = (parent - 2) / GCFS_FUSE_INODES_PER_TASK;

	if(parent == FUSE_ROOT_ID)
	{
		struct fuse_entry_param e = {0};

		g_sTasks.AddTask(name);
		e.ino = GCFS_DIRINODE(g_sTasks.GetCount()-1, GCFS_DIR_TASK);
		e.attr_timeout = 1.0;
		e.entry_timeout = 1.0;
		gcfs_stat(e.ino, &e.attr);
	
		fuse_reply_entry(req, &e);
		return;
	}

	fuse_reply_err(req, -EACCES);
}

static struct fuse_lowlevel_ops gcfs_oper = {};

int init_fuse(int argc, char *argv[])
{
	gcfs_oper.lookup	= gcfs_lookup;
	gcfs_oper.getattr	= gcfs_getattr;
	gcfs_oper.readdir	= gcfs_readdir;
	gcfs_oper.open		= gcfs_open;
	gcfs_oper.read		= gcfs_read;
	gcfs_oper.mkdir	= gcfs_mkdir;

	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_chan *ch;
	char *mountpoint;
	int err = -1;

	if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1 &&
	    (ch = fuse_mount(mountpoint, &args)) != NULL) {
		struct fuse_session *se;

		se = fuse_lowlevel_new(&args, &gcfs_oper,
				       sizeof(gcfs_oper), NULL);
		if (se != NULL) {
			if (fuse_set_signal_handlers(se) != -1) {
				fuse_session_add_chan(se, ch);
				err = fuse_session_loop(se);
				fuse_remove_signal_handlers(se);
				fuse_session_remove_chan(ch);
			}
			fuse_session_destroy(se);
		}
		fuse_unmount(mountpoint, ch);
	}
	fuse_opt_free_args(&args);

	return err ? 1 : 0;
}
