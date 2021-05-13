#include "alloc.h"
#include <vector>
using namespace std;

int main()
{
    vector<int, myAllocator<int>> vec;
    for (int i = 0; i < 100; ++i)
    {
        vec.push_back(1);
    }
    for (int val : vec)
    {
        cout << val << ' ';
    }
    cout << endl;
    return 0;
}