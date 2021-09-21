# lab2

## Thinking 2.1
首先，这种方式是具有可能性的。完全可以使用虚拟地址作为index去索引，但是需要注意的是会出现cache alias问题，因此需要在进程切换时对cache line进行invalidate的操作。（毕竟每个进程一个cache的想法也不现实）。这样做的好处肯定是速度快了，如果命中则不需要经过MMU；坏处就是增大了进程切换时的开销。（当然这样可能也不够现实，据我了解实际的做法是index用虚拟地址，tag用物理地址，最终也需要物理地址的确认才算命中，但是据说早起ARM架构就是用的传统方法，tag也用了虚拟地址）

物理地址查询的优势就是不会产生cache alias问题，不存在进程切换的开销，总体来说性能可能要更好。

## Thinking 2.2
查询TLB的同时并行地去查询MMU，如果TLB命中则终止查询MMU，没命中则同时去更新TLB和查cache。

## Thinking 2.3
x是一个虚拟地址。

## Thinking 2.4
当宏函数体写成 `do { /* ... */} while (0)` 如果结尾忘记加分号编译器会给出报错，这样能最大程度保证代码的行为一致性。如果写成 `{ /* ... */}` 的语句块，结尾没有加分号，也不会报错，这样的代码会非常蹩脚，即使没有产生什么问题，将来也会是莫名其妙的。

## Exercise 2.1
`LIST_INSERT_AFTER` 宏:
```c++
/*
 * Insert the element "elm" *after* the element "listelm" which is
 * already in the list.  The "field" name is the link element
 * as above.
 */
#define LIST_INSERT_AFTER(listelm, elm, field) do {                    \
                LIST_NEXT((elm), field) = LIST_NEXT((listelm), field); \
                if (LIST_NEXT((elm), field) != NULL) {                 \
                    LIST_NEXT((listelm), field)->field.le_prev =       \
                        &LIST_NEXT((elm), field);                      \
                LIST_NEXT((listelm), field) = (elm);                   \
                (elm)->field.le_prev = &LIST_NEXT((listelm), field);   \
        } while (0)
        // Note: assign a to b <==> a = b
        //Step 1, assign elm.next to listelem.next.
        //Step 2: Judge whether listelm.next is NULL, if not, then assign listelm.pre to a proper value.
        //step 3: Assign listelm.next to a proper value.
        //step 4: Assign elm.pre to a proper value.
```
`LIST_INSERT_TAIL` 宏:
```c++
/*
 * Insert the element "elm" at the tail of the list named "head".
 * The "field" name is the link element as above. You can refer to LIST_INSERT_HEAD.
 * Note: this function has big differences with LIST_INSERT_HEAD !
 */
#define LIST_INSERT_TAIL(head, elm, field) do {                    \
                typeof(elm) tail;                                  \
                if (!LIST_FIRST(head)) {                           \
                    LIST_INSERT_HEAD(head, (elm), field);          \
                } else {                                           \
                    for (tail = LIST_FIRST(head);                  \
                         LIST_NEXT(tail, field);                   \
                         tail = LIST_NEXT(tail, field));           \
                    LIST_INSERT_AFTER(tail, (elm), field);         \
                }                                                  \
        } while (0)
/* finish your code here. */
```

## Thinking 2.5
