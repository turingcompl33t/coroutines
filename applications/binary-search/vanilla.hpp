// vanilla.hpp
// Vanilla binary search.

#ifndef VANILLA_BS_H
#define VANILLA_BS_H

template <typename Iter>
bool vanilla_binary_search(Iter first, Iter last, int val)
{
    auto len = last - first;
    while (len > 0)
    {
        auto const half   = len / 2;
        auto const middle = first + half;

        auto middle_key = *middle;
        if (middle_key < val)
        {
            first = middle + 1;
            len = len - half - 1;
        }
        else
        {
            len = half;
        }

        if (middle_key == val)
        {
            return true;
        }
    }
    
    return false;
}

#endif // VANILLA_BS_H