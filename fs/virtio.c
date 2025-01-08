#include "virtio.h"
#include <inc/x86.h>
#include <inc/lib.h>

static struct virtio_disk d;

static int virtio_map(struct virtio_disk *disk, uint32_t* mult) {
    disk->mmio_base_addr = (volatile uint8_t *)VIRTIO_VADDR;

    uint8_t barno;
    uint32_t offset;

    get_virtio_bar_and_offset(disk->pcidev, &barno, &offset);

    size_t size = get_bar_size(disk->pcidev, barno);
    uintptr_t adr1 = get_bar_address(disk->pcidev, barno);
    cprintf("BAR: %x\n", barno);
    cprintf("OFFSET: %x\n", offset);
    cprintf("SIZE: %lx\n", size);
    cprintf("ADR: %lx\n", adr1);
    int res = sys_map_physical_region(adr1, CURENVID, (void*)disk->mmio_base_addr, ROUNDUP(size, PAGE_SIZE), PROT_RW | PROT_CD);
    if (res) return VIRTIO_MAP_ERR;

    disk->mmio_base_addr += offset;

    // for read-write
    disk->mmio_notify_addr = (volatile uint8_t *)VIRTIO_QUEUE;
    
    uint8_t old_barno = barno;
    get_virtio_bar_and_offset_and_mult(disk->pcidev, &barno, &offset, mult);

    size = get_bar_size(disk->pcidev, barno);
    if (barno != old_barno) {
        uintptr_t adr2 = get_bar_address(disk->pcidev, barno);
        cprintf("BAR: %x\n", barno);
        cprintf("OFFSET: %x\n", offset);
        cprintf("SIZE: %lx\n", size);
        cprintf("ADR: %lx\n", adr2);
        res = sys_map_physical_region(adr2, CURENVID, (void*)disk->mmio_notify_addr, ROUNDUP(size, PAGE_SIZE), PROT_RW | PROT_CD);
        if (res) return VIRTIO_MAP_ERR;
    } else {
        disk->mmio_notify_addr = (volatile uint8_t *)VIRTIO_VADDR;
    }

    disk->mmio_notify_addr += offset;

    return VIRTIO_OK;
}

void debug_pci_cfg(volatile struct virtio_pci_common_cfg* cfg) {
    cprintf("#################################################\n");
    cprintf("cfg->device_feature_select: %x \n", cfg->device_feature_select);
    cprintf("cfg->device_feature: %x \n", cfg->device_feature); 
    cprintf("cfg->driver_feature_select: %x \n", cfg->driver_feature_select); 
    cprintf("cfg->driver_feature: %x \n", cfg->driver_feature); 
    cprintf("cfg->config_msix_vector: %x \n", cfg->config_msix_vector); 
    cprintf("cfg->num_queues: %x \n", cfg->num_queues); 
    cprintf("cfg->queue_select: %x \n", cfg->queue_select); 
    cprintf("cfg->queue_size: %x \n", cfg->queue_size); 
    cprintf("cfg->queue_msix_vector: %x \n", cfg->queue_msix_vector); 
    cprintf("cfg->queue_enable: %x \n", cfg->queue_enable); 
    cprintf("cfg->queue_notify_off: %x \n", cfg->queue_notify_off); 
    cprintf("cfg->queue_desc: %lx \n", cfg->queue_desc); 
    cprintf("cfg->queue_driver: %lx \n", cfg->queue_driver); 
    cprintf("cfg->queue_device: %lx \n", cfg->queue_device); 
    cprintf("cfg->queue_notif_config_data: %x \n", cfg->queue_notif_config_data); 
    cprintf("cfg->queue_reset: %x \n", cfg->queue_reset); 
    cprintf("cfg->admin_queue_index: %x \n", cfg->admin_queue_index); 
    cprintf("cfg->admin_queue_num: %x \n", cfg->admin_queue_num); 
    cprintf("#################################################\n");
}

