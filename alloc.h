#include <mutex>
#include <iostream>

/*
 * 模仿SGI STL二级空间配置器内存源码模板实现
 * 多线程-线程安全的问题
*/

template <int _inst>
class __malloc_alloc_template
{
private:
    static void *_S_oom_malloc(size_t);
    static void *_S_oom_realloc(void *, size_t);
    static void (*__malloc_alloc_oom_handler)();

public:
    static void *allocate(size_t __n)
    {
        void *__result = malloc(__n);
        if (0 == __result)
            __result = _S_oom_malloc(__n);
        return __result;
    }
    static void dellocate(void *__p, size_t)
    {
        free(__p);
    }
};

typedef __malloc_alloc_template<0> malloc_alloc;

template <typename T>
class myAllocator
{
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
        NFREELISTS = MAX_CHUNK / ALIGN;
    };
    // 自由链表的个数
    union Object
    {
        union Object *freeListLink;
        char clientData[1];
    };

    // 已经分配的chunk块的使用情况
    static char *startFree;
    static char *endFree;
    static size_t heapSize;

    // freeList表示存储自由链表数组的起始地址
    static Object *volatile freeList[NFREELISTS];

    // 用于控制线程安全
    static std::mutex mtx;

    // 上调所需要分配的内存到大于它的最小的8的倍数
    static size_t roundUp(size_t bytes)
    {
        // 先加7，再把最后三位置为0
        return (((bytes) + (size_t)ALIGN - 1) & ~((size_t)ALIGN - 1));
    }

    // 返回bytes大小的chunk在freeList中的索引
    static size_t freeListIndex(size_t bytes)
    {
        return (((bytes) + (size_t)ALIGN - 1) / (size_t)ALIGN - 1);
    }

    // 讲分配好的内存进行连接
    static void *refill(size_t n)
    {
        int nobjs = 20;
        char *chunk = chunk_alloc(size_t size, int &nobjs);
    }

public:
    using valueType = T;
    using valuePtr = T *;
    constexpr myAllocator() noexcept
    {
    }
    constexpr myAllocator(const myAllocator<Other_Type> &) noexcept
    {
    }
    // 开辟内存
    value_ptr allocate(size_t n)
    {
        n = n * sizeof(valueType);
        void *res = 0;
        if (n > (size_t)MAX_CHUNK)
        {
            // 超出free-list中最大的chunk块的大小，直接调用malloc
            res = malloc_alloc::allocate(n);
        }
        else
        {
        }
    }
};