module;
#include <umka/umka_api.h>
export module umka;
import std;
export namespace umka
{
    using str_t = const char *;
    using int_t = std::int64_t;
    using uint_t = std::uint64_t;
    using real_t = double;
    using real32_t = float;
    using bool_t = bool;
    using char_t = char;
    using ptr_t = void *;

    using interpreter_handle_t = Umka *;
    interpreter_handle_t interpreter;
    using slot_t = UmkaStackSlot;
    using efunc = UmkaExternFunc;
    struct func_t
    {
        public:
            std::string name;
            efunc extern_fn;
            func_t(std::string name, efunc fn) : name{name}, extern_fn{fn}
            {
            }
    };
    class module_t
    {
        public:
            std::string name{};
            std::string source{};
            std::vector<func_t> functions{};
            module_t(std::string name, std::string source, std::vector<func_t> functions = {})
                : name{name}, source{source}, functions{functions}
            {
            }
    };

    template <typename T> class arr_t
    {
        public:
            const UmkaType *type;
            int64_t itemsize;
            T *data;

            auto len() -> const int64_t
            {
                return umkaGetDynArrayLen(this);
            }

            auto decref() -> void
            {
                umkaDecRef(interpreter, data);
            }
            auto begin()
            {
                return data;
            }
            auto end()
            {
                return data + len();
            }
            auto begin() const
            {
                return data;
            }
            auto end() const
            {
                return data + len();
            }
            auto cbegin() const
            {
                return data;
            }
            auto cend() const
            {
                return data + len();
            }
            auto operator[](const int64_t index) noexcept -> T &
            {
                return data[index];
            }

            auto operator[](const int64_t index) const noexcept -> const T &
            {
                return data[index];
            }

            auto at(const int64_t index) -> T &
            {
                if (index < 0 || index >= len())
                    throw std::out_of_range("arr_t index out of range");

                return data[index];
            }

            auto at(const int64_t index) const -> const T &
            {
                if (index < 0 || index >= len())
                    throw std::out_of_range("arr_t index out of range");

                return data[index];
            }

            auto front() noexcept -> T &
            {
                return data[0];
            }

            auto front() const noexcept -> const T &
            {
                return data[0];
            }

            auto back() noexcept -> T &
            {
                return data[len() - 1];
            }

            auto back() const noexcept -> const T &
            {
                return data[len() - 1];
            }
    };

    class umka
    {
        public:
            umka(std::filesystem::path script, std::size_t stack_size, std::initializer_list<module_t> modules = {})
            {
                interpreter = umkaAlloc();
                umkaInit(interpreter,
                         script.string().c_str(),
                         nullptr,
                         stack_size,
                         nullptr,
                         0,
                         nullptr,
                         true,
                         true,
                         nullptr);
                for (auto &module : modules)
                {
                    for (auto &function : module.functions)
                    {
                        umkaAddFunc(interpreter, function.name.c_str(), function.extern_fn);
                    }
                    umkaAddModule(interpreter, module.name.c_str(), module.source.c_str());
                }
                if (!umkaCompile(interpreter))
                {
                    std::println("umkacxx compile error: {}", umkaGetError(interpreter)->msg);
                    std::terminate();
                }
                if (umkaRun(interpreter) != 0)
                {
                    std::println("umkacxx runtime error: {}", umkaGetError(interpreter)->msg);
                    std::terminate();
                }
            }
            template <typename T> auto call(std::string module, std::string function) -> T
            {

                UmkaFuncContext fn;
                if (!umkaGetFunc(interpreter, module.c_str(), function.c_str(), &fn))
                {
                    std::println("Error: Could not find function '{}' in module '{}'", function, module);
                    std::terminate();
                }
                if constexpr (std::is_aggregate_v<T>)
                {
                    T result;
                    fn.result->ptrVal = &result;
                    int errCode = umkaCall(interpreter, &fn);
                    if (errCode != 0)
                    {
                        auto *error = umkaGetError(interpreter);
                        std::println("Runtime error: {}", error->msg);
                        std::terminate();
                    }
                    return result;
                }
                else
                {
                    int errCode = umkaCall(interpreter, &fn);
                    if (errCode != 0)
                    {
                        auto *error = umkaGetError(interpreter);
                        std::println("Runtime error: {}", error->msg);
                        std::terminate();
                    }
                    if constexpr (std::is_same_v<T, int_t> or std::is_same_v<T, char_t> or std::is_same_v<T, bool_t> or
                                  std::is_enum_v<T>)
                    {
                        return static_cast<T>(fn.result->intVal);
                    }
                    else if constexpr (std::is_same_v<T, uint_t>)
                    {
                        return fn.result->uintVal;
                    }
                    else if constexpr (std::is_pointer_v<T> or std::is_same_v<T, ptr_t> or std::is_same_v<T, str_t>)
                    {
                        return reinterpret_cast<T>(fn.result->ptrVal);
                    }
                    else if constexpr (std::is_same_v<T, real_t>)
                    {
                        return fn.result->realVal;
                    }
                    else if constexpr (std::is_same_v<T, real32_t>)
                    {
                        return static_cast<real32_t>(fn.result->realVal);
                    }
                }
            }
    };

    template <typename T> auto get_param(slot_t *params, int index)
    {
        auto *slot = umkaGetParam(params, index);
        if constexpr (std::is_array_v<T>)
        {
            using item_t = std::remove_extent_t<T>;
            return reinterpret_cast<item_t *>(slot);
        }
        else if constexpr (std::is_aggregate_v<T>)
        {
            return *reinterpret_cast<T *>(slot);
        }
        else if constexpr (std::is_pointer_v<T> or std::is_same_v<T, ptr_t> or std::is_same_v<T, str_t>)
        {
            return static_cast<T>(slot->ptrVal);
        }
        else if constexpr (std::is_same_v<T, real_t>)
        {
            return static_cast<T>(slot->realVal);
        }
        else if constexpr (std::is_same_v<T, real32_t>)
        {
            return static_cast<real32_t>(slot->realVal);
        }
        else if constexpr (std::is_same_v<T, int_t> or std::is_same_v<T, char_t> or std::is_same_v<T, bool_t> or
                           std::is_enum_v<T>)
        {
            return static_cast<T>(slot->intVal);
        }
        else if constexpr (std::is_same_v<T, uint_t>)
        {
            return static_cast<uint_t>(slot->uintVal);
        }
        else
        {
            std::println("unsupported type");
            std::terminate();
        }
    }

    template <typename T> auto set_result(slot_t *result, T val) -> void
    {
        if constexpr (std::is_aggregate_v<T>)
        {
            *static_cast<T *>(result->ptrVal) = val;
        }
        else if constexpr (std::is_same_v<T, str_t>)
        {
            result->ptrVal = umkaMakeStr(interpreter, val);
        }
        else if constexpr (std::is_pointer_v<T> or std::is_same_v<T, ptr_t>)
        {
            result->ptrVal = const_cast<void *>(static_cast<const void *>(val));
        }
        else if constexpr (std::is_same_v<T, real_t> or std::is_same_v<T, real32_t> or std::is_same_v<T, float>)
        {
            result->realVal = static_cast<double>(val);
        }
        else if constexpr (std::is_same_v<T, int_t> or std::is_same_v<T, char_t> or std::is_same_v<T, bool_t> or
                           std::is_enum_v<T>)
        {
            result->intVal = static_cast<int64_t>(val);
        }
        else if constexpr (std::is_same_v<T, uint_t>)
        {
            result->uintVal = static_cast<uint64_t>(val);
        }
        else
        {
            std::println("unsupported return type");
            std::terminate();
        }
    }
} // namespace umka