int virtio_disk_init(void) {
    struct virtio_disk* disk = &d;
    uint32_t mult;
    int err;

    struct PciDevice *pcidevice = find_pci_dev(1, 0);
    if (pcidevice == NULL)
        panic("VIRTIO device not found\n");

    disk->pcidev = pcidevice;

    if (disk->pcidev->vendor_id != 0x1af4)
        panic("virtio init: wrong pci vendor_id\n");

    if (disk->pcidev->device_id < 0x1000 && disk->pcidev->device_id > 0x107f)
        panic("virtio init: wrong pci device_id\n");

    if (disk->pcidev->revision_id < 1)
        cprintf("warning: virtio_init: pci revesion_id < 1\n");

    if (disk->pcidev->subdevice_id < 0x40)
        cprintf("warning: virtio_init: pci subdevice_id < 0x40\n");

    err = virtio_map(disk, &mult); 
    if (err)
        panic("VIRTIO registers mapping failed\n");

    volatile struct virtio_pci_common_cfg* cfg = (volatile struct virtio_pci_common_cfg*)disk->mmio_base_addr;

    //debug_pci_cfg(cfg);
    
    // checking driver_status
    uint32_t status = 0;
    cfg->device_status = status; // reset
    status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
    cfg->device_status = status;
    status |= VIRTIO_CONFIG_S_DRIVER;
    cfg->device_status = status;

    // negotiate features
    uint64_t features = 0;
    cfg->driver_feature = features;

    status |= VIRTIO_CONFIG_S_FEATURES_OK;
    cfg->device_status = status;

    if ((cfg->device_status & VIRTIO_CONFIG_S_FEATURES_OK) == 0)
        panic("virtio_init: device init failed\n");

    cfg->queue_select = 0;

    if (cfg->queue_enable != 0)
        panic("virtio_init: failed to enable queue\n");

    cprintf("num_queue: %d\n", cfg->num_queues);
    uint16_t max = cfg->queue_size;
    if(max == 0)
        panic("virtio disk has no queue 0");
    if(max < VIRTQ_ENTRY_NUM)
        panic("virtio disk max queue too short");

    // alloc 
    err = sys_alloc_region(0, disk->buffer, 3 * PAGE_SIZE, PROT_RW | PROT_CD);
    if (err)
        panic("Virtio err to alloc_region\n");

    for (int i = 0; i < 3; ++i) {
        volatile char* page = (volatile char *)disk->buffer + PAGE_SIZE * i;
        *page = 0;
        if (i == 1)
            disk->descs = (struct virtq_desc*) page;
        if (i == 2)
            disk->avail = (struct virtq_avail*) page;
        else
            disk->used = (struct virtq_used*) page;
    }

    // set queue size.
    cfg->queue_size = VIRTQ_ENTRY_NUM;

    // write physical addresses.
    cfg->queue_desc = get_phys_addr(disk->descs);
    cfg->queue_device = get_phys_addr(disk->used);
    cfg->queue_driver = get_phys_addr(disk->avail);
    
    cfg->queue_enable = 1;

    for (int i = 0; i < VIRTQ_ENTRY_NUM; ++i)
        disk->free[i] = 1;

    status |= VIRTIO_CONFIG_S_DRIVER_OK;
    cfg->device_status = status;

    disk->mmio_notify_addr += cfg->queue_notify_off * mult;

    return VIRTIO_OK;
}

static int alloc_desc(struct virtio_disk* disk) {
    for(int i = 0; i < VIRTQ_ENTRY_NUM; i++){
        if(disk->free[i]) {
            disk->free[i] = 0;
            return i;
        }
    }
    return -1;
}

static void free_desc(struct virtio_disk* disk, int i) {
    if (i >= VIRTQ_ENTRY_NUM)
        panic("free_desc 1");
    if (disk->free[i])
        panic("free_desc 2");
    disk->descs[i].addr = 0;
    disk->descs[i].len = 0;
    disk->descs[i].flags = 0;
    disk->descs[i].next = 0;
    disk->free[i] = 1;
}

