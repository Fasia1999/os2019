#include <kernel.h>
#include <klib.h>

int main() {
  _ioe_init();
  _cte_init(os->trap);
  os->init(); 
  os->run();
  return 1;
}
