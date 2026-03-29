module;
#include <initializer_list>
#include <string>
#include <umka_api.h>
export module umkacxx:core;
import std;
import :types;
export namespace umkacxx
{
    template <typename T> auto get_param(types::umka_slot *params, int index) -> T
    {
        auto *slot = umkaGetParam(params, index);
        if constexpr (std::is_pointer_v<T>)
        {
            return static_cast<T>(slot->ptrVal);
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            return static_cast<T>(slot->realVal);
        }
        else
        {
            return static_cast<T>(slot->intVal);
        }
    }
    template <typename T> auto set_result(types::umka_slot *result, T val) -> void
    {
        if constexpr (std::is_pointer_v<T>)
        {
            result->ptrVal = static_cast<void *>(val);
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            result->realVal = static_cast<double>(val);
        }
        else
        {
            result->intVal = static_cast<int64_t>(val);
        }
    }
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
            umka(const std::filesystem::path &main_script,
                 std::size_t stack_size,
                 std::initializer_list<types::module_t> modules = {})
                : vm{umkaAlloc(), umkaFree}
            {
                umkaInit(vm.get(), main_script.c_str(), nullptr, stack_size, nullptr, 0, nullptr, true, true, nullptr);
                for (auto &mod : modules)
                {
                    for (auto &fn : mod.funcs)
                    {
                        umkaAddFunc(vm.get(), fn.name.c_str(), fn.fn);
                    }
                    umkaAddModule(vm.get(), mod.name.c_str(), mod.src.c_str());
                }
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
            auto make_str(const char *s) const -> const char *
            {
                return umkaMakeStr(vm.get(), s);
            }
            template <typename T>
            auto call(std::string_view func_name,
                      std::string_view module_name = {},
                      std::initializer_list<types::slot_t> params = {}) const -> T
            {
                UmkaFuncContext fn{};
                if (!umkaGetFunc(vm.get(), module_name.empty() ? nullptr : module_name.data(), func_name.data(), &fn))
                {
                    std::println("Error: Could not find function '{}' in module '{}'", func_name, module_name);
                    std::terminate();
                }

                int i = 0;
                for (auto const &p : params)
                {
                    UmkaStackSlot *slot = umkaGetParam(fn.params, i++);
                    if (!slot)
                    {
                        break;
                    }
                    std::visit(
                        [slot](auto &&val) {
                            using V = std::decay_t<decltype(val)>;
                            if constexpr (std::is_same_v<V, int64_t>)
                            {
                                slot->intVal = val;
                            }
                            else if constexpr (std::is_same_v<V, uint64_t>)
                            {
                                slot->uintVal = val;
                            }
                            else if constexpr (std::is_same_v<V, void *>)
                            {
                                slot->ptrVal = val;
                            }
                            else if constexpr (std::is_same_v<V, double>)
                            {
                                slot->realVal = val;
                            }
                            else if constexpr (std::is_same_v<V, float>)
                            {
                                slot->real32Val = val;
                            }
                            else if constexpr (std::is_same_v<V, const char *>)
                            {
                                slot->ptrVal = const_cast<char *>(val);
                            }
                        },
                        p);
                }

                if constexpr (std::is_void_v<T>)
                {
                    umkaCall(vm.get(), &fn);
                }
                else if constexpr (std::is_arithmetic_v<T> || std::is_pointer_v<T>)
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
