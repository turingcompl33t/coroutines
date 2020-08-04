## C++ Coroutines

### The `awaitable` / `awaiter` Interface

The `awaitable` interface controls the semantics of the `co_await` expression. That is, the `awaitable` interface specifies a collection of methods that are invoked on an object when it is `co_await`ed that control:

- Whether or not object that is being `co_await`ed is already ready and thus may proceed synchronously and the coroutine need not suspend at all (`await_ready()`)
- Logic that will be executed after the coroutine suspends but before it returns control to its caller, allowing the coroutine to be scheduled for resumption at some point in the future (`await_suspend()`)
- Logic that will be executed when the coroutine is resumed that produces the result of the `co_await` statement (`await_resume()`)

**The Interface**

The core interface for the `awaiter` that (eventually, see below) appears to the right of a `co_await` is the following:

- `bool await_ready()`
- One of the following flavors of `await_suspend()`:
    - `void await_suspend(std::coroutine_handle<>)`
    - `void await_suspend(std::coroutine_handle<P>)`
    - `bool await_suspend(std::coroutine_handle<>)`
    - `bool await_suspend(std::coroutine_handle<P>)`
- `void await_resume()`

**Aside: Normally Awaitable vs Contextually Awaitable Types**

The terminolofy _normally awaitable_ vs _contextually awaitable_ originates with Lewis Baker (as far as I can tell).

The distinction here is subtle: any type that supports operator `co_await` is an _awaitable type_. However, the `promise` interface supports an `await_transform()` method that influences the way in which expressions that appear to the right of operator `co_await` are evaluated - the promise type for the coroutine can interpose (in a way) and transform some arbitrary type into something that is `await`able. 

Therefore, its helpful to have the distinction between types that require this extra level of transformation (_contextually awaitable_ types) and the ones that do no (_normally awaitable_ types).

**Resolving the Awaiter Type**

The process that the compiler goes through in order to resolve a statement like `co_await <expr>` proceeds roughly as follows:

- If the promise type for the coroutine in which `co_await` appears defines an `await_transform()` then `<expr>` is passed to `await_transform()` in order to obtain the `awaitable` object, otherwise, the result of evaluating `<expr>` is used directly as the `awaitable` object.
- Next, the `awaiter` for the `awaitable` object is obtained: if the `awaitable` type implements `operator co_await()`, then the result of invoking this method is used to produce the `awaiter`, otherwise, the `awaitable` itself is used as the `awaiter`.

**`co_await <expr>`: The Full Picture**

The rough outline of the algorithm that the compiler generates when it encounters `co_await <expr>` is as follows:

- Evaluate `<expr>`
- Resolve the `awaitable` object from `<expr>`
- Resolve the `awaiter` object from the `awaitable` object
- Invoke `await_ready()`; if the method returns `true`, then skip the following steps and proceed directly to returning the result of `await_resume()`, otherwise, proceed
- Suspend the coroutine
- Invoke the `awaiter`'s `await_suspend()` method; there are two versions of this method, one that returns `void` and one that returns `bool`; as one might suspect, the `void await_suspend()` method unconditionally suspends the coroutine, while `bool await_suspend()` allows some logic to be implemented after the coroutine is already suspended, but may return `false` to resume the coroutine immediately without returning control to the caller
- Return control to the caller
- Coroutine is resumed
- Resolve the final result of `co_await <expr>` to the result of invoking `await_resume()` on the `awaiter`

### The `promise` Interface

