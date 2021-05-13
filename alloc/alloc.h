#pragma once
#include <mutex>
#include <cstdlib>
#include <iostream>
#include "alloc.cpp"

/*
 * 模仿SGI STL二级空间配置器内存源码模板实现
 * 多线程-线程安全的问题
*/

/*
 * 一级空间配置器
 * 
*/
template <int inst>
class malloc_alloc_template
{
private:
    // 以下函数用于处理内存不足的情况
    // oom：out of memory
    static void *oom_malloc(size_t);
    static void *oom_realloc(void *, size_t);
    static void (*malloc_alloc_oom_handler)();

public:
    // 一级空间配置器直接使用malloc
    static void *allocate(size_t n);

    // 一级空间配置器直接使用free
    static void dellocate(void *p, size_t);

    // 一级空间配置器直接使用realloc
    static void *reallocate(void *p, size_t, size_t new_size);

    // 参数是一个函数指针，可以指定自己的oom-handler
    static void (*set_malloc_handler(void (*f)()))()
    {
        void (*old)() = malloc_alloc_oom_handler;
        malloc_alloc_oom_handler = f;
        return old;
    }
};

typedef malloc_alloc_template<0> malloc_alloc;

template <typename T>
class myAllocator
{
public:
    using valueType = T;
    constexpr myAllocator() noexcept
    {
    }
    constexpr myAllocator(const myAllocator &) noexcept = default;
    template <class OtherType>
    constexpr myAllocator(const myAllocator<OtherType> &) noexcept
    {
    }
    // 开辟内存
    T *allocate(size_t n);

    // 释放内存
    void deallocate(void *p, size_t n);

    // 内存扩容或者缩容
    void *reallocate(void *p, size_t oldSize, size_t newSize);

    // 对象构造
    void contruct(T *p, const T &val)
    {
        // placement new
        new (p) T(val);
    }
    void destroy(T *p)
    {
        // 只调用析构函数，不释放内存空间
        p->~T();
    }

private:
    enum
    {
        ALIGN = 8
    };
    // 自由链表是从8字节开始，以8字节为对齐方式
    enum
    {
        MAX_CHUNK = 128
    };
    // 内存池最大的chunk的字节数为128bytes
    enum
    {
        NFREELISTS = 16
    };
    // 自由链表的个数
    union Object
    {
        union Object *freeListLink;
        char clientData[1];
    };

    // 已经分配的chunk块的使用情况
    // startFree到endFree保存着已经分配到内存池的可用内存
    static char *startFree;
    static char *endFree;
    static size_t heapSize;

    // freeList表示存储自由链表数组的起始地址
    static Object *volatile freeList[NFREELISTS];

    // 用于控制线程安全
    static std::mutex mtx;

    // 上调所需要分配的内存到大于它的最小的8的倍数
    static size_t roundUp(size_t bytes);

    // 返回bytes大小的chunk在freeList中的索引
    static size_t freeListIndex(size_t bytes);

    //从内存池中获得多块内存并加入到free-list
    static void *refill(size_t n);

    // 分配自由链表，chunk块
    // nObjects会保存分配了多少个chunk
    static void *chunkAlloc(size_t size, int &nObjects);
};