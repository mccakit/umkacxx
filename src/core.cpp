module;
#include <umka_api.h>
export module umkacxx:core;
import std;
import :types;
export namespace umkacxx
{
    /// Manages an Umka VM and provides a typed function call interface.
    ///
    /// ## Calling Umka functions
    /// Use `call<T, U>(name)` where:
    ///   - T is the raw Umka-side return type (must match Umka's memory layout)
    ///   - U is the C++ wrapper type to construct from the result
    ///
    /// U must be constructible as:
    ///   - U{vm_handle, T}  for struct/dynarray returns
    ///   - U{T}             for arithmetic/pointer returns
    ///
    /// ## Memory management
    /// Umka heap objects (dynamic arrays) are reference counted.
    /// When your U constructor receives a `umka_dynarray<T>`, copy its
    /// contents into stdlib types and call `umka_decref(vm, arr.data)` on
    /// the array's `.data` pointer. Only decref arrays you received directly
    /// from the VM — nested arrays inside a struct are freed by Umka when
    /// the outer array is decreffed.
    class umka
    {
        public:
            types::vm_handle vm;

            /// Constructs and runs the VM from the given Umka script path.
            /// Terminates on compile or runtime error.
            umka(const std::filesystem::path &main_script) : vm{umkaAlloc(), umkaFree}
            {
                umkaInit(vm.get(), main_script.c_str(), nullptr, 4096, nullptr, 0, nullptr, true, true, nullptr);
                if (!umkaCompile(vm.get()))
                {
                    std::println("umkacxx compile error: {}", umkaGetError(vm.get())->msg);
                    std::terminate();
                }
                if (umkaRun(vm.get()) != 0)
                {
                    std::println("umkacxx runtime error: {}", umkaGetError(vm.get())->msg);
                    std::terminate();
                }
            }

            template <typename T> auto call(std::string_view func_name) -> T
            {
                UmkaFuncContext fn{};
                umkaGetFunc(vm.get(), nullptr, func_name.data(), &fn);
                if constexpr (std::is_arithmetic_v<T> || std::is_pointer_v<T>)
                {
                    UmkaStackSlot result_slot{};
                    fn.result = &result_slot;
                    umkaCall(vm.get(), &fn);
                    if constexpr (std::is_pointer_v<T>)
                    {
                        return static_cast<T>(result_slot.ptrVal);
                    }
                    else if constexpr (std::is_floating_point_v<T>)
                    {
                        return static_cast<T>(result_slot.realVal);
                    }
                    else
                    {
                        return static_cast<T>(result_slot.intVal);
                    }
                }
                else
                {
                    T result_storage{};
                    UmkaStackSlot result_slot{.ptrVal = &result_storage};
                    fn.result = &result_slot;
                    umkaCall(vm.get(), &fn);
                    return *static_cast<T *>(result_slot.ptrVal);
                }
            }
    };
} // namespace umkacxx
