# Print a page table

1. 在`kernel/def.h`中增加函数声明`void vmprint(void)`并在`kernel/vm.c`中定义它

```c
void vmprint(pagetable_t p) {
  printf("page table %p\n", p);
  _vmprint(p, 0);
}

// vmprint的递归实现
void _vmprint(pagetable_t p, int level) {
  uint64 pa;
  
  for(int i = 0; i<512; ++i) {
    if(p[i] & PTE_V) {
      pa = PTE2PA(p[i]);
      switch(level) {
      case 0:
        printf("..%d: pte %p pa %p\n", i, p[i], pa);
        _vmprint((pagetable_t)pa, level+1);
        break;
      case 1:
        printf(".. ..%d: pte %p pa %p\n", i, p[i], pa);
        _vmprint((pagetable_t)pa, level+1);
        break;
      case 2:
        printf(".. .. ..%d: pte %p pa %p\n", i, p[i], pa);
        break;
      }
    }
  }
}
```
