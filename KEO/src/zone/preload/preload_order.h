// preload_order.h - Player order hook, called on the main thread.

#ifndef KEO_PRELOAD_ORDER_H
#define KEO_PRELOAD_ORDER_H

void hook_addOrderSelected(void* thisPI, void* destIndoors, int task,
                            void* subject, bool shift, bool addDontClear,
                            const float* location);

#endif // KEO_PRELOAD_ORDER_H
