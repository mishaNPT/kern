#include "virtio.h"
#include <inc/x86.h>
#include <inc/lib.h>

static struct virtio_disk d;

static int virtio_map(struct virtio_disk *disk) {
    disk->mmio_base_addr = (volatile uint8_t *)VIRTIO_VADDR;

    size_t size = get_bar_size(disk->pcidev, 0);
    size = size > (SECTOR_SIZE * VIRTQ_ENTRY_NUM) ? (SECTOR_SIZE * VIRTQ_ENTRY_NUM) : size;
    cprintf("size = %lx\n", size);
    int res = sys_map_physical_region(get_bar_address(disk->pcidev, 0), CURENVID, (void*)disk->mmio_base_addr, size, PROT_RW | PROT_CD);
    if (res) return VIRTIO_MAP_ERR;

    return VIRTIO_OK;
}

int virtio_disk_init(void) {
    struct virtio_disk* disk = &d;

    uint32_t status = 0;

    struct PciDevice *pcidevice = find_pci_dev(1, 0);
    if (pcidevice == NULL)
        panic("VIRTIO device not found\n");

    disk->pcidev = pcidevice;

    if (virtio_map(disk))
        panic("VIRTIO registers mapping failed\n");

    if (*VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_MAGIC) != 0x74726976)
        panic("virtio: invalid magic value");
    if (*VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_VERSION) != 1)
        panic("virtio: invalid version");
    if (*VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_DEVICE_ID) != VIRTIO_DEVICE_BLK)
        panic("virtio: invalid device id");
  
    // reset device
    *VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_DEVICE_STATUS) = status;

    // set ACKNOWLEDGE status bit
    status |= VIRTIO_STATUS_ACK;
    *VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_DEVICE_STATUS) = status;

    // set DRIVER status bit
    status |= VIRTIO_STATUS_DRIVER;
    *VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_DEVICE_STATUS) = status;

    // negotiate features
    uint64_t features = *VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_DEVICE_FEATURES);
    features &= ~(1 << VIRTIO_BLK_F_RO);
    features &= ~(1 << VIRTIO_BLK_F_SCSI);
    features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
    features &= ~(1 << VIRTIO_BLK_F_MQ);
    features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
    features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
    features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
    *VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_DRIVER_FEATURES) = features;

    // tell device that feature negotiation is complete.
    status |= VIRTIO_STATUS_FEAT_OK;
    *VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_DEVICE_STATUS) = status;

    // re-read status to ensure FEATURES_OK is set.
    status = *VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_DEVICE_STATUS);
    if(!(status & VIRTIO_STATUS_FEAT_OK))
        panic("virtio disk FEATURES_OK unset");

    // initialize queue 0.
    *VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_QUEUE_SEL) = 0;

    // ensure queue 0 is not in use.
    if(*VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_QUEUE_READY))
        panic("virtio disk should not be ready");

    // check maximum queue size.
    uint32_t max = *VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_QUEUE_NUM_MAX);
    if(max == 0)
        panic("virtio disk has no queue 0");
    if(max < VIRTQ_ENTRY_NUM)
        panic("virtio disk max queue too short");

    // allocate and zero queue memory.
    /*disk->descs = kalloc();
    disk->avail = kalloc();
    disk->used = kalloc();
    if(!disk.desc || !disk.avail || !disk.used)
        panic("virtio disk kalloc");
    memset(disk->descs, 0, PAGE_SIZE);
    memset(disk->avail, 0, PAGE_SIZE);
    memset(disk->used, 0, PAGE_SIZE);

    // set queue size.
    *VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_QUEUE_NUM) = VIRTQ_ENTRY_NUM;

    // write physical addresses.
    *VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_QUEUE_DESC_LOW) = (uint64_t)disk->descs;
    *VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_QUEUE_DESC_HIGH) = (uint64_t)disk->descs >> 32;
    *VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_DRIVER_DESC_LOW) = (uint64_t)disk->avail;
    *VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_DRIVER_DESC_HIGH) = (uint64_t)disk->avail >> 32;
    *VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_DEVICE_DESC_LOW) = (uint64_t)disk->used;
    *VIRTIO_REG32(disk->mmio_base_addr, VIRTIO_REG_DEVICE_DESC_HIGH) = (uint64_t)disk->used >> 32;

  // queue is ready.
  *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

  // all NUM descriptors start out unused.
  for(int i = 0; i < NUM; i++)
    disk.free[i] = 1;

  // tell device we're completely ready.
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;*/

    return VIRTIO_OK;
}
