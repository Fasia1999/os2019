#include<common.h>
#include<klib.h>
#include<am.h>
#include<kernel.h>


static void kmt_spin_init(spinlock_t *lk, const char* name);
static void kmt_spin_lock(spinlock_t *lk);
static void kmt_spin_unlock(spinlock_t *lk);
intptr_t _atomic_xchg(volatile intptr_t *addr, intptr_t newval);

/*static void idle(){
    while(1);
}*/

intptr_t _atomic_xchg(volatile intptr_t *addr, intptr_t newval) {
    //printf("atomic_xchg\n");
  intptr_t result;
  _intr_write(0);
  //trace_status();
  //asm volatile("nop;);
  result = *addr;
  *addr = newval;
  _intr_write(1);
  //printf("atomic_xchg> ");
  //trace_status();
  return result;
}



task_t kernel_task[MAX_CPU];
task_entry tasks[MAX_TASK];
static spinlock_t entry_lock[MAX_TASK];
int current_id[MAX_CPU];
static int past_id[MAX_CPU];

static _Context *kmt_context_save(_Event ev, _Context* context){
    
    int index = past_id[0];
    if(index != -1){
        //printf("locked1: %d\n", index);
        kmt_spin_lock(&entry_lock[index]);
        switch(tasks[index].task->state){
            case RUNNING:
            //case SIGNALED:
                tasks[index].task->state = RUNNABLE;
                break;
            case WAITING_TOBE:
                tasks[index].task->state = WAITING;
                break;
            case RUNNABLE_TOBE:
                tasks[index].task->state = RUNNABLE;
                break;
            case KILLED:
                //assert(0);
                tasks[index].used = 0;
                pmm->free(tasks[index].task->stack);
                break;
            default:
                break;
        }
        kmt_spin_unlock(&entry_lock[index]); 
        //printf("unlocked1\n");
        
    }else{
        //kernel_task[_cpu()].context = *context;        
    }

    int cur_index = current_id[0];
    if(cur_index != -1){
        tasks[cur_index].task->context = *context;
    }else{
        //printf("here\n");
        kernel_task[0].context = *context;
    }

    past_id[0] = cur_index;
    current_id[0] = -1;
    return NULL;
}

static _Context *kmt_context_switch(_Event ev, _Context* context){
    //printf("kmt_context_switch\n");
    int begin = current_id[0] +1 ;
    for(int i =0;i < MAX_TASK;++i){
        int index = (i + begin )% MAX_TASK;
        assert(index <= MAX_TASK);
        
        //printf("locked2: %d\n", index);
        kmt_spin_lock(&entry_lock[index]);
        if( tasks[index].used != 0){
            if(tasks[index].task->state == RUNNABLE){
                tasks[index].task->state = RUNNING;
                kmt_spin_unlock(&entry_lock[index]);
                //printf("unlocked2\n");
                current_id[0] = index;
                //printf("task name: %s\n", tasks[index].task->name);
                return &(tasks[index].task->context);
            }
            else
            {
                //printf("*task name*: %s, state: %d\n", tasks[index].task->name, tasks[index].task->state);
            }
        }
        else
        {
            //printf("*task name*: %s\n", tasks[index].task->name);
        }
        kmt_spin_unlock(&entry_lock[index]);
        //printf("unlocked2\n");
    }
    current_id[0] = -1;
    //printf("*kernel task name*: %s\n", kernel_task[0].name);
    return &(kernel_task[0].context);
}

static void kmt_init(){
    //printf("kmt_init\n");
    for(int i = 0;i < MAX_TASK; ++i){
        kmt_spin_init(&entry_lock[i],"entry_lock");
    }
    for(int i = 0;i < MAX_CPU; ++i){
        kernel_task[i].name = "idle";
        kernel_task[i].state = RUNNABLE;   
    }
    for(int i = 0;i < MAX_CPU; ++i){
        current_id[i] = -1;  
        past_id[i] = -1;      
    }
    for(int i = 0; i < MAX_TASK; ++i){
        tasks[i].used = 0;
    }
    //printf("kmt_init*2 irq\n");
    os->on_irq(INT8_MIN, _EVENT_NULL, kmt_context_save);
    os->on_irq(INT8_MAX, _EVENT_NULL, kmt_context_switch);
}


static int kmt_create(task_t *task, const char * name, void(*entry)(void *arg), void* arg){
    //printf("kmt_create\n");

    if(task == NULL){return -1;}

    task->name = name;
    task->stack = pmm->alloc(STACKSIZE);
    if(task->stack == NULL){
        printf("kmt_create: task stack = NULL\n");
        return -1;
    }
    _Area stack = (_Area){task->stack, task->stack + STACKSIZE};
    //if(arg) printf("kmt_create: %s\n", (char*)arg);
    //printf("kmt_create address: %x\n", entry);
    task->context = *_kcontext(stack, entry, arg);
    task->state = RUNNABLE;
    int c = -1;
    int enable = _intr_read();
    //printf("kmt_create> enable: %d\n", enable);
    _intr_write(0);

    for(int i = 0;i < MAX_TASK; ++i){
        //printf("locked3: %d\n", i);
        kmt_spin_lock(&entry_lock[i]);
        
        if(tasks[i].used == 0){
            c = i;
            tasks[i].used = 1;
            tasks[i].task = task;
            
            kmt_spin_unlock(&entry_lock[i]);
            //printf("unlocked3\n");
            break;
        }
        kmt_spin_unlock(&entry_lock[i]);
        //printf("unlocked3\n");
    }
    _intr_write(enable);
    if(c == -1){
        pmm->free(task->stack);
        return -1;
    }
    //_yield();
    return 0;   
}

