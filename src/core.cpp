module;
#include <umka_api.h>
export module umkacxx:core;
import std;
export namespace umkacxx
{
    /// Shared ownership handle to the Umka VM.
    using vm_handle = std::shared_ptr<Umka>;

    /// Mirrors Umka's internal dynamic array layout. Use as raw type in `umka::call`.
    template <typename T> struct umka_dynarray_raw
    {
        public:
            const UmkaType *type;
            int64_t itemsize;
            T *data;
    };

    /// Owning C++ wrapper for a Umka dynamic array. Copies data out of Umka's GC heap on construction.
    template <typename T> class umka_dynarray
    {
        public:
            vm_handle vm;
            std::vector<T> data;

            /// Copies elements from a same-type raw array and decrefs the raw pointer.
            umka_dynarray(const umka_dynarray_raw<T> &raw, vm_handle vm) : vm{vm}
            {
                int len = umkaGetDynArrayLen(&raw);
                data.reserve(len);
                for (int i = 0; i < len; i++)
                {
                    data.push_back(raw.data[i]);
                }
                umkaDecRef(vm.get(), raw.data);
            }

            /// Converts elements from a different raw type via `T(raw_elem, vm)`.
            template <typename RawT> umka_dynarray(const umka_dynarray_raw<RawT> &raw, vm_handle vm) : vm{vm}
            {
                int len = umkaGetDynArrayLen(&raw);
                data.reserve(len);
                for (int i = 0; i < len; i++)
                {
                    data.emplace_back(raw.data[i], vm);
                }
            }
    };

    /// Umka source module. `src` must be null-terminated.
    /// @tparam N Deduced from the `std::array` size via CTAD.
    template <std::size_t N> struct umka_module
    {
        public:
            std::string name{};
            std::array<char, N> src{};
    };

    template <std::size_t N> umka_module(std::string, std::array<char, N>) -> umka_module<N>;

    /// Manages an Umka VM and provides a typed function call interface.
    class umka
    {
        public:
            std::shared_ptr<Umka> umka_vm;

            /// Constructs the VM, registers modules, compiles and runs the main script.
            template <std::size_t... Ns>
            umka(const std::filesystem::path &main_script, umka_module<Ns> &&...modules)
                : umka_vm{umkaAlloc(), umkaFree}
            {
                umkaInit(umka_vm.get(), main_script.c_str(), nullptr, 4096, nullptr, 0, nullptr, true, true, nullptr);
                (umkaAddModule(umka_vm.get(), modules.name.c_str(), modules.src.data()), ...);
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

            /// Constructs the VM with no additional modules.
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

            /// `umkaFree` called automatically via shared_ptr deleter.
            ~umka() = default;

            /// Calls an Umka function.
            /// @tparam T Raw return type — scalar or struct mirroring Umka's memory layout.
            /// @tparam U Result type — constructed as `U{raw, vm}` for structs, `U{value}` for scalars.
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
                    return U{*static_cast<T *>(result_slot.ptrVal), umka_vm};
                }
            }
    };
} // namespace umkacxx
