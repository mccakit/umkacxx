module;
#include <cstdio> // stderr; std::println's FILE* overload needs the macro
#include <umka/umka_api.h>
export module umka;
import std;

export namespace umka
{
    // ------------------------------------------------------------------ types

    using str_t = const char *;
    using int_t = std::int64_t;
    using uint_t = std::uint64_t;
    using real_t = double;
    using real32_t = float;
    using bool_t = bool;
    using char_t = char;
    using ptr_t = void *;

    using interpreter_handle_t = Umka *;
    using slot_t = UmkaStackSlot;
    using efunc_t = UmkaExternFunc;
    using type_t = const UmkaType *;
    using context_t = UmkaFuncContext;

    // ----------------------------------------------------------------- errors

    // Every failure is fatal and reported here. Nothing in this file throws.
    [[noreturn]] auto fail(std::string_view what) -> void
    {
        std::println(stderr, "umka: {}", what);
        std::terminate();
    }

    [[noreturn]] auto fail(interpreter_handle_t interpreter, std::string_view what) -> void
    {
        const UmkaError *error = umkaGetError(interpreter);
        if (!error)
        {
            fail(what);
        }
        std::println(stderr,
                     "umka: {}: {} ({}:{} in {})",
                     what,
                     error->msg ? error->msg : "<no message>",
                     error->fileName ? error->fileName : "?",
                     error->line,
                     error->fnName ? error->fnName : "?");
        std::terminate();
    }

    // ---------------------------------------------------------- module naming

    // Naming a module is always explicit: "" is not a spelling of the main
    // module, umka::main_module is. An empty path is a hard error.
    class main_module_t
    {
    };

    inline constexpr main_module_t main_module{};

    // --------------------------------------------------------- dynamic arrays

