#define FUSE_USE_VERSION 26

#include <minerva-safefs-layer/minerva_layer.hpp>

#include <fuse.h>

static struct fuse_operations minerva_operations;

int main(int argc, char* argv[])
{
    set_file_format(MSGPACK);
    minerva_operations.init = minerva_init;
    minerva_operations.opendir = minerva_opendir;
    minerva_operations.readdir = minerva_readdir;
    minerva_operations.getattr = minerva_getattr;
    minerva_operations.fgetattr = minerva_fgetattr;
    minerva_operations.access = minerva_access;
    minerva_operations.releasedir = minerva_releasedir;
    minerva_operations.write = minerva_write;
    minerva_operations.create = minerva_create;
    minerva_operations.open = minerva_open;
    minerva_operations.read = minerva_read;
    minerva_operations.release = minerva_release;
    minerva_operations.mknod  = minerva_mknod;
    minerva_operations.mkdir = minerva_mkdir;
    minerva_operations.rmdir = minerva_rmdir;
    minerva_operations.truncate = minerva_truncate;
    minerva_operations.chmod = minerva_chmod;
    minerva_operations.flush = minerva_flush;
    minerva_operations.rename = minerva_rename;
    minerva_operations.unlink = minerva_unlink;
    minerva_operations.utimens = minerva_utimens;

    return fuse_main(argc, argv, &minerva_operations, NULL);
}