static void kmt_teardown(task_t *task){
    
    printf("kmt_teardown> ");
    int enable = _intr_read();
    _intr_write(0);
    trace_status();
    for(int i = 0;i < MAX_TASK;++i){
        //printf("locked4: %d\n", i);
        kmt_spin_lock(&entry_lock[i]);
        
        if((tasks[i].used == 1)&&(tasks[i].task == task)){
            switch(tasks[i].task->state){
                case RUNNABLE:
                    pmm->free(tasks[i].task->stack);
                    tasks[i].used = 0;
                break;
                case RUNNING:
                    tasks[i].task->state = KILLED;
                break;
                case WAITING:
                    //printf("locked5\n");
                    kmt_spin_lock(&tasks[i].task->wait->lock);
                    tasks[i].task->wait->count--;
                    tasks[i].task->wait->wait_queue[i] = 0;
                    kmt_spin_unlock(&tasks[i].task->wait->lock);
                    //printf("unlocked5\n");
                    pmm->free(tasks[i].task->stack);
                    tasks[i].used = 0;
                break;

                default:
                break;
            }            
            kmt_spin_unlock(&entry_lock[i]);
            //printf("unlocked4\n");
            break;
        }
        kmt_spin_unlock(&entry_lock[i]);
        //printf("unlocked4\n");
    }


        
    _intr_write(enable);
    printf("kmt_teardown> ");
    trace_status();

    return;
}

static void kmt_spin_init(spinlock_t *lk, const char* name){
    lk->name = name;
    lk->locked = 0;
    return;
}

static void kmt_spin_lock(spinlock_t *lk){
    _intr_write(0);
    while(_atomic_xchg(&lk->locked,1)) ;//printf("lock_name: %s\n", lk->name);
    return;
}

static void kmt_spin_unlock(spinlock_t *lk){
    _atomic_xchg(&lk->locked,0);
    _intr_write(1);
    return;
}

static void kmt_sem_init(sem_t *sem, const char* name, int count){
    sem->name = name;
    sem->count = count;
    for(int i = 0;i < MAX_TASK;++i){
        sem->wait_queue[i] = 0;
    }
    kmt_spin_init(&sem->lock, name);
    return;
}

static void kmt_sem_wait(sem_t *sem){
    //printf("kmt_sem_wait\n");
    int index = current_id[0];

    int enable = _intr_read();
    _intr_write(0);
    trace_status();

    //printf("locked6\n");
    kmt_spin_lock(&sem->lock);
    
    sem->count--;
    if(sem->count < 0){
        //printf("wait %s, %d\n", sem->name, sem->count);
        sem->wait_queue[index] = 1;
        kmt_spin_unlock(&sem->lock);
        //printf("locked7: %d\n", index);
        kmt_spin_lock(&entry_lock[index]);
       
        if(tasks[index].task->state == RUNNING){
            tasks[index].task->wait = sem;
            tasks[index].task->state = WAITING_TOBE;
        }
        kmt_spin_unlock(&entry_lock[index]);
        //printf("unlocked7\n");
        _intr_write(enable);
        //printf("kmt_sem_wait> ");
        //trace_status();
        _yield();
    }else{
        kmt_spin_unlock(&sem->lock);
        //printf("unlocked6\n");
        _intr_write(enable);
        //printf("kmt_sem_wait> ");
        //trace_status();
    }
    return;
}

static void kmt_sem_signal(sem_t *sem){
    //printf("kmt_sem_signal> ");
    int enable = _intr_read();
    _intr_write(0);  
    //trace_status();  
    if(current_id[0]!=-1){
        //printf("locked8: %d\n", current_id[0]);
        kmt_spin_lock(&entry_lock[current_id[0]]);
        //printf("11111111111111\n");
        if(tasks[current_id[0]].task->state != RUNNING){
            //printf("2\n");
            kmt_spin_unlock(&entry_lock[current_id[0]]);
            //printf("unlocked8\n");
            _intr_write(enable);
            //trace_status();
            return;
        }
        //printf("3\n");
        kmt_spin_unlock(&entry_lock[current_id[0]]);
        //printf("unlocked8\n");
    }
    //printf("4\n");
    kmt_spin_lock(&sem->lock);
    
    if(sem->count < 0){
        //printf("signal %s, %d\n", sem->name, sem->count);
        for(int i = 0;i < MAX_TASK; ++i){
            //printf("locked10: %d\n", i);
            kmt_spin_lock(&entry_lock[i]);
            
            if(sem->wait_queue[i]==1){
                if(tasks[i].task->state == WAITING){            
                    tasks[i].task->state = RUNNABLE;
                }else if(tasks[i].task->state == WAITING_TOBE){
                    tasks[i].task->state = RUNNABLE_TOBE;
                }
                sem->wait_queue[i] = 0;
                kmt_spin_unlock(&entry_lock[i]); 
                //printf("unlocked10\n");
                break;
            }
            kmt_spin_unlock(&entry_lock[i]);    
            //printf("unlocked10\n");
        }
    }
    sem->count ++;
    kmt_spin_unlock(&sem->lock);
    //printf("unlocked9\n");
    _intr_write(enable);
    //trace_status();
    return;
}
MODULE_DEF(kmt){
    .init = kmt_init,
    .create = kmt_create,
    .teardown = kmt_teardown,    
    .spin_init = kmt_spin_init, 
    .spin_lock = kmt_spin_lock,
    .spin_unlock = kmt_spin_unlock,
    .sem_init = kmt_sem_init,
    .sem_wait = kmt_sem_wait,
    .sem_signal = kmt_sem_signal,
};