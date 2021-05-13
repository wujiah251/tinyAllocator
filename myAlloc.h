#pragma once

#include <mutex>
#include <cstdlib>
#include <iostream>

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
    static void *allocate(size_t n)
    {
        void *result = malloc(n);
        if (0 == result)
        {
            // 内存不够调用oom_malloc
            result = oom_malloc(n);
        }
        return result;
    }

    // 一级空间配置器直接使用free
    static void deallocate(void *p, size_t /* __n */)
    {
        free(p);
    }

    // 一级空间配置器直接使用realloc
    static void *reallocate(void *p, size_t /* old_sz */, size_t new_size)
    {
        void *result = realloc(p, new_size);
        if (0 == result)
        {
            // 无法满足需要的内存时，调用oom_realloc
            result = oom_realloc(p, new_size);
        }
        return result;
    }

    static void (*set_malloc_handler(void (*f)()))()
    {
        void (*old)() = malloc_alloc_oom_handler;
        malloc_alloc_oom_handler = f;
        return (old);
    }
};

template <int inst>
void (*malloc_alloc_template<inst>::malloc_alloc_oom_handler)() = nullptr;

template <int inst>
void *
malloc_alloc_template<inst>::oom_malloc(size_t n)
{
    void (*my_malloc_handler)();
    void *result;

    for (;;)
    {
        my_malloc_handler = malloc_alloc_oom_handler;
        if (0 == my_malloc_handler)
        {
            throw std::bad_alloc();
        }
        (*my_malloc_handler)();
        result = malloc(__n);
        if (result)
        {
            return (result);
        }
    }
}

template <int inst>
void *malloc_alloc_template<inst>::oom_realloc(void *p, size_t n)
{
    void (*my_malloc_handler)();
    void *result;

    for (;;)
    {
        my_malloc_handler = malloc_alloc_oom_handler;
        if (0 == my_malloc_handler)
        {
            throw std::bad_alloc();
        }
        (*my_malloc_handler)();
        result = realloc(p, n);
        if (result)
            return (result);
    }
}

typedef malloc_alloc_template<0> malloc_alloc;

template <typename T>
class myallocator
{
public:
    using value_type = T;

    constexpr myallocator() noexcept
    { // construct default allocator (do nothing)
    }
    constexpr myallocator(const myallocator &) noexcept = default;
    template <class _Other>
    constexpr myallocator(const myallocator<_Other> &) noexcept
    { // construct from a related allocator (do nothing)
    }

    // 开辟内存
    T *allocate(size_t __n)
    {
        __n = __n * sizeof(T);

        void *__ret = 0;

        if (__n > (size_t)_MAX_BYTES)
        {
            __ret = malloc_alloc::allocate(__n);
        }
        else
        {
            Object *volatile *__my_free_list = _S_free_list + _S_freelist_index(__n);

            std::lock_guard<std::mutex> guard(mtx);

            Object *__result = *__my_free_list;
            if (__result == 0)
                __ret = _S_refill(_S_round_up(__n));
            else
            {
                *__my_free_list = __result->_M_free_list_link;
                __ret = __result;
            }
        }
        return (T *)__ret;
    }

    // 释放内存
    void deallocate(void *__p, size_t __n)
    {
        if (__n > (size_t)_MAX_BYTES)
        {
            malloc_alloc::deallocate(__p, __n);
        }
        else
        {
            Object *volatile *__my_free_list = _S_free_list + _S_freelist_index(__n);
            Object *__q = (Object *)__p;

            std::lock_guard<std::mutex> guard(mtx);

            __q->_M_free_list_link = *__my_free_list;
            *__my_free_list = __q;
            // lock is released here
        }
    }

    // 内存扩容&缩容
    void *reallocate(void *__p, size_t __old_sz, size_t __new_sz)
    {
        void *__result;
        size_t __copy_sz;

        if (__old_sz > (size_t)_MAX_BYTES && __new_sz > (size_t)_MAX_BYTES)
        {
            return (realloc(__p, __new_sz));
        }
        if (_S_round_up(__old_sz) == _S_round_up(__new_sz))
            return (__p);
        __result = allocate(__new_sz);
        __copy_sz = __new_sz > __old_sz ? __old_sz : __new_sz;
        memcpy(__result, __p, __copy_sz);
        deallocate(__p, __old_sz);
        return (__result);
    }

    // 对象构造
    void construct(T *__p, const T &val)
    {
        new (__p) T(val);
    }

    // 对象析构
    void destory(T *__p)
    {
        __p->~T();
    }

private:
    enum
    {
        _ALIGN = 8
    }; // 自由链表是从8字节开始，以8字节为对齐方式，一直扩充到128
    enum
    {
        _MAX_BYTES = 128
    }; // 内存池最大的chunk块
    enum
    {
        _NFREELISTS = 16
    }; // 自由链表的个数

