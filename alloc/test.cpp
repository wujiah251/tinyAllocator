#include "alloc.h"
#include <vector>
#include <ctime>
#include <chrono>
using namespace std;
using namespace chrono;

struct node
{
    int a;
    int b;
    int c;
    int d;
    int f;
    int g;
    int nums[100];
};

typedef node type;
#define times 10000

vector<type, myAllocator<type>> vec1;
vector<type> vec2;

int main()
{
    system_clock::time_point start1, end1, start2, end2;
    start1 = system_clock::now();
    type tmp;
    for (int i = 0; i < times; ++i)
    {
        vec1.push_back(tmp);
    }
    end1 = system_clock::now();
    system_clock::duration duration1 = end1.time_since_epoch() - start1.time_since_epoch();
    start2 = system_clock::now();
    for (int i = 0; i < times; ++i)
    {
        vec2.push_back(tmp);
    }
    end2 = system_clock::now();
    system_clock::duration duration2 = end2.time_since_epoch() - start2.time_since_epoch();
    time_t microseconds_since_epoch = duration_cast<microseconds>(duration1).count();
    cout << microseconds_since_epoch << endl;
    microseconds_since_epoch = duration_cast<microseconds>(duration2).count();
    cout << microseconds_since_epoch << endl;
    return 0;
}