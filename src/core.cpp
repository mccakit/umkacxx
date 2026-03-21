module;
#include <umka_api.h>
export module umkacxx:core;
import std;
export namespace umkacxx
{
    /// @brief Shared ownership handle to the Umka VM instance.
    using vm_handle = std::shared_ptr<Umka>;

    /// @brief Mirrors Umka's internal dynamic array layout in memory.
    /// @tparam T Element type — must match the Umka-side array element type exactly.
    /// @note Do not construct manually. Use as the raw type parameter in `umka::call`.
    template <typename T> struct umka_dynarray_raw
    {
        public:
            const UmkaType *type;
            int64_t itemsize;
            T *data;
    };

    /// @brief Owning C++ wrapper for a Umka dynamic array.
    /// @tparam T Element type.
    /// @note Copies data out of Umka's GC heap on construction and decrefs the raw pointer.
    ///       After construction the VM may die freely — data is owned by the vector.
    template <typename T> class umka_dynarray
    {
        public:
            using element_type = T;

            /// @brief Copied elements from the Umka array.
            std::vector<T> data;

            /// @param raw Raw Umka dynarray to copy from.
            /// @param vm  VM handle used to decref the raw data pointer after copying.
            umka_dynarray(const umka_dynarray_raw<T> &raw, vm_handle vm)
            {
                int len = umkaGetDynArrayLen(&raw);
                data.reserve(len);
                for (int i = 0; i < len; i++)
                    data.push_back(raw.data[i]);
                umkaDecRef(vm.get(), raw.data);
            }
    };

    /// @brief Represents an Umka source module to be registered with the VM.
    /// @tparam N Size of the source array, deduced automatically via CTAD.
    /// @note `src` must contain a null terminator as the last element.
    template <std::size_t N> struct umka_module
    {
            std::string name{};
            std::array<char, N> src{};
    };

    /// @brief CTAD deduction guide for umka_module.
    /// @note Deduces `N` from the size of the provided `std::array`.
    template <std::size_t N> umka_module(std::string, std::array<char, N>) -> umka_module<N>;

    /// @brief Matches Umka scalar types: arithmetic types and pointers.
    /// @note Used to select the correct `call` overload for primitive return types.
    template <typename T>
    concept umka_scalar = std::is_arithmetic_v<T> || std::is_pointer_v<T>;

    /// @brief Manages an Umka VM instance and provides a typed call interface.
    class umka
    {
        public:
            /// @brief Shared handle to the underlying Umka VM.
            std::shared_ptr<Umka> umka_vm;

            /// @brief Constructs the VM, registers modules, compiles and runs the main script.
            /// @param main_script Path to the main Umka script.
            /// @param modules     Variadic list of additional modules to register.
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

            /// @brief Constructs the VM with no additional modules.
            /// @param main_script Path to the main Umka script.
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

            /// @brief Destructor. Calls `umkaFree` automatically via the shared_ptr deleter.
            ~umka() = default;

            /// @brief Calls an Umka function returning a scalar value.
            /// @tparam T Return type — must satisfy `umka_scalar`.
            /// @param func_name Name of the Umka function to call.
            /// @return The scalar return value.
            template <typename T>
                requires umka_scalar<T>
            auto call(std::string_view func_name) -> T
            {
                UmkaFuncContext fn{};
                umkaGetFunc(umka_vm.get(), nullptr, func_name.data(), &fn);
                UmkaStackSlot result_slot{};
                fn.result = &result_slot;
                umkaCall(umka_vm.get(), &fn);
                if constexpr (std::is_pointer_v<T>)
                {
                    return reinterpret_cast<T>(result_slot.ptrVal);
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

            /// @brief Calls an Umka function returning a struct, optionally converting to a RAII type.
            /// @tparam T Raw type mirroring Umka's memory layout — must not satisfy `umka_scalar`.
            /// @tparam U RAII result type. Must be constructible as `U(const T&, vm_handle)`.
            ///           If `T == U`, acts as a plain struct copy.
            /// @param func_name Name of the Umka function to call.
            /// @return Instance of `U` constructed from the raw result.
            template <typename T, typename U>
                requires(!umka_scalar<T>)
            auto call(std::string_view func_name) -> U
            {
                UmkaFuncContext fn{};
                umkaGetFunc(umka_vm.get(), nullptr, func_name.data(), &fn);
                T result_storage{};
                UmkaStackSlot result_slot{.ptrVal = &result_storage};
                fn.result = &result_slot;
                umkaCall(umka_vm.get(), &fn);
                return U{*static_cast<T *>(result_slot.ptrVal), umka_vm};
            }
    };
} // namespace umkacxx
