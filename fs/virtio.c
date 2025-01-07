#include "virtio.h"
#include <inc/x86.h>
#include <inc/lib.h>

static struct virtio_disk d;

static int virtio_map(struct virtio_disk *disk) {
    disk->mmio_base_addr = (volatile uint8_t *)VIRTIO_VADDR;

    uint8_t barno;
    uint32_t offset;

    get_virtio_bar_and_offset(disk->pcidev, &barno, &offset);

    size_t size = get_bar_size(disk->pcidev, barno);
    uintptr_t adr = get_bar_address(disk->pcidev, barno);
    int res = sys_map_physical_region(adr, CURENVID, (void*)disk->mmio_base_addr, ROUNDUP(size, PAGE_SIZE), PROT_RW | PROT_CD);
    if (res) return VIRTIO_MAP_ERR;

    disk->mmio_base_addr += offset;

    cprintf("ADR: %lx\n", get_phys_addr((void*)disk->mmio_base_addr));

    return VIRTIO_OK;
}

int virtio_disk_init(void) {
    struct virtio_disk* disk = &d;
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

    err = virtio_map(disk); 
    if (err)
        panic("VIRTIO registers mapping failed\n");

    volatile struct virtio_pci_common_cfg* cfg = (volatile struct virtio_pci_common_cfg*)disk->mmio_base_addr;

    // checking driver_status
    uint32_t status = 0;
    cfg->device_status = status; // reset
    status |= VIRTIO_STATUS_ACK;
    cfg->device_status = status;
    status |= VIRTIO_STATUS_DRIVER;
    cfg->device_status = status;

    // negotiate features
    uint64_t features = 0;
    features &= ~(1 << VIRTIO_BLK_F_RO);
    features &= ~(1 << VIRTIO_BLK_F_SCSI);
    features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
    features &= ~(1 << VIRTIO_BLK_F_MQ);
    features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
    features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
    features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
    cfg->driver_feature = features;

    status |= VIRTIO_STATUS_FEAT_OK;
    cfg->device_status = status;

    if ((cfg->device_status & VIRTIO_STATUS_FEAT_OK) != 0)
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

    status |= VIRTIO_STATUS_DRIVER_OK;
    cfg->device_status = status;

    return VIRTIO_OK;
}
