#pragma once

// Filesystem includes
#include <fuse.h>

int minerva_init(struct fuse_operations** fuse_operations);

void store(const char* path, const char* data, size_t data_size);


