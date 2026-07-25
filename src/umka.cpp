module;
#include <cstdio>
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
    using type_t = const UmkaType *;

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

    // Mirrors Umka's DynArray layout: {type, itemSize, data}. T must match the
    // Umka item layout exactly - only checked for arrays built by make_arr.
    template <typename T> class arr_t
    {
        public:
            const UmkaType *type;
            int_t itemsize;
            T *data;

            [[nodiscard]] auto len() const -> int_t
            {
                return umkaGetDynArrayLen(this);
            }

            [[nodiscard]] auto empty() const -> bool
            {
                return len() == 0;
            }

            auto incref() const -> void
            {
                umkaIncRef(interpreter, data);
            }

            // Don't call this on an array nested inside another array/struct -
            // releasing the outer object already releases the inner ones.
            auto decref() const -> void
            {
                umkaDecRef(interpreter, data);
            }

            [[nodiscard]] auto to_vector() const -> std::vector<T>
            {
                return std::vector<T>(begin(), end());
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

    // Umka releases every reference-typed parameter (str, []T, ^T, any) when a
    // function returns. Wrap a value in borrow() to keep your own copy alive
    // across the call.
    template <typename T> struct borrowed
    {
        public:
            T value;
    };

    template <typename T> [[nodiscard]] auto borrow(T value) -> borrowed<T>
    {
        return borrowed<T>{value};
    }

    namespace detail
    {
        template <typename> constexpr bool always_false = false;

        template <typename T> struct is_borrowed : std::false_type
        {
        };
        template <typename T> struct is_borrowed<borrowed<T>> : std::true_type
        {
        };
        template <typename T> constexpr bool is_borrowed_v = is_borrowed<std::remove_cvref_t<T>>::value;

        template <typename T> struct is_arr : std::false_type
        {
        };
        template <typename T> struct is_arr<arr_t<T>> : std::true_type
        {
        };
        template <typename T> constexpr bool is_arr_v = is_arr<std::remove_cvref_t<T>>::value;

        template <typename T> struct is_sequence : std::false_type
        {
        };
        template <typename T, typename A> struct is_sequence<std::vector<T, A>> : std::true_type
        {
        };
        template <typename T, std::size_t N> struct is_sequence<std::span<T, N>> : std::true_type
        {
        };
        template <typename T> constexpr bool is_sequence_v = is_sequence<std::remove_cvref_t<T>>::value;

        // A [N]char Umka array isn't a string - pass those as std::array<char_t, N>.
        template <typename T>
        constexpr bool is_string_v =
            std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> || std::is_same_v<T, char *> ||
            std::is_same_v<T, const char *> ||
            (std::is_array_v<T> && std::is_same_v<std::remove_cv_t<std::remove_extent_t<T>>, char>);

        template <typename T>
        constexpr bool is_structured_v =
            (std::is_class_v<T> || std::is_array_v<T>) && !is_string_v<T> && !is_borrowed_v<T> && !is_sequence_v<T>;

        template <typename T> auto make_umka_str(const T &value) -> char *
        {
            using U = std::remove_cvref_t<T>;
            if constexpr (std::is_same_v<U, std::string>)
            {
                return umkaMakeStr(interpreter, value.c_str());
            }
            else if constexpr (std::is_same_v<U, std::string_view>)
            {
                const std::string terminated{value}; // views aren't guaranteed null-terminated
                return umkaMakeStr(interpreter, terminated.c_str());
            }
            else
            {
                return umkaMakeStr(interpreter, static_cast<str_t>(value));
            }
        }

        [[noreturn]] auto fail(std::string_view msg) -> void
        {
            std::println(stderr, "umka: {}", msg);
            std::terminate();
        }
    } // namespace detail

    // type must be the Umka []T type (from umkaGetParamType/umkaGetResultType/
    // umkaGetFieldType) - the VM needs it to trace references.
    template <typename T> [[nodiscard]] auto make_arr(type_t type, std::int64_t len) -> arr_t<T>
    {
        if (!type)
        {
            detail::fail("cannot create a dynamic array without its Umka type");
        }

        arr_t<T> array{}; // zeroed: umkaMakeDynArray frees the previous contents first
        umkaMakeDynArray(interpreter, &array, type, static_cast<int>(len));

        if (array.itemsize != static_cast<int_t>(sizeof(T)))
        {
            detail::fail(
                std::format("item size mismatch: Umka says {} bytes, C++ type is {} bytes", array.itemsize, sizeof(T)));
        }
        return array;
    }

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
            return slot->real32Val; // arrives as a float, not a double
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
            static_assert(detail::always_false<T>, "unsupported parameter type");
        }
    }

    // result->ptrVal arrives holding the interpreter handle, not the result
    // storage - for a structured result, fetch the real destination with
    // umkaGetResult first. Call umkaGetInstance(result) before this, never after.
    template <typename T> auto set_result(slot_t *params, slot_t *result, const T &val) -> void
    {
        slot_t *slot = umkaGetResult(params, result);

        if constexpr (detail::is_string_v<T>)
        {
            slot->ptrVal = detail::make_umka_str(val);
        }
        else if constexpr (detail::is_structured_v<T>)
        {
            static_assert(std::is_trivially_copyable_v<T>, "a structured result must be trivially copyable");
            std::memcpy(slot->ptrVal, &val, sizeof(T));
        }
        else if constexpr (std::is_pointer_v<T> or std::is_same_v<T, ptr_t>)
        {
            slot->ptrVal = const_cast<void *>(static_cast<const void *>(val));
        }
        else if constexpr (std::is_same_v<T, bool_t>)
        {
            slot->intVal = val ? 1 : 0;
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            slot->realVal = static_cast<real_t>(val);
        }
        else if constexpr (std::is_same_v<T, uint_t>)
        {
            slot->uintVal = val;
        }
        else if constexpr (std::is_enum_v<T> or std::is_integral_v<T>)
        {
            slot->intVal = static_cast<int_t>(val);
        }
        else
        {
            static_assert(detail::always_false<T>, "unsupported result type");
        }
    }

    // Scalar-only overload for existing external functions. Structured results
    // need the three-arg form above since the destination comes from umkaGetResult.
    template <typename T> auto set_result(slot_t *result, const T &val) -> void
    {
        static_assert(!detail::is_structured_v<T>, "a structured result needs set_result(params, result, value)");

        if constexpr (detail::is_string_v<T>)
        {
            result->ptrVal = detail::make_umka_str(val);
        }
        else if constexpr (std::is_pointer_v<T> or std::is_same_v<T, ptr_t>)
        {
            result->ptrVal = const_cast<void *>(static_cast<const void *>(val));
        }
        else if constexpr (std::is_same_v<T, bool_t>)
        {
            result->intVal = val ? 1 : 0;
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            result->realVal = static_cast<real_t>(val);
        }
        else if constexpr (std::is_same_v<T, uint_t>)
        {
            result->uintVal = val;
        }
        else if constexpr (std::is_enum_v<T> or std::is_integral_v<T>)
        {
            result->intVal = static_cast<int_t>(val);
        }
        else
        {
            static_assert(detail::always_false<T>, "unsupported result type");
        }
    }

    [[nodiscard]] auto param_count(const UmkaFuncContext &fn) -> int
    {
        int count = 0;
        while (umkaGetParam(fn.params, count))
        {
            ++count;
        }
        return count;
    }

    // params must come from a UmkaFuncContext filled in by umkaGetFunc /
    // umkaMakeFuncContext - it carries the stack frame layout umkaGetParam needs.
    template <typename T> auto set_param(slot_t *params, int index, const T &value) -> void
    {
        using U = std::remove_cvref_t<T>;

        slot_t *slot = umkaGetParam(params, index);
        if (!slot)
        {
            detail::fail(std::format("parameter {} does not exist", index));
        }

        if constexpr (detail::is_borrowed_v<U>)
        {
            using inner_t = std::remove_cvref_t<decltype(value.value)>;
            static_assert(detail::is_arr_v<inner_t> || std::is_pointer_v<inner_t>,
                          "borrow() applies to str_t, arr_t<T> and pointer types");

            if constexpr (detail::is_arr_v<inner_t>)
            {
                umkaIncRef(interpreter, value.value.data);
                std::memcpy(slot, &value.value, sizeof(inner_t));
            }
            else
            {
                void *ptr = const_cast<void *>(static_cast<const void *>(value.value));
                umkaIncRef(interpreter, ptr);
                slot->ptrVal = ptr;
            }
        }
        else if constexpr (std::is_same_v<U, bool_t>)
        {
            slot->intVal = value ? 1 : 0;
        }
        else if constexpr (std::is_same_v<U, real32_t>)
        {
            slot->real32Val = value;
        }
        else if constexpr (std::is_floating_point_v<U>)
        {
            slot->realVal = static_cast<real_t>(value);
        }
        else if constexpr (std::is_same_v<U, uint_t>)
        {
            slot->uintVal = value;
        }
        else if constexpr (std::is_enum_v<U> or std::is_integral_v<U>)
        {
            slot->intVal = static_cast<int_t>(value);
        }
        else if constexpr (detail::is_string_v<U>)
        {
            slot->ptrVal = detail::make_umka_str(value);
        }
        else if constexpr (detail::is_sequence_v<U>)
        {
            using item_t = std::remove_cv_t<typename U::value_type>;
            const type_t type = umkaGetParamType(params, index);

            if constexpr (detail::is_string_v<item_t>)
            {
                auto array = make_arr<str_t>(type, std::ssize(value));
                int_t i = 0;
                for (const auto &item : value)
                {
                    array.data[i++] = detail::make_umka_str(item);
                }
                std::memcpy(slot, &array, sizeof(array));
            }
            else
            {
                static_assert(std::is_trivially_copyable_v<item_t>,
                              "dynamic array items must be trivially copyable; "
                              "build nested arrays and strings item by item with make_arr");
                auto array = make_arr<item_t>(type, std::ssize(value));
                int_t i = 0;
                for (const auto &item : value)
                {
                    array.data[i++] = item;
                }
                std::memcpy(slot, &array, sizeof(array));
            }
        }
        else if constexpr (std::is_pointer_v<U>)
        {
            slot->ptrVal = const_cast<void *>(static_cast<const void *>(value));
        }
        else if constexpr (detail::is_structured_v<U>)
        {
            static_assert(std::is_trivially_copyable_v<U>, "a structured parameter must be trivially copyable");
            std::memcpy(slot, &value, sizeof(U));
        }
        else
        {
            static_assert(detail::always_false<U>, "unsupported parameter type");
        }
    }

    class umka
    {
        public:
            umka(std::filesystem::path script, int stack_size, std::initializer_list<module_t> modules = {})
            {
                const std::string path = script.string();

                interpreter = umkaAlloc();
                if (!umkaInit(interpreter, path.c_str(), nullptr, stack_size, nullptr, 0, nullptr, true, true, nullptr))
                {
                    report("init error");
                }
                for (auto &module : modules)
                {
                    for (auto &function : module.functions)
                    {
                        if (!umkaAddFunc(interpreter, function.name.c_str(), function.extern_fn))
                        {
                            std::println(stderr, "umka: external function '{}' was rejected", function.name);
                        }
                    }
                    if (!umkaAddModule(interpreter, module.name.c_str(), module.source.c_str()))
                    {
                        std::println(stderr, "umka: module '{}' was rejected", module.name);
                    }
                }
                if (!umkaCompile(interpreter))
                {
                    report("compile error");
                }
                if (umkaRun(interpreter) != 0)
                {
                    report("runtime error");
                }
            }

            ~umka()
            {
                if (interpreter)
                {
                    umkaFree(interpreter);
                    interpreter = nullptr;
                }
            }

            umka(const umka &) = delete;
            auto operator=(const umka &) -> umka & = delete;
            umka(umka &&) = delete;
            auto operator=(umka &&) -> umka & = delete;

            [[nodiscard]] auto alive() const -> bool
            {
                return umkaAlive(interpreter);
            }

            [[nodiscard]] auto mem_usage() const -> int_t
            {
                return umkaGetMemUsage(interpreter);
            }

            [[nodiscard]] static auto version() -> str_t
            {
                return umkaGetVersion();
            }

            [[nodiscard]] auto make_str(str_t s) const -> str_t
            {
                return umkaMakeStr(interpreter, s);
            }

            auto incref(const void *ptr) const -> void
            {
                umkaIncRef(interpreter, const_cast<void *>(ptr));
            }

            auto decref(const void *ptr) const -> void
            {
                umkaDecRef(interpreter, const_cast<void *>(ptr));
            }

            // An empty module means the main module. Parameter count is checked
            // against the function signature; types aren't (Umka exposes no type
            // sizes), so a mismatched struct corrupts the stack silently.
            template <typename T = void, typename... Args>
            auto call(std::string_view module, std::string_view function, Args &&...args) const -> T
            {
                UmkaFuncContext &fn = context(module, function);

                constexpr int given = static_cast<int>(sizeof...(Args));
                const int expected = param_count(fn);
                if (given != expected)
                {
                    detail::fail(std::format("'{}' takes {} parameter(s), {} given", function, expected, given));
                }

                int index = 0;
                (set_param(fn.params, index++, std::forward<Args>(args)), ...);

                if constexpr (std::is_void_v<T>)
                {
                    run(fn, function);
                }
                else if constexpr (detail::is_structured_v<T>)
                {
                    static_assert(std::is_trivially_copyable_v<T>,
                                  "a structured result must be trivially copyable: use a plain struct, "
                                  "std::array or arr_t, not std::string or std::vector");

                    T result{};
                    fn.result->ptrVal = &result;
                    run(fn, function);
                    return result;
                }
                else
                {
                    run(fn, function);

                    if constexpr (std::is_same_v<T, bool_t>)
                    {
                        return fn.result->intVal != 0;
                    }
                    else if constexpr (std::is_floating_point_v<T>)
                    {
                        return static_cast<T>(fn.result->realVal);
                    }
                    else if constexpr (std::is_same_v<T, uint_t>)
                    {
                        return fn.result->uintVal;
                    }
                    else if constexpr (std::is_enum_v<T> or std::is_integral_v<T>)
                    {
                        return static_cast<T>(fn.result->intVal);
                    }
                    else if constexpr (std::is_pointer_v<T>)
                    {
                        return static_cast<T>(fn.result->ptrVal);
                    }
                    else
                    {
                        static_assert(detail::always_false<T>, "unsupported result type");
                    }
                }
            }

        private:
            // Reused across calls so umkaGetFunc doesn't re-allocate a buffer
            // every time; safe under reentrancy since the VM copies params onto
            // its own stack before running.
            mutable std::unordered_map<std::string, UmkaFuncContext> contexts{};

            auto context(std::string_view module, std::string_view function) const -> UmkaFuncContext &
            {
                std::string key{module};
                key += "::";
                key += function;

                const auto [entry, added] = contexts.try_emplace(std::move(key));
                if (!added)
                {
                    return entry->second;
                }

                const std::string module_name{module};
                const std::string function_name{function};
                if (!umkaGetFunc(interpreter,
                                 module_name.empty() ? nullptr : module_name.c_str(),
                                 function_name.c_str(),
                                 &entry->second))
                {
                    contexts.erase(entry);
                    detail::fail(std::format(
                        "function '{}' not found in module '{}'", function, module.empty() ? "<main>" : module));
                }
                return entry->second;
            }

            auto run(UmkaFuncContext &fn, std::string_view function) const -> void
            {
                if (umkaCall(interpreter, &fn) == 0)
                {
                    return;
                }
                report(std::format("runtime error in '{}'", function));
            }

            [[noreturn]] auto report(std::string_view what) const -> void
            {
                const UmkaError *error = umkaGetError(interpreter);
                std::println(stderr,
                             "umka: {}: {} ({}:{} in {})",
                             what,
                             error->msg ? error->msg : "<no message>",
                             error->fileName ? error->fileName : "?",
                             error->line,
                             error->fnName ? error->fnName : "?");
                std::terminate();
            }
    };

} // namespace umka
