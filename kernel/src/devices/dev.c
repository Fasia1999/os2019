#include <common.h>
#include <devices.h>

extern devops_t tty_ops, fb_ops, rd_ops, input_ops;

#define DEVICES(_) \
  _(0, rd_t,    "ramdisk0", 1, &rd_ops) \
  _(1, rd_t,    "ramdisk1", 2, &rd_ops) \
  _(2, input_t, "input",    1, &input_ops) \
  _(3, fb_t,    "fb",       1, &fb_ops) \
  _(4, tty_t,   "tty1",     1, &tty_ops) \
  _(5, tty_t,   "tty2",     2, &tty_ops) \
  _(6, tty_t,   "tty3",     3, &tty_ops) \
  _(7, tty_t,   "tty4",     4, &tty_ops)

#define DEV_CNT(...) + 1
device_t *devices[0 DEVICES(DEV_CNT)];

const size_t devices_cnt = DEVICES(DEV_CNT);
device_t *dev_lookup(const char *name) {
  for (int i = 0; i < LENGTH(devices); i++) 
    if (strcmp(devices[i]->name, name) == 0)
      return devices[i];
  panic("lookup device failed.");
  return NULL;
}

static device_t *dev_create(size_t dev_size, const char* dev_name, int dev_id, devops_t *dev_ops) {

  void *data = pmm->alloc(dev_size);

  device_t *dev = pmm->alloc(sizeof(device_t));
//  printf("dev %p\n", dev);
  *dev = (device_t) {
    .name = dev_name,
    .ptr = data,
    .id = dev_id,
    .ops = dev_ops,
  };
  return dev;
}

void tty_task(void *arg);
void input_task(void *arg);

#define CREATE(id, device_type, dev_name, dev_id, dev_ops) \
  devices[id] = dev_create(sizeof(device_type), dev_name, dev_id, dev_ops);

#define INIT(id, device_type, dev_name, dev_id, dev_ops) \
  devices[id]->ops->init(devices[id]);

static void dev_init() {
  //printf("dev_init\n");
  DEVICES(CREATE);
  DEVICES(INIT);
  //printf("dev_init_input1\n");
  kmt->create(pmm->alloc(sizeof(task_t)), "input_task", input_task, NULL);
  //_yield();
  //printf("dev_init_input2\n");
  //printf("dev_init_tty1\n");
  kmt->create(pmm->alloc(sizeof(task_t)), "input_task", tty_task, NULL);
  //_yield();
  //printf("dev_init_tty2\n");
  
}

MODULE_DEF(dev) {
  .init = dev_init,
};
