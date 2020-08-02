// state_machine.hpp
// Interleaved binary search via hand-crafted state machine.
//
// Adapted from original code by Gor Nishanov.

#ifndef STATE_MACHINE_BS_H
#define STATE_MACHINE_BS_H

#include <vector>
#include <xmmintrin.h>

struct Frame
{
    enum State { KEEP_GOING, FOUND, NOT_FOUND, EMPTY };

    int const* first;
    int const* last;
    int const* middle;

    size_t len;
    size_t half;

    int val;

    State state = EMPTY;

    template <typename T>
    static void prefetch(T const& x)
    {
        _mm_prefetch(reinterpret_cast<char const*>(&x), _MM_HINT_NTA);
    }

    void init(int const* first_, int const* last_, int key_)
    {
        val   = key_;
        first = first_;
        last  = last_;

        len = last - first;

        if (0 == len)
        {
            state = NOT_FOUND;
            return;
        }

        half   = len / 2;
        middle = first + half;
        state  = KEEP_GOING;
        
        prefetch(*middle);
    }

    bool run()
    {
        auto const x = *middle;
        if (x < val)
        {
            first = middle;
            ++first;
            len = len - half - 1;
        }
        else
        {
            len = half;
        }

        if (x == val)
        {
            state = FOUND;
            return true;
        }

        if (len > 0)
        {
            half   = len / 2;
            middle = first + half;
            
            prefetch(*middle);
            
            return false;
        }

        state = NOT_FOUND;
        return true;
    }
};

bool state_machine_binary_search(
    int const* first, 
    int const* last, 
    int const  key)
{
    Frame f{};
    
    f.init(first, last, key);
    while (Frame::KEEP_GOING == f.state)
    {
        f.run();
    }

    return Frame::FOUND == f.state;
}

size_t state_machine_multi_lookup(
    std::vector<int> const& dataset, 
    std::vector<int> const& lookups, 
    size_t n_streams)
{
    std::vector<Frame> frames(n_streams);

    size_t const N = n_streams - 1;
    size_t i = N;

    // the number of keys found amongst all lookups
    size_t result = 0;

    auto const beg = &dataset[0];
    auto const end = beg + dataset.size();

    for (auto key : lookups)
    {
        auto* frame = &frames[i];
        if (Frame::State::KEEP_GOING != frame->state)
        {
            // this frame is not currently running a query;
            // initialize it and move on to the next lookup
            frame->init(beg, end, key);
            i = (0 == i) ? N : i - 1;
        }
        else
        {
            // switch between active frames until one completes;
            // once this happens, we can move on to start next lookup
            for (;;)
            {
                if (frame->run())
                {
                    // frame::run() returned `true`, implying that
                    // this lookup operation is complete

                    if (Frame::State::FOUND == frame->state)
                    {
                        // a positive search result 
                        ++result;
                    }

                    // initialize 
                    frame->init(beg, end, key);
                    i = (0 == i) ? N : i - 1;
                    break;
                }
                
                // current frame is not yet complete and has 
                // initiated a prefetch; switch to the next frame 
                i = (0 == i) ? N : i - 1;
                frame = &frames[i];
            }
        }
    }

    // at the time that the outer loop above completes,
    // we have looked at all of the keys in the vector of lookups,
    // but we have not necessarily completed all of the searches,
    // thus we need to loop here until all frames report
    // that they have completed the lookup that they are computing

    bool more_work = false;
    do
    {
        more_work = false;
        for (auto& frame : frames)
        {
            if (Frame::State::KEEP_GOING == frame.state)
            {
                more_work = true;
                if (frame.run() 
                 && Frame::State::FOUND == frame.state)
                {
                    ++result;
                }
            }
        }
    } while (more_work);

    return result;
}

#endif // STATE_MACHINE_BS_H

