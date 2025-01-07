#include "virtio.h"
#include <inc/x86.h>
#include <inc/lib.h>

static struct virtio_disk d;

static int virtio_map(struct virtio_disk *disk) {
    disk->mmio_base_addr = (volatile uint8_t *)VIRTIO_VADDR;

    uint8_t barno;
    uint32_t offset;

    get_virtio_bar_and_offset(disk->pcidev, &barno, &offset);

    //cprintf("BAR: %u\n", barno);
    //cprintf("OFFSET: %u\n", offset);

    size_t size = get_bar_size(disk->pcidev, barno);
    uintptr_t adr = get_bar_address(disk->pcidev, barno);
    //cprintf("SIZE: %lu\n", size);
    //cprintf("ADDR: %zx\n", adr);
    int res = sys_map_physical_region(adr, CURENVID, (void*)disk->mmio_base_addr, ROUNDUP(size, PAGE_SIZE), PROT_RW | PROT_CD);
    if (res) return VIRTIO_MAP_ERR;

    disk->mmio_base_addr += offset;

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
    if ((cfg->device_status & VIRTIO_STATUS_DRIVER_OK) == 0)
        cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;

    

    return VIRTIO_OK;
}
