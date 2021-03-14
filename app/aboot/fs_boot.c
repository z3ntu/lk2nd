// SPDX-License-Identifier: GPL-2.0-only

#include <debug.h>
#include <target.h>
#include <string.h>

#include <lib/bio.h>
#include <lib/fs.h>

#include "fs_boot.h"

static int fsboot_fs_list_root(char *dev_name) {
	struct dirhandle *dirh;
	struct dirent dirent;

	int ret = 0, error = -2;

	if (fs_mount("/mnt", "ext2", dev_name) >= 0) {
		error = 0;
		ret = fs_open_dir("/mnt", &dirh);
		if (ret >= 0) {
			while (fs_read_dir(dirh, &dirent) >= 0) {
				dprintf(ALWAYS, "| /%s/%s\n", dev_name, dirent.name);
			}
			fs_close_dir(dirh);
		} else {
			dprintf(ALWAYS, "    fs_open_dir ret = %d\n", ret);
			error = -1;
		}
		fs_unmount("/mnt");
	}

	return error;
}

static void fsboot_bio_dump_partitions(int bdev_id) {
	int i = 0, j = 0, ret = 0;
	char dev_name[128];
	bdev_t *dev = NULL;


	if (bdev_id == 1) {
		/* HACK: There is no hd1p0 for some reason */
		i = 1;
	}

	sprintf(dev_name, "hd%d", bdev_id);

	dev = bio_open(dev_name);
	if (!dev) {
		dprintf(ALWAYS, "fs-boot: Can't open %s\n", dev_name);
		return;
	}
	bio_close(dev);

	dprintf(ALWAYS, "fs-boot: Looking at %s:\n", dev_name);

	sprintf(dev_name, "hd%dp%d", bdev_id, i);

	while (dev = bio_open(dev_name)) {
		dprintf(ALWAYS, "%.8s:  %.10s (%6llu MiB): \n",dev->name, dev->label, dev->size / (1024 * 1024));
		bio_close(dev);

		/* Try to mount it */
		if (fsboot_fs_list_root(dev_name) < 0) {
			j = 0;
			sprintf(dev_name, "hd%dp%dp%d", bdev_id, i, j);
			while (dev = bio_open(dev_name)) {
				dprintf(ALWAYS, "%.8s:  %.10s (%6llu MiB): \n",dev->name, dev->label, dev->size / (1024 * 1024));
				bio_close(dev);

				/* Try to mount it */
				fsboot_fs_list_root(dev_name);

				j++;
				sprintf(dev_name, "hd%dp%dp%d", bdev_id, i, j);
			}
		}

		i++;
		sprintf(dev_name, "hd%dp%d", bdev_id, i);
	}
}

void fsboot_test(void) {

	dprintf(ALWAYS, "====== fs-boot test ======\n");

	bio_dump_devices();

	fsboot_bio_dump_partitions(2); // sdcard
	fsboot_bio_dump_partitions(1); // emmc


	dprintf(ALWAYS, "====== ============ ======\n");
}


static int fsboot_fs_load_img(char *dev_name, void* target, size_t sz) {
	struct dirhandle *dirh;
	struct dirent dirent;

	char image_path[128] = "/mnt/";
	int ret = 0;

	if (fs_mount("/mnt", "ext2", dev_name) >= 0) {
		ret = fs_open_dir("/mnt", &dirh);
		if (ret >= 0) {
			while (fs_read_dir(dirh, &dirent) >= 0) {
				dprintf(ALWAYS, "xx: /%s/%s\n", dev_name, dirent.name);

				if (strncmp(dirent.name, "boot.img", 7) == 0) {
					strcpy(image_path, "/mnt/");
					strcat(image_path, dirent.name);
					dprintf(ALWAYS, "Found boot image: %s : %s\n", dev_name, image_path);

					fs_close_dir(dirh);

					dprintf(ALWAYS, "fs_load_file(image_path, target=%X, sz=%u)\n", target, sz);
					return fs_load_file(image_path, target, sz);
				}
			}
			fs_close_dir(dirh);
		} else {
			return ret;
		}
		fs_unmount("/mnt");
	}

	return -1;
}

static int fsboot_find_and_boot(int bdev_id, void* target, size_t sz) {
	int i = 0, j = 0, ret = 0;
	char dev_name[128];
	bdev_t *dev = NULL;

	if (bdev_id == 1) {
		/* HACK: There is no hd1p0 for some reason */
		i = 1;
	}

	sprintf(dev_name, "hd%d", bdev_id);

	dev = bio_open(dev_name);
	if (!dev) {
		dprintf(ALWAYS, "fs-boot: Can't open %s\n", dev_name);
		return -1;
	}
	bio_close(dev);

	sprintf(dev_name, "hd%dp%d", bdev_id, i);

	while (dev = bio_open(dev_name)) {
		bio_close(dev);

		ret = fsboot_fs_load_img(dev_name, target, sz);
		if (ret >= 0)
			return ret;

		if (ret < 0) {
			j = 0;
			sprintf(dev_name, "hd%dp%dp%d", bdev_id, i, j);
			while (dev = bio_open(dev_name)) {
				bio_close(dev);

				ret = fsboot_fs_load_img(dev_name, target, sz);
				if (ret >= 0)
					return ret;

				j++;
				sprintf(dev_name, "hd%dp%dp%d", bdev_id, i, j);
			}
		}
		i++;
		sprintf(dev_name, "hd%dp%d", bdev_id, i);
	}

	return -1;
}

int fsboot_boot_first(void* target, size_t sz) {
	int ret = -1;

	ret = fsboot_find_and_boot(2, target, sz); // sdcard
	if (ret > 0)
		return ret;

	ret = fsboot_find_and_boot(1, target, sz); // emmc
	if (ret > 0)
		return ret;

	return -1;
}
