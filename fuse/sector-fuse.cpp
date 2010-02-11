#include "sectorfs.h"

struct fuse_operations sector_ops;

/*
{
   .getattr = SectorFS::getattr,
   .readlink = SectorFS::readlink,
   .mknod = SectorFS::mknod,
   .mkdir = SectorFS::mkdir,
   .unlink = SectorFS::unlink,
   .rmdir = SectorFS::rmdir,
   .symlink = SectorFS::symlink,
   .rename = SectorFS.rename,
   .link = SectorFS::link,
   .chmod = SectorFS::chmod,
   .chown = SectorFS::chown,
   .truncate = SectorFS::truncate,
   .utime = SectorFS::utime,
   .open = SectorFS::open,
   .read = SectorFS::read,
   .write = SectorFS::write,
   .statfs = SectorFS::statfs,
   .flush = SectorFS::flush,
   .release = SectorFS::release,
   .fsync = SectorFS::fsync,
   .setxattr = SectorFS::setxattr,
   .getxattr = SectorFS::getxattr,
   .listxattr = SectorFS::.listxattr,
   .removexattr = SectorFS::removexattr,
   .opendir = SectorFS::opendir,
   .readdir = SectorFS::readdir,
   .releasedir = SectorFS::releasedir,
   .fsyncdir = SectorFS::fsyncdir,
   .init = SectorFS::init,
   .destroy = SectorFS::destroy,
   .access = SectorFS::access,
   .create = SectorFS::create,
   .ftruncate = SectorFS::ftruncate,
   .fgetattr = SectorFS::fgetattr,
   .lock = SectorFS::lock,
   .bmap = SectorFS::bmap
};
*/

int main(int argc, char *argv[])
{
   sector_ops.init = SectorFS::init;
   sector_ops.destroy = SectorFS::destroy;

   sector_ops.getattr = SectorFS::getattr;
   sector_ops.fgetattr = SectorFS::fgetattr;
   sector_ops.mknod = SectorFS::mknod;
   sector_ops.mkdir = SectorFS::mkdir;
   sector_ops.unlink = SectorFS::unlink;
   sector_ops.rmdir = SectorFS::rmdir;
   sector_ops.rename = SectorFS::rename;
   sector_ops.statfs = SectorFS::statfs;
   sector_ops.readdir = SectorFS::readdir;

   sector_ops.create = SectorFS::create;
   sector_ops.truncate = SectorFS::truncate;
   sector_ops.ftruncate = SectorFS::ftruncate;
   sector_ops.open = SectorFS::open;
   sector_ops.read = SectorFS::read;
   sector_ops.write = SectorFS::write;
   sector_ops.flush = SectorFS::flush;
   sector_ops.release = SectorFS::release;
   sector_ops.utime = SectorFS::utime;
   sector_ops.utimens = SectorFS::utimens;

   sector_ops.chmod = SectorFS::chmod;
   sector_ops.chown = SectorFS::chown;

   sector_ops.access = SectorFS::access;


   SectorFS::g_SectorConfig.loadInfo("../conf/client.conf");


   return fuse_main(argc, argv, &sector_ops, NULL);
}
