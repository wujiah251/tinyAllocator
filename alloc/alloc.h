#include <mutex>
#include <cstdlib>
#include <iostream>
#include <string.h>

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
    static void dellocate(void *p);

    // 一级空间配置器直接使用realloc
    static void *reallocate(void *p, size_t new_size);

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
    using value_type = T;
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
    static char *chunkAlloc(size_t size, int &nObjects);
};

template <int inst>
void *malloc_alloc_template<inst>::oom_malloc(size_t n)
{
    void (*my_malloc_handler)();
    void *result;
    while (1)
    {
        my_malloc_handler = malloc_alloc_oom_handler;
        if (0 == my_malloc_handler)
        {
            throw std::bad_alloc();
        }
        (*my_malloc_handler)();
        result = malloc(n);
        if (result)
            return result;
    }
}

template <int inst>
void *malloc_alloc_template<inst>::oom_realloc(void *p, size_t n)
{
    void (*my_malloc_handler)();
    void *result;
    while (1)
    {
        my_malloc_handler = malloc_alloc_oom_handler;
        if (0 == my_malloc_handler)
        {
            throw std::bad_alloc();
        }
        // 执行一次my_malloc_handler
        (*my_malloc_handler)();
        result = realloc(p, n);
        if (result)
            return result;
    }
}

template <int inst>
void (*malloc_alloc_template<inst>::malloc_alloc_oom_handler)() = nullptr;

template <int inst>
void *malloc_alloc_template<inst>::allocate(size_t n)
{
    void *result = malloc(n);
    if (0 == result)
    {
        // 内存不够调用oom_malloc
        result = oom_malloc(n);
    }
    return result;
}

template <int inst>
void malloc_alloc_template<inst>::dellocate(void *p)
{
    free(p);
}

template <int inst>
void *malloc_alloc_template<inst>::reallocate(void *p, size_t new_size)
{
    void *result = realloc(p, new_size);
    if (result == 0)
    {
        // 无法满足需要的内存时，调用oom_realloc
        result = oom_realloc(p, new_size);
    }
    return result;
}

template <typename T>
size_t myAllocator<T>::roundUp(size_t bytes)
{
    // +7在把二进制表示的后三位置位0
    return (((bytes) + (size_t)ALIGN - 1) & ~(size_t)ALIGN - 1);
}

template <typename T>
size_t myAllocator<T>::freeListIndex(size_t bytes)
{
    return (((bytes) + (size_t)ALIGN - 1) / (size_t)ALIGN - 1);
}

template <typename T>
void *myAllocator<T>::refill(size_t n)
{
    // 默认分配20个Object
    int nObjects = 20;
    char *chunk = chunkAlloc(n, nObjects);
    Object *volatile *myFreeList;
    Object *result;
    Object *currentObject;
    Object *nextObject;
    int i;

    if (1 == nObjects)
    {
        // 如果只获得了一个chunk，则返回给调用者，free-list无新节点
        return chunk;
    }
    // 找到匹配的
    myFreeList = freeList + freeListIndex(n);
    // result保存要返回的那个chunk，其他插入free-list
    result = (Object *)chunk;
    *myFreeList = nextObject = (Object *)(chunk + n);
    // 还剩下nObjects-1个块，需要建立nObjects-2个链接
    for (i = 2; i < nObjects; ++i)
    {
        currentObject = nextObject;
        nextObject = (Object *)((char *)nextObject + n);
        currentObject->freeListLink = nextObject;
    }
    // 末尾置为空
    nextObject->freeListLink = 0;
    return result;
}

