#ifndef __SECTOR_FS_FUSE_H__
#define __SECTOR_FS_FUSE_H__

#define FUSE_USE_VERSION 26

#include <fsclient.h>
#include <util.h>

#include <fuse.h>
#include <fuse/fuse_opt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string>
#include <map>

struct FileTracker
{
   std::string m_strName;
   int m_iCount;

   SectorFile* m_pHandle;
};

class SectorFS
{
public:
   static void* init(struct fuse_conn_info *conn);
   static void destroy(void *);

   static int getattr(const char *, struct stat *);
   static int fgetattr(const char *, struct stat *, struct fuse_file_info *);
   static int mknod(const char *, mode_t, dev_t);
   static int mkdir(const char *, mode_t);
   static int unlink(const char *);
   static int rmdir(const char *);
   static int rename(const char *, const char *);
   static int statfs(const char *, struct statvfs *);
   static int utime(const char *, struct utimbuf *);
   static int utimens(const char *, const struct timespec tv[2]);
   static int opendir(const char *, struct fuse_file_info *);
   static int readdir(const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
   static int releasedir(const char *, struct fuse_file_info *);
   static int fsyncdir(const char *, int, struct fuse_file_info *);

   //static int readlink(const char *, char *, size_t);
   //static int symlink(const char *, const char *);
   //static int link(const char *, const char *);

   static int chmod(const char *, mode_t);
   static int chown(const char *, uid_t, gid_t);

   static int create(const char *, mode_t, struct fuse_file_info *);
   static int truncate(const char *, off_t);
   static int ftruncate(const char *, off_t, struct fuse_file_info *);
   static int open(const char *, struct fuse_file_info *);
   static int read(const char *, char *, size_t, off_t, struct fuse_file_info *);
   static int write(const char *, const char *, size_t, off_t, struct fuse_file_info *);
   static int flush(const char *, struct fuse_file_info *);
   static int fsync(const char *, int, struct fuse_file_info *);
   static int release(const char *, struct fuse_file_info *);

   //static int setxattr(const char *, const char *, const char *, size_t, int);
   //static int getxattr(const char *, const char *, char *, size_t);
   //static int listxattr(const char *, char *, size_t);
   //static int removexattr(const char *, const char *);

   static int access(const char *, int);

   static int lock(const char *, struct fuse_file_info *, int cmd, struct flock *);

public:
   static Session g_SectorConfig;

private:
   static std::map<std::string, FileTracker*> m_mOpenFileList;
   static pthread_mutex_t m_OpenFileLock;

private:
   static int translateErr(int sferr);

private:
   static bool g_bRunning;
   static void* HeartBeat(void*);
};

#endif
