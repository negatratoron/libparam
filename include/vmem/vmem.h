/*
 * vmem.h
 *
 *  Created on: Sep 22, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_VMEM_H_
#define SRC_PARAM_VMEM_H_

#define VMEM_MAX(a,b) ((a) > (b) ? a : b)
#define VMEM_MIN(a,b) ((a) < (b) ? a : b)

#include <stddef.h>
#include <param/param.h>

enum vmem_types{
	VMEM_TYPE_RAM = 1,
	VMEM_TYPE_FRAM = 2,
	VMEM_TYPE_FRAM_SECURE = 3,
	VMEM_TYPE_FLASH = 4,
	VMEM_TYPE_DRIVER = 5,
	VMEM_TYPE_FLASH_QSPI = 6,
	VMEM_TYPE_FILE = 7,
	VMEM_TYPE_FRAM_CACHE = 8,
};

typedef struct vmem_s {
	int type;
	void (*read)(struct vmem_s * vmem, uint32_t addr, void * dataout, uint32_t len);
	void (*write)(struct vmem_s * vmem, uint32_t addr, void * datain, uint32_t len);
	int (*backup)(struct vmem_s * vmem);
	int (*restore)(struct vmem_s * vmem);
	void * vaddr;
	uint32_t size;
	const char *name;
	int big_endian;
	void * driver;
} vmem_t;

void * vmem_memcpy(void * to, void * from, uint32_t size);
vmem_t * vmem_index_to_ptr(int idx);
int vmem_ptr_to_index(vmem_t * vmem);

extern int __start_vmem, __stop_vmem;

#endif /* SRC_PARAM_VMEM_H_ */