The promise type associated with a coroutine controls various aspects of the execution of the coroutine itself. In his blog article [Understanding the Promise Type](https://lewissbaker.github.io/2018/09/05/understanding-the-promise-type), Lewis Baker describes the promise type for a coroutine as a sort of "coroutine state controller", and this description is the one that finally clarified the role of the promise type for me.

Essentially, the promise type associated with a coroutine allows a coroutine author to hook into the execution of the coroutine at various well-defined points to modify the coroutines execution.

The promise type for a coroutine is constructed automatically directly after the coroutine frame for the coroutine is constructed.

**`get_return_object()`**

This method is invoked on the `promise` type after initial construction of the coroutine frame in order to retrieve the object that is returned to the called of the coroutine. The result of `get_return_object()` is stored in the coroutine frame. 

The actual type returned by `get_return_object()` may differ from the type returned by the coroutine itself. For instance, consider a coroutine that returns an `int` that is computed asynchronously and denotes this by returning a `task<int>` type:

```
task<int> async_compute()
{
    // do work
} 
```

The body of `get_return_object()` for `task::promise_type` might be:

```
task get_return_object()
{
    return std::coroutine_handle<promise_type>::from_promise(*this);
}
```

Ostensibly, this function is not returning a `task` type but rather a `coroutine_handle` specialized for the `promise_type` defined for `task`, but this is fine because an implicit conversion to the return type is performed. Thus, all that is needed to make the above definitions work is a constructor for the task type that looks like:

```
task(std::coroutine_handle<promise_type> handle)
{
    // ...
}
```

**`initial_suspend()`**

This method is invoked after `get_return_object()`, before the statements within the body of the coroutine (what we write as an application-level) programmer are executed. The result of this method call is awaited upon via operator `co_await`. Thus, this hook allows us to suspend execution of the coroutine before any of the code within the coroutine's body has executed.

As an example, if one were authoring a promise type for a coroutine for which there was no reason to suspend execution of the coroutine before any of the body was executed, one might implement `initial_suspend()` as follows:

```
auto initial_suspend()
{
    return std::suspend_never{};
}
```

`suspend_never{}` is one of the trivial types provided by the standard library in accordance with the coroutines TS that simply returns `true` from its `await_ready()` method, and thus never suspends execution of the coroutine. If, on the other hand, one wanted to design a promise type that always yielded control back to the caller before executing the body of the coroutine, one could provide an `initial_suspend()` implementation similar to:

```
auto initial_suspend()
{
    return std::suspend_always{};
}
```

The logic here is analogous to that provided above for `std::suspend_never{}`.

The result of the `initial_suspend()` call is discarded, so whatever `awaiter` is used internally in the promise type's `initial_suspend()` should generally return `void`.

In general, a promise type that suspends execution at `initial_suspend()` may be characterized as a "lazy" coroutine whereas a promise type that does not may be referred to as an "eager" coroutine.

**`yield_value()`**

An expression of the form `co_yield <expr>` is translated to `co_await promise.yield_value(<expr>)`. 

**`return_void()`**

The `return_void()` method of the coroutine's promise type is invoked when a `co_return` statment is encountered without any expression following the statement, or in the event that control flow reaches the end of the body of a coroutine without encountering a `co_return` statement.

**`return_value()`**

The `return_value()` method of the coroutine's promise type is invoked when a statement of the form `co_return <expr>` is encountered.  

**`unhandled_exception()`**

The `unhandled_exception()` method of the coroutine's promise type is invoked in the event that an exception propogates out of the body of the coroutine. Non-robust low-level coroutine implementations (such as many of the implementations in this repository) punt on the topic of exception handling by implementing `unhandled_exception()` in a manner similar to the following:

```
void unhandled_exception()
{
    std::terminate()
}
```

Obviously, production-quality code this is far from an acceptable method for handling exceptions that propogate out of the body of the coroutine. A more robust solution for exception handling with coroutines is to save the exception in the promise type for the coroutine and rethrow the exception when the faulting coroutine is `co_await`ed upon:

```
// class promise<T>

void unhandled_exception() noexcept
{
    // Store the exception pointer for unhandled exception in the promise object.
    ::new (static_cast<void*>(std::addressof(exception))) 
        std::exception_ptr{std::current_exception()};
}

T& result()
{
    if (ResultType::exception == result_type)
    {
        std::rethrow_exception(exception);
    }

    // ...
}
```

**`final_suspend()`**