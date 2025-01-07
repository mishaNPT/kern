#ifndef VIRTIO_H
#define VIRTIO_H

#include "pci.h"

#define PACKED     __attribute__((packed))
#define ALIGNED(n) __attribute__((aligned(n)))

#define SECTOR_SIZE       512
#define VIRTQ_ENTRY_NUM   16
#define VIRTIO_DEVICE_BLK 2
#define VIRTIO_BLK_PADDR  0x10001000

// device feature bits
#define VIRTIO_BLK_F_RO              5	/* Disk is read-only */
#define VIRTIO_BLK_F_SCSI            7	/* Supports scsi command passthru */
#define VIRTIO_BLK_F_CONFIG_WCE     11	/* Writeback mode available in config */
#define VIRTIO_BLK_F_MQ             12	/* support more than one vq */
#define VIRTIO_F_ANY_LAYOUT         27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX     29

#define VIRTIO_STATUS_ACK       1
#define VIRTIO_STATUS_DRIVER    2
#define VIRTIO_STATUS_DRIVER_OK 4
#define VIRTIO_STATUS_FEAT_OK   8

#define VIRTQ_AVAIL_F_NO_INTERRUPT 1
#define VIRTQ_DESC_F_NEXT          1
#define VIRTQ_DESC_F_WRITE         2

#define VIRTIO_BLK_T_IN  0
#define VIRTIO_BLK_T_OUT 1

/* NVMe error codes */
enum vertio_error {
    VIRTIO_OK = 0,              /* OK */
    VIRTIO_BAD_ARG = 1,         /* Bad argument */
    VIRTIO_NO_DEVICE = 2,       /* No VIRTIO device */
    VIRTIO_MAP_ERR = 3,         /* Unable to map VIRTIO device */
    VIRTIO_UNSUPPORTED = 4,     /* VIRTIO device unsupported */
    //NVME_ACMD_FAILED = 5,     /* Admin command failed */
    VIRTIO_IOQ_INIT_FAILED = 6, /* I/O queue initialization failed */
    //NVME_IOCMD_FAILED = 7,    /* I/O command failed */
    VIRTIO_ALLOC_FAILED = 8,    /* Failed to allocate buffer */
    VIRTIO_CMD_TIMEOUT = 9      /* Waiting time for command completion has expired */
};

// Virtqueue Descriptor area entry.
struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} PACKED;

// Virtqueue Available Ring.
struct virtq_avail {
    uint16_t flags;
    uint16_t index;
    uint16_t ring[VIRTQ_ENTRY_NUM];
    uint16_t unused;
} PACKED;

// Virtqueue Used Ring entry.
struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
} PACKED;

// Virtqueue Used Ring.
struct virtq_used {
    uint16_t flags;
    uint16_t index;
    struct virtq_used_elem ring[VIRTQ_ENTRY_NUM];
} PACKED;

// Virtio-blk request.
struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
    uint8_t data[512];
    uint8_t status;
} PACKED;

struct virtio_pci_common_cfg {
    /* About the whole device. */
    uint32_t device_feature_select; /* read-write */
    uint32_t device_feature; /* read-only for driver */
    uint32_t driver_feature_select; /* read-write */
    uint32_t driver_feature; /* read-write */
    uint16_t config_msix_vector; /* read-write */
    uint16_t num_queues; /* read-only for driver */
    uint8_t device_status; /* read-write */
    uint8_t config_generation; /* read-only for driver */
    /* About a specific virtqueue. */
    uint16_t queue_select; /* read-write */
    uint16_t queue_size; /* read-write */
    uint16_t queue_msix_vector; /* read-write */
    uint16_t queue_enable; /* read-write */
    uint16_t queue_notify_off; /* read-only for driver */
    uint64_t queue_desc; /* read-write */
    uint64_t queue_driver; /* read-write */
    uint64_t queue_device; /* read-write */
    uint16_t queue_notif_config_data; /* read-only for driver */
    uint16_t queue_reset; /* read-write */
    /* About the administration virtqueue. */
    uint16_t admin_queue_index; /* read-only for driver */
    uint16_t admin_queue_num; /* read-only for driver */
};


struct virtio_disk {
    struct PciDevice* pcidev; /* Associated PCI device */

    uint8_t* buffer;

    volatile uint8_t* mmio_base_addr;

    struct virtq_desc* descs;
    struct virtq_avail* avail;
    struct virtq_used* used;
    int queue_index;
    volatile uint16_t* used_index;
    uint16_t last_used_index;
    bool free[VIRTQ_ENTRY_NUM];
    struct virtio_blk_req* blk_req;
};

int virtio_disk_init(void);

// paramentry vizmoshno ne te
//int virtio_write(uint64_t secno, const void *src, size_t nsecs);
//int virtio_read(uint64_t secno, void *dst, size_t nsecs);

#endif
