#include <common.h>
#include <klib.h>
#include <../include/devices.h>


#define HANDLER_MAX_NUM 0x80
static struct handler_entry{
  handler_t handler;
  int event;
  int seq;
  int next;
}handlers[HANDLER_MAX_NUM]={
  {.next = -1},
};
static int handler_num = 0;

void echo_task(void *name) {
  //printf("echo_task: %s\n", name);
  device_t *tty = dev_lookup(name);
  while (1) {
    //printf("****************echo_task********************\n");
    char line[128], text[128];
    memset(line, 0, sizeof(line));
    memset(text, 0, sizeof(text));
    strcpy(text, "Shell>");
    tty->ops->write(tty, 2, text, strlen(text));
    int nread = tty->ops->read(tty, 0, line, sizeof(line));
    line[nread - 1] = 0;
    strcpy(text, "Echo: ");
    strcat(text, line);
    strcat(text, "\n");
    tty->ops->write(tty, 0, text, strlen(text));
  }
}

static void os_init() {
  //printf("os_init\n");
  pmm->init();
  kmt->init();
  dev->init();
  enable_interrupt();
  kmt->create(pmm->alloc(sizeof(task_t)), "echo_task", echo_task, "tty1");
  kmt->create(pmm->alloc(sizeof(task_t)), "echo_task", echo_task, "tty2");
  kmt->create(pmm->alloc(sizeof(task_t)), "echo_task", echo_task, "tty3");
  kmt->create(pmm->alloc(sizeof(task_t)), "echo_task", echo_task, "tty4");
}

static void hello() {
  trace_status();
  trace_count();
  trace_compare();
  for (const char *ptr = "Hello from CPU #"; *ptr; ptr++) {
    _putc(*ptr);
    
  }
  _putc("12345678"[0]); _putc('\n');  
}

static void os_run() {
  //printf("os_run\n");
  hello();   
  _intr_write(1);
  while (1) {
    //printf("*******************os_run****************\n");
  }
}


static _Context *os_trap(_Event ev, _Context *context) {
  _Context *ret = NULL;
  int iter = handlers[0].next;
  while(iter != -1){
    if( (handlers[iter].event == _EVENT_NULL) || (handlers[iter].event == ev.event)){
      _Context* next = handlers[iter].handler(ev, context);
      if(next) ret = next;
    }
    iter = handlers[iter].next;
  }
  return ret;
}

static void os_on_irq(int seq, int event, handler_t handler) {
  handler_num++;
  assert(handler_num < HANDLER_MAX_NUM);
  if(handler_num >= HANDLER_MAX_NUM){return;}
  handlers[handler_num].handler = handler;
  handlers[handler_num].event = event;
  handlers[handler_num].seq = seq;
  handlers[handler_num].next = -1;
  int iter = 0;
  int next = handlers[iter].next;
  while(next!=-1){
    if((handlers[next].event == event) && (handlers[next].seq > seq)){
      break;
    }else{
      iter = next;
      next = handlers[iter].next;
    }
  }
  handlers[handler_num].next = next;
  handlers[iter].next = handler_num;
  
}

MODULE_DEF(os) {
  .init   = os_init,
  .run    = os_run,
  .trap   = os_trap,
  .on_irq = os_on_irq,
};
