#include "alloc.h"
#include <vector>
using namespace std;

int main()
{
    vector<int, myAllocator<int>> vec;
    cout << "construct suscess" << endl;
    for (int i = 0; i < 5; ++i)
    {
        vec.push_back(1);
    }
    cout << "push_back suscess" << endl;
    for (int val : vec)
    {
        cout << val << ' ';
    }
    cout << endl;
    return 0;
}