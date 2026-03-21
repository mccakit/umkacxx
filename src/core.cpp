module;
#include <umka_api.h>
export module umkacxx:core;
import std;
export namespace umkacxx
{
    using vm_handle = std::shared_ptr<Umka>;
    template <typename T> struct umka_dynarray_raw
    {
        public:
            const UmkaType *type;
            int64_t itemsize;
            T *data;
    };
    // RAII wrapper — populated after reading from VM
    template <typename T> class umka_dynarray
    {
        public:
            vm_handle vm;
            T *data;
            int length;
            umka_dynarray(const umka_dynarray_raw<T> &raw, vm_handle vm)
                : vm{vm}, data{raw.data}, length{umkaGetDynArrayLen(&raw)}
            {
            }
            ~umka_dynarray()
            {
                umkaDecRef(vm.get(), data);
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

    class umka
    {
        public:
            std::shared_ptr<Umka> umka_vm;

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

            //Return is scalar
            template <typename T>
            auto call_scalar(std::string_view func_name) -> T
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

            //Return type is a struct with scalar fields
            template <typename T>
            auto call_struct(std::string_view func_name) -> T
            {
                UmkaFuncContext fn{};
                umkaGetFunc(umka_vm.get(), nullptr, func_name.data(), &fn);
                T result_storage{};
                UmkaStackSlot result_slot{.ptrVal = &result_storage};
                fn.result = &result_slot;
                umkaCall(umka_vm.get(), &fn);
                return *static_cast<T *>(result_slot.ptrVal);
            }

            //Return type is a struct with arr fields
            template <typename T, typename U>
            auto call_struct(std::string_view func_name) -> U
            {
                UmkaFuncContext fn{};
                umkaGetFunc(umka_vm.get(), nullptr, func_name.data(), &fn);
                T result_storage{};
                UmkaStackSlot result_slot{.ptrVal = &result_storage};
                fn.result = &result_slot;
                umkaCall(umka_vm.get(), &fn);
                return U{*static_cast<T *>(result_slot.ptrVal), umka_vm};
            }

            //Return type is an array
            template <typename T> auto call_arr(std::string_view func_name) -> umka_dynarray<T>
            {
                UmkaFuncContext fn{};
                umkaGetFunc(umka_vm.get(), nullptr, func_name.data(), &fn);
                umka_dynarray_raw<T> result_storage{};
                UmkaStackSlot result_slot{.ptrVal = &result_storage};
                fn.result = &result_slot;
                umkaCall(umka_vm.get(), &fn);
                return umka_dynarray<T>{*static_cast<umka_dynarray_raw<T> *>(result_slot.ptrVal), umka_vm};
            }
    };
} // namespace umkacxx