    // 每一个chunk块的头信息， _M_free_list_link存储下一个chunk块的地址
    union Object
    {
        union Object *_M_free_list_link;
        char client_data[1];
    };

    // 已分配的内存chunk块的使用情况
    static char *startFree;
    static char *_S_end_free;
    static size_t _S_heap_size;

    // freeList表示存储自由链表数组的起始地址
    static Object *volatile _S_free_list[_NFREELISTS];

    // 用于控制线程安全
    static std::mutex mtx;

    // 将 __bytes 上调至最邻近的8的倍数
    static size_t _S_round_up(size_t __bytes)
    {
        return (((__bytes) + (size_t)_ALIGN - 1) & ~((size_t)_ALIGN - 1));
    }

    // 返回 _bytes 大小的小额区块位于 free-list 中的编号
    static size_t _S_freelist_index(size_t __bytes)
    {
        return (((__bytes) + (size_t)_ALIGN - 1) / (size_t)_ALIGN - 1);
    }

    // 把分配好的chunk块进行连接
    static void *_S_refill(size_t __n)
    {
        int __nobjs = 20;
        char *__chunk = _S_chunk_alloc(__n, __nobjs);
        Object *volatile *__my_free_list;
        Object *__result;
        Object *__currentObject;
        Object *__nextObject;
        int __i;

        if (1 == __nobjs)
            return (__chunk);
        __my_free_list = _S_free_list + _S_freelist_index(__n);

        /* Build free list in chunk */
        __result = (Object *)__chunk;
        *__my_free_list = __nextObject = (Object *)(__chunk + __n);
        for (__i = 1;; __i++)
        {
            __currentObject = __nextObject;
            __nextObject = (Object *)((char *)__nextObject + __n);
            if (__nobjs - 1 == __i)
            {
                __currentObject->_M_free_list_link = 0;
                break;
            }
            else
            {
                __currentObject->_M_free_list_link = __nextObject;
            }
        }
        return (__result);
    }

    // 分配自由链表，chunk块
    static char *_S_chunk_alloc(size_t __size, int &__nobjs)
    {
        char *__result;
        size_t __total_bytes = __size * __nobjs;
        size_t __bytes_left = _S_end_free - startFree;

        if (__bytes_left >= __total_bytes)
        {
            __result = startFree;
            startFree += __total_bytes;
            return (__result);
        }
        else if (__bytes_left >= __size)
        {
            __nobjs = (int)(__bytes_left / __size);
            __total_bytes = __size * __nobjs;
            __result = startFree;
            startFree += __total_bytes;
            return (__result);
        }
        else
        {
            size_t __bytes_to_get =
                2 * __total_bytes + _S_round_up(_S_heap_size >> 4);
            // Try to make use of the left-over piece.
            if (__bytes_left > 0)
            {
                Object *volatile *__my_free_list =
                    _S_free_list + _S_freelist_index(__bytes_left);

                ((Object *)startFree)->_M_free_list_link = *__my_free_list;
                *__my_free_list = (Object *)startFree;
            }
            startFree = (char *)malloc(__bytes_to_get);
            if (nullptr == startFree)
            {
                size_t __i;
                Object *volatile *__my_free_list;
                Object *__p;
                // Try to make do with what we have.  That can't
                // hurt.  We do not try smaller requests, since that tends
                // to result in disaster on multi-process machines.
                for (__i = __size;
                     __i <= (size_t)_MAX_BYTES;
                     __i += (size_t)_ALIGN)
                {
                    __my_free_list = _S_free_list + _S_freelist_index(__i);
                    __p = *__my_free_list;
                    if (0 != __p)
                    {
                        *__my_free_list = __p->_M_free_list_link;
                        startFree = (char *)__p;
                        _S_end_free = startFree + __i;
                        return (_S_chunk_alloc(__size, __nobjs));
                        // Any leftover piece will eventually make it to the
                        // right free list.
                    }
                }
                _S_end_free = 0; // In case of exception.
                startFree = (char *)malloc_alloc::allocate(__bytes_to_get);
                // This should either throw an
                // exception or remedy the situation.  Thus we assume it
                // succeeded.
            }
            _S_heap_size += __bytes_to_get;
            _S_end_free = startFree + __bytes_to_get;
            return (_S_chunk_alloc(__size, __nobjs));
        }
    }
};

template <typename T>
char *myallocator<T>::startFree = nullptr;

template <typename T>
char *myallocator<T>::_S_end_free = nullptr;

template <typename T>
size_t myallocator<T>::_S_heap_size = 0;

template <typename T>
typename myallocator<T>::Object *volatile myallocator<T>::_S_free_list[_NFREELISTS] =
    {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

template <typename T>
std::mutex myallocator<T>::mtx;