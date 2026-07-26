# umkacxx

A C++ module wrapper over the [Umka](https://github.com/vtereshkov/umka-lang) C API.

Its job is to make Umka's C types easier to handle from C++ — not to hide them.
It picks the right stack-slot union member from your C++ type, checks argument
counts, and manages the interpreter's lifetime. It does **not** convert C++
containers, and it does **not** manage Umka's reference counts for you.

---

## Contents

1. [Requirements](#requirements)
2. [Quick start](#quick-start)
3. [Two ideas that explain everything](#two-ideas-that-explain-everything)
4. [Vocabulary types](#vocabulary-types)
5. [Creating an interpreter](#creating-an-interpreter)
6. [Calling Umka from C++](#calling-umka-from-c)
7. [Ownership and borrowing](#ownership-and-borrowing)
8. [Calling C++ from Umka](#calling-c-from-umka)
9. [Building Umka values](#building-umka-values)
10. [Errors](#errors)
11. [Type mapping reference](#type-mapping-reference)
12. [Pitfalls](#pitfalls)
13. [API summary](#api-summary)

---

## Requirements

- A C++26 compiler with named-module and `import std` support
- CMake 4.4 or newer
- The Umka C library, found via `find_package(umka REQUIRED)`

```cmake
target_link_libraries(your_target PRIVATE umkacxx::cxx_module)
```

Then in your source:

```cpp
import umka;
```

Everything lives in `namespace umka`. Nothing is private; the wrapper is
transparent by design, because the C API underneath is.

---

## Quick start

`script.um`:

```
fn add(a, b: int): int {
    return a + b
}

fn main() {}
```

`main.cpp`:

```cpp
import std;
import umka;

int main()
{
    umka::vm_t vm{"script.um", 1024 * 1024};

    auto add = vm.function(umka::main_module, "add");
    std::println("{}", add.call<umka::int_t>(40, 2));   // 42
}
```

That's the whole loop: construct a VM, look a function up once, call it.

---

## Two ideas that explain everything

### 1. Everything travels in slots

Umka passes values in `UmkaStackSlot` — an 8-byte union with several names for
the same storage (`intVal`, `uintVal`, `realVal`, `real32Val`, `ptrVal`).
Writing through one name and reading through another gives you garbage, silently.

The wrapper's entire marshalling layer exists to pick the right name from your
C++ type at compile time. That is the value it adds. Everything else is
plumbing.

### 2. The wrapper reinterprets, it never converts

If an operation would need an allocation or a transforming copy, it is your job,
done explicitly. This is why `std::vector`, `std::string`, `std::span` and
`std::array` are **compile errors** as arguments. There is no hidden allocation
and no hidden lifetime.

The one exception is deliberate and C-level: a `const char*` argument is
converted to a real Umka string with `umkaMakeStr`, because handing Umka a bare
C pointer where it expects a reference-counted string is a memory bug, not a
convenience.

---

## Vocabulary types

Scalars mirror Umka's own spelling:

| Alias | Underlying |
| --- | --- |
| `umka::int_t` | `std::int64_t` |
| `umka::uint_t` | `std::uint64_t` |
| `umka::real_t` | `double` |
| `umka::real32_t` | `float` |
| `umka::bool_t` | `bool` |
| `umka::char_t` | `char` |
| `umka::str_t` | `const char*` |
| `umka::ptr_t` | `void*` |

Raw C API types, renamed:

| Alias | Underlying |
| --- | --- |
| `umka::interpreter_handle_t` | `Umka*` |
| `umka::slot_t` | `UmkaStackSlot` |
| `umka::type_t` | `const UmkaType*` |
| `umka::efunc_t` | `UmkaExternFunc` |
| `umka::context_t` | `UmkaFuncContext` |

### `umka::arr_t<T>`

A layout mirror of Umka's dynamic array. Its three members are laid out exactly
as `UmkaDynArray(T)`, which is what lets it be `memcpy`'d in and out of slots:

```cpp
template <typename T> class arr_t
{
    public:
        umka::type_t type;
        umka::int_t  itemsize;
        T           *data;
};
```

It is a **view, not an owner**. It has `len()`, `empty()`, `operator[]`, `at()`,
`front()`, `back()`, and iterators, so range-`for` works:

```cpp
umka::int_t sum = 0;
for (auto value : arr)
{
    sum += value;
}
```

`at()` bounds-checks and terminates on failure; `operator[]` does not check.

Because `arr_t` must stay layout-compatible it holds no interpreter pointer, so
reference counting goes through the VM: `vm.decref(arr)`, not `arr.decref()`.

---

## Creating an interpreter

```cpp
umka::vm_t vm{script_path, stack_size, {module_a, module_b}};
```

- **`script_path`** — a `std::filesystem::path` to the main script.
- **`stack_size`** — counted in **slots, not bytes**. `2 * 1024 * 1024` is 16 MiB.
- **modules** — optional native modules to register (see
  [Calling C++ from Umka](#calling-c-from-umka)).

The constructor does the whole sequence: allocate, initialise, register every
extern function, add every module source, compile, and run `main()`. Any failure
terminates with a diagnostic. There is no way to get the order wrong because you
do not control the order.

`vm_t` is neither copyable nor movable. The destructor frees the interpreter.

Also available:

```cpp
vm.alive();               // bool
vm.mem_usage();           // umka::int_t
umka::vm_t::version();    // umka::str_t, static
```

---

## Calling Umka from C++

### Look up once, call many times

```cpp
auto add = vm.function(umka::main_module, "add");     // in the main script
auto helper = vm.function("./util.um", "helper");     // in a named module
```

Naming a module is **always explicit**. `""` is not a spelling of the main
module — `umka::main_module` is, and an empty path is a hard error.

Do the lookup **once and keep the handle**. Each lookup allocates a parameter
buffer in the interpreter's arena, and that arena is not reclaimed until the VM
is destroyed. Looking up inside a loop is a slow leak.

```cpp
class my_thing
{
    public:
        umka::vm_t vm;
        umka::fn_t configure;   // resolved in the constructor, reused forever

        explicit my_thing(const std::filesystem::path &p)
            : vm{p, 1024 * 1024}, configure{vm.function(umka::main_module, "configure")}
        {
        }
};
```

An `fn_t` must not outlive its `vm_t`; its buffer lives in the VM's arena.
Copying an `fn_t` shares that buffer rather than duplicating it.

### Calling

```cpp
auto n = add.call<umka::int_t>(40, 2);
```

The template argument is the **result** type; it defaults to `void`:

```cpp
accumulate.call(10);          // returns nothing
```

The argument count is checked against the signature. Argument *types* are not —
Umka exposes no type sizes, so a mirror struct with fields in the wrong order
corrupts the stack in silence. This is the one place you have to be careful.

```cpp
add.param_count();      // int
add.param_type(0);      // umka::type_t, needed by make_arr
```

### Struct results

Write a **layout mirror**: a plain struct with the same fields, in the same
order, using wrapper types. It must be trivially copyable.

```
type point = struct {
    name: str
    age: int
}

fn get_point(): point { return point{"Umka", 42} }
```

```cpp
struct point_umka
{
    public:
        umka::str_t name;
        umka::int_t age;
};

auto p = get_point.call<point_umka>();
```

Nesting works, as long as every level is a mirror:

```cpp
struct inner_umka
{
    public:
        umka::str_t name;
        umka::arr_t<umka::int_t> values;
};

struct outer_umka
{
    public:
        umka::int_t id;
        inner_umka items;
};
```

### Fixed arrays

C++ cannot return a bare array by value, so an Umka `[N]T` travels inside a
wrapper struct in both directions:

```cpp
struct five_ints
{
    public:
        umka::int_t items[5];
};

auto arr = get_fixed_arr.call<five_ints>();
sum_fixed_arr.call<umka::int_t>(five_ints{{10, 20, 30, 40, 50}});
```

### Multiple results

Umka's `(int, int)` is a single anonymous struct of `item0`, `item1`, …

```cpp
struct pair_umka
{
    public:
        umka::int_t item0;
        umka::int_t item1;
};

auto p = make_pair.call<pair_umka>(7, 9);
```

### What you may pass as an argument

Accepted: scalars and enums, raw pointers, `umka::str_t`, `umka::arr_t<T>`,
`umka::borrowed_t<…>`, and trivially-copyable mirror structs.

Rejected at compile time, on purpose: `std::string`, `std::string_view`,
`std::vector`, `std::span`, `std::array`.

---

## Ownership and borrowing

**Two rules cover everything.**

### Rule 1: Umka eats what you pass it

Every reference-typed parameter — string, dynamic array, pointer, `any` — is
released when the callee returns. Passing the same value twice reads freed
memory on the second call.

`borrow()` bumps the reference count first, so your copy survives:

```cpp
const umka::str_t owned = vm.make_str("Hello");

str_len.call<umka::int_t>(umka::borrow(owned));
str_len.call<umka::int_t>(umka::borrow(owned));   // still valid

vm.decref(owned);   // your turn to let go
```

`borrow()` applies to `str_t`, `arr_t<T>` and pointer types.

The same applies to a struct passed by value: it carries its fields' references
with it, and the callee releases them. After passing a struct, do not use it
again.

### Rule 2: nothing is released implicitly

What a `call` hands back is yours. Release it when you're done:

```cpp
auto arr = get_int_arr.call<umka::arr_t<umka::int_t>>();
// ... use arr ...
vm.decref(arr);
```

Release the **outermost** object only. Releasing an outer array or struct
releases everything nested inside it; releasing an inner one as well is a
double-release.

---

## Calling C++ from Umka

Three pieces: the C++ function, an Umka declaration promising it exists, and a
`module_t` bundling them.

### 1. The function

Fixed shape — two slot rows in, nothing out:

```cpp
void umka_add(umka::slot_t *params, umka::slot_t *result)
{
    const auto a = umka::get_param<umka::int_t>(params, 0);
    const auto b = umka::get_param<umka::int_t>(params, 1);
    umka::set_result(result, a + b);
}
```

`get_param<T>` reads the right union member for `T`. It also handles fixed
arrays, which arrive as a pointer:

```cpp
auto *items = umka::get_param<umka::int_t[3]>(params, 0);
```

### 2. The declaration

A body-less declaration means "this one lives outside". The `*` exports it.

```
// mylib.um
fn add*(a, b: int): int
```

### 3. The bundle

```cpp
constexpr char src[] = {
#embed "mylib.um"
    , 0};

export umka::module_t mylib{
    "mylib.um",
    src,
    {
        {"add", umka_add},
    }};
```

`#embed` bakes the Umka source into the binary, so the `.um` file never has to
sit next to the executable.

Hand the bundle to the constructor:

```cpp
umka::vm_t vm{"main.um", 1024 * 1024, {mylib}};
```

And use it from Umka. **Module members are reached with `::`, not `.`:**

```
import "mylib.um"

fn main() {
    printf("%d\n", mylib::add(40, 2))
}
```

### Returning values

**Scalars** — two-argument `set_result`:

```cpp
umka::set_result(result, sum);
```

**Structs and arrays** — three-argument `set_result`. Their destination comes
from `umkaGetResult`, not from `result` itself:

```cpp
umka::set_result(params, result, my_struct);
```

Getting this wrong is silent corruption, so the two-argument overload
`static_assert`s against class types.

### Reaching the interpreter

An extern function is handed no interpreter. It is hiding in `result->ptrVal`
on entry:

```cpp
auto *interpreter = umka::instance(result);   // FIRST, before writing to result
```

Read it before you write anything into `result`, because writing overwrites it.

---

## Building Umka values

### Strings

```cpp
umka::str_t s = vm.make_str("hello");                    // from a vm
umka::str_t s = umka::make_str(interpreter, "hello");    // inside an extern
```

A string built this way is real reference-counted VM memory. A plain string
literal is not — which is why passing one as an argument goes through
`umkaMakeStr` automatically.

### Dynamic arrays

`make_arr` needs the Umka `[]T` type so the VM can trace the array's references.
You get it from the function signature:

```cpp
auto sum = vm.function(umka::main_module, "sum_int_arr");

auto values = vm.make_arr<umka::int_t>(sum.param_type(0), 3);
values[0] = 10;
values[1] = 20;
values[2] = 30;

sum.call<umka::int_t>(values);
```

Inside an extern function, the result type comes from `result_type`:

```cpp
void umka_make_arr(umka::slot_t *params, umka::slot_t *result)
{
    auto *interpreter = umka::instance(result);
    const auto n = umka::get_param<umka::int_t>(params, 0);

    auto arr = umka::make_arr<umka::int_t>(interpreter, umka::result_type(params, result), n);
    for (umka::int_t i = 0; i < n; ++i)
    {
        arr[i] = (i + 1) * 10;
    }
    umka::set_result(params, result, arr);
}
```

`make_arr` verifies that Umka's item size matches `sizeof(T)` and terminates on
mismatch — the only type check the wrapper can actually perform.

### Collected memory

To return a `^T` from an extern function, allocate memory the VM owns:

```cpp
auto *p = static_cast<umka::int_t *>(umka::alloc_data(interpreter, sizeof(umka::int_t)));
*p = 77;
umka::set_result(result, p);
```

Handing back a pointer to C++ memory instead means the VM will try to collect
something it does not own.

---

## Errors

Every failure prints to `stderr` and calls `std::terminate()`. Nothing throws.

```
umka: compile error: Unknown identifier fn_wrap (main.um:7 in run_add)
```

This covers: allocation, init, a rejected extern function or module, compile
failure, a runtime error inside a call, a function not found, an argument-count
mismatch, an `at()` out of range, and a `make_arr` item-size mismatch.

There is no error channel and no recovery. Failures are for you to fix, not to
handle, and the diagnostic names the file, line and Umka function.

---

## Type mapping reference

| Umka | C++ | Notes |
| --- | --- | --- |
| `int` | `umka::int_t` | |
| `uint` | `umka::uint_t` | |
| `real` | `umka::real_t` | |
| `real32` | `umka::real32_t` | see pitfalls |
| `bool` | `umka::bool_t` | |
| `char` | `umka::char_t` | |
| `str` | `umka::str_t` | `const char*`; must be VM-owned |
| `^T` | `T*` | |
| `[]T` | `umka::arr_t<T>` | |
| `[N]T` | `struct { T items[N]; }` | wrapper struct both ways |
| `struct {…}` | mirror struct | same fields, same order |
| `enum` | `enum class E : umka::int_t` | |
| `(A, B)` | `struct { A item0; B item1; }` | |

---

## Pitfalls

**Mirror struct field order is unchecked.** Get it wrong and the stack is
corrupted in silence. This is the single most dangerous thing in the library.

**`real32` uses different union members in each direction.** Going in it is
`real32Val`; coming out it is `realVal`. The wrapper handles this, but if you
drop to the raw C API mid-way, it will bite.

**Read `instance(result)` before writing to `result`.** Writing overwrites the
interpreter pointer you were about to need.

**Structured results need the three-argument `set_result`.** The two-argument
form writes to the wrong place for anything larger than a slot.

**Do not look up functions in a loop.** Each lookup permanently consumes arena
memory.

**An `fn_t` must not outlive its `vm_t`.** Its parameter buffer is arena memory.

**Module members use `::` in Umka**, even though struct fields and enum
constants use `.`: `mylib::point{...}`, `mylib::Color.blue`.

**Register everything before compiling.** The `vm_t` constructor does this for
you; if you reach past it into the C API, order matters.

---

## API summary

### `umka::vm_t`

| Member | Purpose |
| --- | --- |
| `vm_t(path, stack_size, modules = {})` | allocate, init, register, compile, run |
| `function(main_module, name)` | look up in the main script |
| `function(module, name)` | look up in a named module |
| `lookup(module_or_null, name)` | raw form used by both overloads |
| `make_str(s)` | build a VM-owned string |
| `make_arr<T>(type, len)` | build a VM-owned dynamic array |
| `alloc_data(size, on_free = nullptr)` | collected memory for a `^T` |
| `incref(ptr)` / `incref(arr)` | retain |
| `decref(ptr)` / `decref(arr)` | release |
| `alive()` / `mem_usage()` / `version()` | interpreter state |

### `umka::fn_t`

| Member | Purpose |
| --- | --- |
| `call<R = void>(args...)` | fill parameters, invoke, return the result |
| `param_count()` | declared parameter count |
| `param_type(index)` | Umka type of a parameter, for `make_arr` |
| `invoke()` | run with parameters already set |

### Free functions

| Function | Purpose |
| --- | --- |
| `get_param<T>(params, index)` | read an argument in an extern function |
| `set_result(result, val)` | scalar result |
| `set_result(params, result, val)` | structured result |
| `set_param<T>(interpreter, params, index, val)` | write an argument directly |
| `instance(result)` | recover the interpreter inside an extern |
| `param_type(params, index)` | Umka type of a parameter |
| `result_type(params, result)` | Umka type of the result |
| `make_str(interpreter, s)` | build a string without a `vm_t` |
| `make_arr<T>(interpreter, type, len)` | build an array without a `vm_t` |
| `alloc_data(interpreter, size, on_free)` | collected memory |
| `incref` / `decref(interpreter, ptr)` | retain / release |
| `borrow(value)` | retain a value across one call |

### Types

`str_t` `int_t` `uint_t` `real_t` `real32_t` `bool_t` `char_t` `ptr_t`
`interpreter_handle_t` `slot_t` `type_t` `efunc_t` `context_t`
`arr_t<T>` `borrowed_t<T>` `func_t` `module_t` `fn_t` `vm_t`
`main_module_t` / `main_module`