static void free_chain(struct virtio_disk* disk, int i) {
    while(1) {
        int flag = disk->descs[i].flags;
        int nxt = disk->descs[i].next;
        free_desc(disk, i);
        if(flag & VIRTQ_DESC_F_NEXT)
            i = nxt;
        else
            break;
    }
}

static int alloc3_desc(struct virtio_disk* disk, int *idx) {
    for(int i = 0; i < 3; i++){
        idx[i] = alloc_desc(disk);
        if(idx[i] < 0){
            for(int j = 0; j < i; j++)
               free_desc(disk, idx[j]);
            return -1;
        }
    } 
    return 0;
}

int virtio_disk_rw(struct virtio_disk* disk, uint64_t secno, const void *src, size_t nsecs, bool write) {
    // allocate the three descriptors.
    int idx[3];
    if(alloc3_desc(disk, idx) != 0)
        panic("virtio_rw: failed to alloc desc\n");

    // format the three descriptors.
    // qemu's virtio-blk.c reads them.

    struct virtio_blk_req *buf0 = &disk->blk_req[idx[0]];

    if(write)
        buf0->type = VIRTIO_BLK_T_OUT; // write the disk
    else
        buf0->type = VIRTIO_BLK_T_IN; // read the disk
    buf0->reserved = 0;
    buf0->sector = secno;

    disk->descs[idx[0]].addr = (uint64_t) buf0;
    disk->descs[idx[0]].len = sizeof(struct virtio_blk_req);
    disk->descs[idx[0]].flags = VIRTQ_DESC_F_NEXT;
    disk->descs[idx[0]].next = idx[1];

    disk->descs[idx[1]].addr = (uint64_t)get_phys_addr((void *)src);
    disk->descs[idx[1]].len = nsecs * SECTOR_SIZE;
    if(write)
        disk->descs[idx[1]].flags = 0; // device reads b->data
    else
        disk->descs[idx[1]].flags = VIRTQ_DESC_F_WRITE; // device writes b->data
    disk->descs[idx[1]].flags |= VIRTQ_DESC_F_NEXT;
    disk->descs[idx[1]].next = idx[2];

    disk->info[idx[0]] = 0xff; // device writes 0 on success
    disk->descs[idx[2]].addr = (uint64_t) get_phys_addr((void *)&disk->info[idx[0]]);
    disk->descs[idx[2]].len = 1;
    disk->descs[idx[2]].flags = VIRTQ_DESC_F_WRITE; // device writes the status
    disk->descs[idx[2]].next = 0;

    // tell the device the first index in our chain of descriptors.
    __atomic_store_n(&disk->avail->ring[disk->avail->index % VIRTQ_ENTRY_NUM], idx[0], __ATOMIC_RELEASE);

    // tell the device another avail ring entry is available.
    __atomic_store_n(&disk->avail->index, disk->avail->index + 1, __ATOMIC_RELEASE);

    volatile uint16_t* adr = (volatile uint16_t*)disk->mmio_notify_addr;
    __atomic_store_n(&adr, 0, __ATOMIC_RELEASE);   

    int status;
    while ((status = __atomic_load_n(&disk->info[idx[0]], __ATOMIC_ACQUIRE)) == 0xFF)
        asm volatile ("pause");
    
    free_chain(disk, idx[0]);

    return status;
}

int virtio_write(uint64_t secno, const void *src, size_t nsecs) {
    if (!src)
        return -VIRTIO_BAD_ARG;

    return virtio_disk_rw(&d, secno, src, nsecs, 1);
}


int virtio_read(uint64_t secno, void *dst, size_t nsecs) {
    if (!dst)
        return -VIRTIO_BAD_ARG;

    if (!dst) return -VIRTIO_BAD_ARG;

    return virtio_disk_rw(&d, secno, dst, nsecs, 0);    
}