    // Layout-compatible with UmkaDynArray(T): {type, itemsize, data}. Slots are
    // memcpy'd straight in and out of this, so member order and the absence of
    // any extra state are load-bearing. T must match the Umka item layout - only
    // checked for arrays built by vm_t::make_arr.
    template <typename T> class arr_t
    {
        public:
            type_t type{};
            int_t itemsize{};
            T *data{};

            [[nodiscard]] auto len() const noexcept -> int_t
            {
                return umkaGetDynArrayLen(this);
            }

            [[nodiscard]] auto empty() const noexcept -> bool
            {
                return len() == 0;
            }

            auto begin() noexcept -> T *
            {
                return data;
            }

            auto end() noexcept -> T *
            {
                return data + len();
            }

            auto begin() const noexcept -> const T *
            {
                return data;
            }

            auto end() const noexcept -> const T *
            {
                return data + len();
            }

            auto cbegin() const noexcept -> const T *
            {
                return data;
            }

            auto cend() const noexcept -> const T *
            {
                return data + len();
            }

            auto operator[](int_t index) noexcept -> T &
            {
                return data[index];
            }

            auto operator[](int_t index) const noexcept -> const T &
            {
                return data[index];
            }

            auto at(int_t index) -> T &
            {
                if (index < 0 || index >= len())
                {
                    fail(std::format("arr_t index {} out of range", index));
                }
                return data[index];
            }

            auto at(int_t index) const -> const T &
            {
                if (index < 0 || index >= len())
                {
                    fail(std::format("arr_t index {} out of range", index));
                }
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

    // -------------------------------------------------------------- borrowing

    // Umka releases every reference-typed parameter (str, []T, ^T, any) when the
    // callee returns. Wrap a value in borrow() to keep your own copy alive across
    // the call.
    template <typename T> class borrowed_t
    {
        public:
            T value{};
    };

    template <typename T> [[nodiscard]] auto borrow(T value) -> borrowed_t<T>
    {
        return borrowed_t<T>{value};
    }

    // ---------------------------------------------------------------- traits

    template <typename> constexpr bool always_false_v = false;

    template <typename> constexpr bool is_arr_v = false;
    template <typename T> constexpr bool is_arr_v<arr_t<T>> = true;

    template <typename> constexpr bool is_borrowed_v = false;
    template <typename T> constexpr bool is_borrowed_v<borrowed_t<T>> = true;

    template <typename T> constexpr bool is_str_v = std::is_same_v<T, const char *> || std::is_same_v<T, char *>;

    // ------------------------------------- interpreter access inside externs

    // An extern function is handed no interpreter. result->ptrVal carries it on
    // entry, so read it before writing anything into result.
    [[nodiscard]] auto instance(slot_t *result) noexcept -> interpreter_handle_t
    {
        return static_cast<interpreter_handle_t>(result->ptrVal);
    }

    [[nodiscard]] auto param_type(slot_t *params, int index) noexcept -> type_t
    {
        return umkaGetParamType(params, index);
    }

    [[nodiscard]] auto result_type(slot_t *params, slot_t *result) noexcept -> type_t
    {
        return umkaGetResultType(params, result);
    }

    [[nodiscard]] auto make_str(interpreter_handle_t interpreter, str_t s) -> str_t
    {
        return umkaMakeStr(interpreter, s);
    }

    // type must be the Umka []T type, from param_type / result_type /
    // umkaGetFieldType - the VM needs it to trace the array's references.
    template <typename T>
    [[nodiscard]] auto make_arr(interpreter_handle_t interpreter, type_t type, int_t len) -> arr_t<T>
    {
        if (!type)
        {
            fail("cannot create a dynamic array without its Umka type");
        }
        if (len < 0 || len > std::numeric_limits<int>::max())
        {
            fail(std::format("dynamic array length {} out of range", len));
        }

        arr_t<T> array{}; // zeroed: umkaMakeDynArray frees the previous contents first
        umkaMakeDynArray(interpreter, &array, type, static_cast<int>(len));

        if (array.itemsize != static_cast<int_t>(sizeof(T)))
        {
            fail(
                std::format("item size mismatch: Umka says {} bytes, C++ type is {} bytes", array.itemsize, sizeof(T)));
        }
        return array;
    }

    // Garbage-collected memory an extern function can hand back as a ^T.
    [[nodiscard]] auto alloc_data(interpreter_handle_t interpreter, int size, efunc_t on_free = nullptr) -> ptr_t
    {
        return umkaAllocData(interpreter, size, on_free);
    }

    auto incref(interpreter_handle_t interpreter, const void *ptr) noexcept -> void
    {
        umkaIncRef(interpreter, const_cast<void *>(ptr));
    }

    auto decref(interpreter_handle_t interpreter, const void *ptr) noexcept -> void
    {
        umkaDecRef(interpreter, const_cast<void *>(ptr));
    }

    // ----------------------------------------------- native code registration

    class func_t
    {
        public:
            std::string name{};
            efunc_t extern_fn{};
    };

    class module_t
    {
        public:
            std::string name{};
            std::string source{};
            std::vector<func_t> functions{};
    };

    // ------------------------------------------------------------ marshalling

    // params must come from a context filled in by umkaGetFunc - it carries the
    // stack frame layout umkaGetParam needs. Arguments arrive by value, so arrays
    // and string literals have already decayed to pointers.
    template <typename T> auto set_param(interpreter_handle_t interpreter, slot_t *params, int index, T value) -> void
    {
        slot_t *slot = umkaGetParam(params, index);
        if (!slot)
        {
            fail(std::format("parameter {} does not exist", index));
        }

        if constexpr (is_borrowed_v<T>)
        {
            using inner_t = decltype(value.value);
            static_assert(is_arr_v<inner_t> || std::is_pointer_v<inner_t>,
                          "borrow() applies to str_t, arr_t<T> and pointer types");

            if constexpr (is_arr_v<inner_t>)
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
        else if constexpr (std::is_same_v<T, bool_t>)
        {
            slot->intVal = value ? 1 : 0;
        }
        else if constexpr (std::is_same_v<T, real32_t>)
        {
            slot->real32Val = value; // a param slot carries a float here, not a double
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            slot->realVal = static_cast<real_t>(value);
        }
        else if constexpr (std::is_same_v<T, uint_t>)
        {
            slot->uintVal = value;
        }
        else if constexpr (std::is_enum_v<T> || std::is_integral_v<T>)
        {
            slot->intVal = static_cast<int_t>(value);
        }
        else if constexpr (is_str_v<T>)
        {
            slot->ptrVal = umkaMakeStr(interpreter, value);
        }
        else if constexpr (is_arr_v<T>)
        {
            std::memcpy(slot, &value, sizeof(T));
        }
        else if constexpr (std::is_pointer_v<T>)
        {
            slot->ptrVal = const_cast<void *>(static_cast<const void *>(value));
        }
        else if constexpr (std::is_class_v<T>)
        {
            static_assert(std::is_trivially_copyable_v<T>,
                          "a struct parameter must be trivially copyable: mirror the Umka "
                          "struct with plain fields, str_t and arr_t");
            std::memcpy(slot, &value, sizeof(T));
        }
        else
        {
            static_assert(always_false_v<T>, "unsupported parameter type");
        }
    }

    template <typename T> [[nodiscard]] auto get_param(slot_t *params, int index)
    {
        slot_t *slot = umkaGetParam(params, index);
        if (!slot)
        {
            fail(std::format("parameter {} does not exist", index));
        }

        if constexpr (std::is_array_v<T>)
        {
            return reinterpret_cast<std::remove_extent_t<T> *>(slot);
        }
        else if constexpr (std::is_class_v<T>)
        {
            return *reinterpret_cast<T *>(slot);
        }
        else if constexpr (is_str_v<T> || std::is_pointer_v<T>)
        {
            return static_cast<T>(slot->ptrVal);
        }
        else if constexpr (std::is_same_v<T, real32_t>)
        {
            return slot->real32Val; // arrives as a float, not a double
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            return static_cast<T>(slot->realVal);
        }
        else if constexpr (std::is_same_v<T, uint_t>)
        {
            return slot->uintVal;
        }
        else if constexpr (std::is_same_v<T, bool_t>)
        {
            return slot->intVal != 0;
        }
        else if constexpr (std::is_enum_v<T> || std::is_integral_v<T>)
        {
            return static_cast<T>(slot->intVal);
        }
        else
        {
            static_assert(always_false_v<T>, "unsupported parameter type");
        }
    }

    // Scalar results only. A structured result's destination comes from
    // umkaGetResult, so it needs the three-argument form below.
    template <typename T> auto set_result(slot_t *result, T val) -> void
    {
        static_assert(!std::is_class_v<T>, "a structured result needs set_result(params, result, value)");

        if constexpr (is_str_v<T>)
        {
            // result->ptrVal holds the interpreter on entry - read it before writing.
            auto *interpreter = static_cast<interpreter_handle_t>(result->ptrVal);
            result->ptrVal = umkaMakeStr(interpreter, val);
        }
        else if constexpr (std::is_pointer_v<T>)
        {
            result->ptrVal = const_cast<void *>(static_cast<const void *>(val));
        }
        else if constexpr (std::is_same_v<T, bool_t>)
        {
            result->intVal = val ? 1 : 0;
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            result->realVal = static_cast<real_t>(val); // real32Val is unused in result slots
        }
        else if constexpr (std::is_same_v<T, uint_t>)
        {
            result->uintVal = val;
        }
        else if constexpr (std::is_enum_v<T> || std::is_integral_v<T>)
        {
            result->intVal = static_cast<int_t>(val);
        }
        else
        {
            static_assert(always_false_v<T>, "unsupported result type");
        }
    }

    template <typename T> auto set_result(slot_t *params, slot_t *result, T val) -> void
    {
        // Read the interpreter out of result->ptrVal before umkaGetResult overwrites it.
        [[maybe_unused]] auto *interpreter = static_cast<interpreter_handle_t>(result->ptrVal);
        slot_t *slot = umkaGetResult(params, result);

        if constexpr (is_str_v<T>)
        {
            slot->ptrVal = umkaMakeStr(interpreter, val);
        }
        else if constexpr (std::is_class_v<T>)
        {
            static_assert(std::is_trivially_copyable_v<T>, "a structured result must be trivially copyable");
            std::memcpy(slot->ptrVal, &val, sizeof(T));
        }
        else if constexpr (std::is_pointer_v<T>)
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
        else if constexpr (std::is_enum_v<T> || std::is_integral_v<T>)
        {
            slot->intVal = static_cast<int_t>(val);
        }
        else
        {
            static_assert(always_false_v<T>, "unsupported result type");
        }
    }

    // ------------------------------------------------------- bound functions

    // A function looked up once and callable many times. The params buffer lives
    // in the interpreter's arena, so an fn_t must not outlive its vm_t. Copying
    // one shares that buffer rather than duplicating it.
    class fn_t
    {
        public:
            interpreter_handle_t interpreter{};
            context_t ctx{};
            std::string name{};
            int params{};

            [[nodiscard]] auto param_count() const noexcept -> int
            {
                return params;
            }

            [[nodiscard]] auto param_type(int index) const noexcept -> type_t
            {
                return umkaGetParamType(ctx.params, index);
            }

            auto invoke() -> void
            {
                if (umkaCall(interpreter, &ctx) != 0)
                {
                    fail(interpreter, std::format("runtime error in '{}'", name));
                }
            }

            // Parameter count is checked against the signature; types are not,
            // because Umka exposes no type sizes, so a mismatched struct corrupts
            // the stack silently.
            template <typename R = void, typename... Args> auto call(Args... args) -> R
            {
                constexpr int given = static_cast<int>(sizeof...(Args));
                if (given != params)
                {
                    fail(std::format("'{}' takes {} parameter(s), {} given", name, params, given));
                }

                int index = 0;
                (set_param(interpreter, ctx.params, index++, args), ...);

                if constexpr (std::is_void_v<R>)
                {
                    invoke();
                }
                else if constexpr (std::is_class_v<R>)
                {
                    static_assert(std::is_trivially_copyable_v<R>,
                                  "a structured result must be trivially copyable: use a plain "
                                  "struct or arr_t");
                    R result{};
                    ctx.result->ptrVal = &result;
                    invoke();
                    return result;
                }
                else
                {
                    invoke();

                    if constexpr (std::is_same_v<R, bool_t>)
                    {
                        return ctx.result->intVal != 0;
                    }
                    else if constexpr (std::is_floating_point_v<R>)
                    {
                        return static_cast<R>(ctx.result->realVal);
                    }
                    else if constexpr (std::is_same_v<R, uint_t>)
                    {
                        return ctx.result->uintVal;
                    }
                    else if constexpr (std::is_pointer_v<R>)
                    {
                        return static_cast<R>(ctx.result->ptrVal);
                    }
                    else if constexpr (std::is_enum_v<R> || std::is_integral_v<R>)
                    {
                        return static_cast<R>(ctx.result->intVal);
                    }
                    else
                    {
                        static_assert(always_false_v<R>, "unsupported result type");
                    }
                }
            }
    };

    // ------------------------------------------------------- the interpreter

    class vm_t
    {
        public:
            interpreter_handle_t interpreter{};

            vm_t(const std::filesystem::path &script, int stack_size, std::initializer_list<module_t> modules = {})
            {
                const std::string path = script.string();

                interpreter = umkaAlloc();
                if (!interpreter)
                {
                    fail("could not allocate an interpreter");
                }
                if (!umkaInit(interpreter, path.c_str(), nullptr, stack_size, nullptr, 0, nullptr, true, true, nullptr))
                {
                    fail(interpreter, "init error");
                }

                for (const auto &module : modules)
                {
                    for (const auto &function : module.functions)
                    {
                        if (!umkaAddFunc(interpreter, function.name.c_str(), function.extern_fn))
                        {
                            fail(std::format("external function '{}' was rejected", function.name));
                        }
                    }
                    if (!umkaAddModule(interpreter, module.name.c_str(), module.source.c_str()))
                    {
                        fail(std::format("module '{}' was rejected", module.name));
                    }
                }

                if (!umkaCompile(interpreter))
                {
                    fail(interpreter, "compile error");
                }
                if (umkaRun(interpreter) != 0)
                {
                    fail(interpreter, "runtime error");
                }
            }

            ~vm_t()
            {
                if (interpreter)
                {
                    umkaFree(interpreter);
                    interpreter = nullptr;
                }
            }

            vm_t(const vm_t &) = delete;
            auto operator=(const vm_t &) -> vm_t & = delete;
            vm_t(vm_t &&) = delete;
            auto operator=(vm_t &&) -> vm_t & = delete;

            [[nodiscard]] auto lookup(str_t module, std::string_view name) -> fn_t
            {
                fn_t fn{};
                fn.interpreter = interpreter;
                fn.name = std::string{name};

                if (!umkaGetFunc(interpreter, module, fn.name.c_str(), &fn.ctx))
                {
                    fail(std::format("function '{}' not found in module '{}'", name, module ? module : "<main>"));
                }
                while (umkaGetParam(fn.ctx.params, fn.params))
                {
                    ++fn.params;
                }
                return fn;
            }

            [[nodiscard]] auto function(main_module_t /*main*/, std::string_view name) -> fn_t
            {
                return lookup(nullptr, name);
            }

            [[nodiscard]] auto function(std::string_view module, std::string_view name) -> fn_t
            {
                if (module.empty())
                {
                    fail("empty module name: pass umka::main_module for the main module");
                }
                const std::string path{module};
                return lookup(path.c_str(), name);
            }

            [[nodiscard]] auto make_str(str_t s) const -> str_t
            {
                return umka::make_str(interpreter, s);
            }

            template <typename T> [[nodiscard]] auto make_arr(type_t type, int_t len) const -> arr_t<T>
            {
                return umka::make_arr<T>(interpreter, type, len);
            }

            [[nodiscard]] auto alloc_data(int size, efunc_t on_free = nullptr) const -> ptr_t
            {
                return umka::alloc_data(interpreter, size, on_free);
            }

            auto incref(const void *ptr) const noexcept -> void
            {
                umka::incref(interpreter, ptr);
            }

            auto decref(const void *ptr) const noexcept -> void
            {
                umka::decref(interpreter, ptr);
            }

            template <typename T> auto incref(const arr_t<T> &array) const noexcept -> void
            {
                umka::incref(interpreter, array.data);
            }

            // Don't call this on an array nested inside another array or struct -
            // releasing the outer object already releases the inner ones.
            template <typename T> auto decref(const arr_t<T> &array) const noexcept -> void
            {
                umka::decref(interpreter, array.data);
            }

            [[nodiscard]] auto alive() const noexcept -> bool
            {
                return umkaAlive(interpreter);
            }

            [[nodiscard]] auto mem_usage() const noexcept -> int_t
            {
                return umkaGetMemUsage(interpreter);
            }

            [[nodiscard]] static auto version() noexcept -> str_t
            {
                return umkaGetVersion();
            }
    };

} // namespace umka
