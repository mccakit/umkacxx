module;
#include <umka_api.h>
export module umkacxx:core;
import std;
export namespace umkacxx
{
    /// Shared ownership handle to the Umka VM.
    using vm_handle = std::shared_ptr<Umka>;

    /// Decrements the reference count of an Umka-managed heap pointer.
    /// Call this on the `.data` pointer of any `umka_dynarray` you received
    /// directly from the VM after you are done copying its contents.
    /// Do NOT call this on nested arrays inside an outer array — Umka frees
    /// those automatically when the outer array is decreffed.
    auto umka_decref(const vm_handle vm, void *ptr) -> void
    {
        umkaDecRef(vm.get(), ptr);
    }

    /// Mirrors Umka's internal dynamic array layout.
    /// T must match the element type of the corresponding Umka []T slice.
    /// Use `.len()` to get the number of elements and `.data` to iterate.
    template <typename T> struct umka_dynarray
    {
        public:
            const UmkaType *type;
            int64_t itemsize;
            T *data;

            auto len() -> const int64_t
            {
                return umkaGetDynArrayLen(this);
            }
    };

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
            std::shared_ptr<Umka> umka_vm;

            /// Constructs and runs the VM from the given Umka script path.
            /// Terminates on compile or runtime error.
            umka(const std::filesystem::path &main_script) : umka_vm{umkaAlloc(), umkaFree}
            {
                umkaInit(umka_vm.get(), main_script.c_str(), nullptr, 4096, nullptr, 0, nullptr, true, true, nullptr);
                if (!umkaCompile(umka_vm.get()))
                {
                    std::println("umkacxx compile error: {}", umkaGetError(umka_vm.get())->msg);
                    std::terminate();
                }
                if (umkaRun(umka_vm.get()) != 0)
                {
                    std::println("umkacxx runtime error: {}", umkaGetError(umka_vm.get())->msg);
                    std::terminate();
                }
            }

            template <typename T, typename U> auto call(std::string_view func_name) -> U
            {
                UmkaFuncContext fn{};
                umkaGetFunc(umka_vm.get(), nullptr, func_name.data(), &fn);
                if constexpr (std::is_arithmetic_v<T> || std::is_pointer_v<T>)
                {
                    UmkaStackSlot result_slot{};
                    fn.result = &result_slot;
                    umkaCall(umka_vm.get(), &fn);
                    if constexpr (std::is_pointer_v<T>)
                    {
                        return U{reinterpret_cast<T>(result_slot.ptrVal)};
                    }
                    else if constexpr (std::is_floating_point_v<T>)
                    {
                        return U{static_cast<U>(result_slot.realVal)};
                    }
                    else
                    {
                        return U{static_cast<U>(result_slot.intVal)};
                    }
                }
                else
                {
                    T result_storage{};
                    UmkaStackSlot result_slot{.ptrVal = &result_storage};
                    fn.result = &result_slot;
                    umkaCall(umka_vm.get(), &fn);
                    return U{umka_vm, *static_cast<T *>(result_slot.ptrVal)};
                }
            }
    };
} // namespace umkacxx
