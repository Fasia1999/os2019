#include <kernel.h>
#include <klib.h>

void enable_interrupt();
int main() {
  enable_interrupt();
  _ioe_init();
  _cte_init(os->trap);
  //_yield();
  // call sequential init code
  //printf("main\n");
  os->init(); 
  os->run();
  //_mpe_init(os->run); // all cores call os->run()
  
//  printf("got here\n");
  return 1;
}