template <typename T>
char *myAllocator<T>::chunkAlloc(size_t size, int &nObjects)
{
    char *result;
    // 需要的空间大小
    size_t totalBytes = size * nObjects;
    // 当前内存池还剩下多少字节
    size_t bytesLeft = endFree - startFree;

    if (bytesLeft >= totalBytes)
    {
        // 剩下的内存能够满足需求
        result = startFree;
        startFree += totalBytes;
        return result;
    }
    else if (bytesLeft >= size)
    {
        // 能至少分配一个，那么就全部分配
        nObjects = (int)(bytesLeft / size);
        totalBytes = size * nObjects;
        result = startFree;
        startFree += totalBytes;
        return result;
    }
    else
    {
        // 剩下的内存连一个区块的大熊啊都无法提供
        // 这是要通过malloc获取的字节数
        size_t bytesToGet = 2 * totalBytes + roundUp(heapSize >> 4);
        if (bytesLeft > 0)
        {
            // 内存池还有一些零头，先配给适当的freelist
            // 首先找到适合的freelist
            Object *volatile *myFreeList = freeList + freeListIndex(bytesLeft);
            // 将剩下的chunk插入到freelist中
            ((Object *)startFree)->freeListLink = *myFreeList;
            *myFreeList = (Object *)startFree;
        }
        startFree = (char *)malloc(bytesToGet);
        if (0 == startFree)
        {
            // malloc分配空间失败
            int i;
            Object *volatile *myFreeList;
            Object *p;
            for (i = size; i <= (size_t)MAX_CHUNK;
                 i += (size_t)ALIGN)
            {
                myFreeList = freeList + freeListIndex(i);
                p = *myFreeList;
                if (p != 0)
                {
                    // freelist中还有没使用的chunk
                    *myFreeList = p->freeListLink;
                    startFree = (char *)p;
                    endFree = startFree + i;
                    return (chunkAlloc(size, nObjects));
                }
            }
            // 没有其他办法了，只能调用一级空间配置器了
            endFree = 0;
            // 没有成功会抛出异常
            startFree = (char *)malloc_alloc::allocate(bytesToGet);
        }
        heapSize += bytesToGet;
        endFree = startFree + bytesToGet;
        // 递归调用自己，修正nObjects
        return (chunkAlloc(size, nObjects));
    }
}

template <typename T>
T *myAllocator<T>::allocate(size_t n)
{
    n = n * sizeof(T);
    void *res = 0;
    if (n > (size_t)MAX_CHUNK)
    {
        // 超出free-list中最大的chunk块的大小，直接调用一级空间配置器
        res = malloc_alloc::allocate(n);
    }
    else
    {
        // 找到对应的list
        Object *volatile *myFreeList = freeList + freeListIndex(n);
        {
            // 建立一个临界区
            std::lock_guard<std::mutex> guard(mtx);
            Object *result = *myFreeList;
            if (result == 0)
            {
                // 链表为空
                res = refill(roundUp(n));
            }
            else
            {
                *myFreeList = result->freeListLink;
                res = result;
            }
        }
    }
    return (T *)res;
}

template <typename T>
void myAllocator<T>::deallocate(void *p, size_t n)
{
    if (n > (size_t)MAX_CHUNK)
    {
        // 大于最大chunk，直接带哦用一级空间配置器
        malloc_alloc::dellocate(p);
    }
    else
    {
        Object *volatile *myFreeList = freeList + freeListIndex(n);
        Object *q = (Object *)p;
        {
            // 在临界区中插入chunk
            std::lock_guard<std::mutex> guard(mtx);
            q->freeListLink = *myFreeList;
            *myFreeList = q;
        }
    }
}

template <typename T>
void *myAllocator<T>::reallocate(void *p, size_t oldSize, size_t newSize)
{
    void *result;
    size_t copySize;
    if (oldSize > (size_t)MAX_CHUNK && (size_t)MAX_CHUNK)
    {
        // 超出最大块的字节数直接调用realloc
        return (realloc(p, newSize));
    }
    if (roundUp(oldSize) == roundUp(newSize))
    {
        // 不需要扩容
        return p;
    }
    // 重新分配内存
    result = allocate(newSize);
    copySize = newSize;
    // 讲之前的内存赋值过来
    memcpy(result, p, copySize);
    deallocate(p, oldSize);
    return result;
}

template <typename T>
char *myAllocator<T>::startFree = nullptr;

template <typename T>
char *myAllocator<T>::endFree = nullptr;

template <typename T>
size_t myAllocator<T>::heapSize = 0;

// 初始化16个链表
template <typename T>
typename myAllocator<T>::Object *volatile myAllocator<T>::freeList[NFREELISTS] =
    {nullptr, nullptr, nullptr, nullptr,
     nullptr, nullptr, nullptr, nullptr,
     nullptr, nullptr, nullptr, nullptr,
     nullptr, nullptr, nullptr, nullptr};

template <typename T>
std::mutex myAllocator<T>::mtx;