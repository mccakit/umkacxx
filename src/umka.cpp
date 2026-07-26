/**
 * @file umka.cpp
 * @brief C++ module wrapper over the Umka C API.
 *
 * The wrapper's job is to make Umka's C types easier to handle from C++, not to
 * hide them. It selects the correct @c UmkaStackSlot union member from a C++
 * type at compile time, checks argument counts, and owns the interpreter's
 * lifetime.
 *
 * Two invariants govern the whole interface:
 *
 * - **It reinterprets, it never converts.** Anything needing an allocation or a
 *   transforming copy is the caller's job, done explicitly with @ref umka::make_str
 *   or @ref umka::make_arr. Standard-library containers are rejected at compile
 *   time. The single exception is a @c const @c char* argument, which is turned
 *   into a real Umka string, because handing the VM a bare C pointer where it
 *   expects a reference-counted string is a memory bug rather than a convenience.
 * - **Nothing is reference counted implicitly.** Umka releases every
 *   reference-typed parameter when a callee returns, and releases nothing that it
 *   hands back. See @ref umka::borrow and @ref umka::vm_t::decref.
 *
 * Every failure is fatal: the wrapper prints to @c stderr and calls
 * @c std::terminate. Nothing here throws.
 */
module;
#include <cstdio> // stderr; std::println's FILE* overload needs the macro
#include <umka/umka_api.h>
export module umka;
import std;

export namespace umka
{
    /**
     * @defgroup types Vocabulary types
     * @brief Aliases mirroring Umka's own type names and the raw C API types.
     * @{
     */

    using str_t = const char *;   ///< Umka @c str. Must point at VM-owned memory.
    using int_t = std::int64_t;   ///< Umka @c int.
    using uint_t = std::uint64_t; ///< Umka @c uint.
    using real_t = double;        ///< Umka @c real.
    using real32_t = float;       ///< Umka @c real32. Uses a different slot member in each direction.
    using bool_t = bool;          ///< Umka @c bool.
    using char_t = char;          ///< Umka @c char.
    using ptr_t = void *;         ///< Umka @c ^void.

    using interpreter_handle_t = Umka *; ///< An interpreter instance.
    using slot_t = UmkaStackSlot;        ///< One 8-byte argument or result slot.
    using efunc_t = UmkaExternFunc;      ///< Signature of a C++ function callable from Umka.
    using type_t = const UmkaType *;     ///< An Umka type, needed to build dynamic arrays.
    using context_t = UmkaFuncContext;   ///< Entry offset, parameter buffer and result slot.

    /** @} */

    /**
     * @defgroup errors Failure reporting
     * @brief Every failure is fatal and reported here. Nothing in this file throws.
     * @{
     */

    /**
     * @brief Report a failure and terminate.
     * @param what Description of what went wrong.
     */
    [[noreturn]] auto fail(std::string_view what) -> void
    {
        std::println(stderr, "umka: {}", what);
        std::terminate();
    }

    /**
     * @brief Report a failure with the interpreter's error details and terminate.
     *
     * Adds the Umka message, source file, line and function name to the
     * diagnostic. Falls back to the plain form if the interpreter has no error
     * recorded.
     *
     * @param interpreter The interpreter whose error state to read.
     * @param what        Description of what the wrapper was attempting.
     */
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

    /** @} */

    /**
     * @defgroup naming Module naming
     * @brief Naming a module is always explicit.
     *
     * An empty string is not a spelling of the main module; @ref umka::main_module
     * is, and an empty path is a hard error.
     * @{
     */

    /** @brief Tag type selecting the main script. @see umka::main_module */
    class main_module_t
    {
    };

    /** @brief Pass to @ref umka::vm_t::function to look a function up in the main script. */
    inline constexpr main_module_t main_module{};

    /** @} */

    /**
     * @brief A view over an Umka dynamic array.
     *
     * Layout-compatible with @c UmkaDynArray(T): <tt>{type, itemsize, data}</tt>.
     * Slots are @c memcpy'd straight in and out of this, so the member order and
     * the absence of any extra state are load-bearing — in particular the type
     * cannot hold an interpreter pointer, which is why reference counting goes
     * through @ref umka::vm_t::decref rather than a member function.
     *
     * This is a **view, not an owner**. It never allocates and never releases.
     *
     * @tparam T The C++ mirror of the Umka item type. It must match the Umka item
     *           layout exactly; this is only verified for arrays built by
     *           @ref umka::make_arr.
     */
    template <typename T> class arr_t
    {
        public:
            type_t type{};    ///< The Umka @c []T type. The VM uses it to trace references.
            int_t itemsize{}; ///< Size of one item in bytes, as reported by Umka.
            T *data{};        ///< First item, or @c nullptr for an empty array.

            /** @brief Number of items. @return The length, read from the array header. */
            [[nodiscard]] auto len() const noexcept -> int_t
            {
                return umkaGetDynArrayLen(this);
            }

            /** @brief Whether the array holds no items. */
            [[nodiscard]] auto empty() const noexcept -> bool
            {
                return len() == 0;
            }

            /** @brief Iterator to the first item. */
            auto begin() noexcept -> T *
            {
                return data;
            }

            /** @brief Iterator past the last item. */
            auto end() noexcept -> T *
            {
                return data + len();
            }

            /** @brief Const iterator to the first item. */
            auto begin() const noexcept -> const T *
            {
                return data;
            }

            /** @brief Const iterator past the last item. */
            auto end() const noexcept -> const T *
            {
                return data + len();
            }

            /** @brief Const iterator to the first item. */
            auto cbegin() const noexcept -> const T *
            {
                return data;
            }

            /** @brief Const iterator past the last item. */
            auto cend() const noexcept -> const T *
            {
                return data + len();
            }

            /**
             * @brief Unchecked item access.
             * @param index Zero-based item index.
             * @warning No bounds checking. Use @ref at for a checked access.
             */
            auto operator[](int_t index) noexcept -> T &
            {
                return data[index];
            }

            /**
             * @brief Unchecked item access.
             * @param index Zero-based item index.
             * @warning No bounds checking. Use @ref at for a checked access.
             */
            auto operator[](int_t index) const noexcept -> const T &
            {
                return data[index];
            }

            /**
             * @brief Bounds-checked item access.
             * @param index Zero-based item index.
             * @note Terminates if @p index is out of range; it does not throw.
             */
            auto at(int_t index) -> T &
            {
                if (index < 0 || index >= len())
                {
                    fail(std::format("arr_t index {} out of range", index));
                }
                return data[index];
            }

            /**
             * @brief Bounds-checked item access.
             * @param index Zero-based item index.
             * @note Terminates if @p index is out of range; it does not throw.
             */
            auto at(int_t index) const -> const T &
            {
                if (index < 0 || index >= len())
                {
                    fail(std::format("arr_t index {} out of range", index));
                }
                return data[index];
            }

            /** @brief First item. @warning Undefined if the array is empty. */
            auto front() noexcept -> T &
            {
                return data[0];
            }

            /** @brief First item. @warning Undefined if the array is empty. */
            auto front() const noexcept -> const T &
            {
                return data[0];
            }

            /** @brief Last item. @warning Undefined if the array is empty. */
            auto back() noexcept -> T &
            {
                return data[len() - 1];
            }

            /** @brief Last item. @warning Undefined if the array is empty. */
            auto back() const noexcept -> const T &
            {
                return data[len() - 1];
            }
    };

    /**
     * @defgroup borrowing Borrowing
     * @brief Keeping a value alive across a call.
     *
     * Umka releases every reference-typed parameter (@c str, @c []T, @c ^T,
     * @c any) when the callee returns. Passing the same value twice reads freed
     * memory on the second call.
     * @{
     */

    /**
     * @brief Wrapper marking an argument as retained for the duration of a call.
     * @tparam T The wrapped value type.
     * @see umka::borrow
     */
    template <typename T> class borrowed_t
    {
        public:
            T value{}; ///< The retained value.
    };

    /**
     * @brief Retain a value so the callee's automatic release does not free it.
     *
     * @code
     * const umka::str_t owned = vm.make_str("Hello");
     * str_len.call<umka::int_t>(umka::borrow(owned));
     * str_len.call<umka::int_t>(umka::borrow(owned));   // still valid
     * vm.decref(owned);
     * @endcode
     *
     * @tparam T Must be @ref umka::str_t, @ref umka::arr_t or a pointer type.
     * @param value The value to retain.
     * @return A wrapper that @ref umka::set_param recognises.
     */
    template <typename T> [[nodiscard]] auto borrow(T value) -> borrowed_t<T>
    {
        return borrowed_t<T>{value};
    }

    /** @} */

    /**
     * @defgroup traits Type traits
     * @brief Compile-time predicates used to dispatch marshalling.
     * @{
     */

    /** @brief Always false; used to fire a @c static_assert from a dependent context. */
    template <typename> constexpr bool always_false_v = false;

    /** @brief Whether @c T is an @ref umka::arr_t. */
    template <typename> constexpr bool is_arr_v = false;
    template <typename T> constexpr bool is_arr_v<arr_t<T>> = true;

    /** @brief Whether @c T is a @ref umka::borrowed_t. */
    template <typename> constexpr bool is_borrowed_v = false;
    template <typename T> constexpr bool is_borrowed_v<borrowed_t<T>> = true;

    /** @brief Whether @c T is a C string pointer, and so needs conversion to an Umka string. */
    template <typename T> constexpr bool is_str_v = std::is_same_v<T, const char *> || std::is_same_v<T, char *>;

    /** @} */

    /**
     * @defgroup externs Interpreter access inside extern functions
     * @brief Reaching the VM from a function that was handed only slots.
     * @{
     */

    /**
     * @brief Recover the interpreter inside an extern function.
     *
     * An extern function is handed no interpreter; @c result->ptrVal carries it
     * on entry.
     *
     * @param result The result slot, exactly as received.
     * @return The interpreter that made the call.
     * @warning Call this **before** writing anything into @p result. Writing
     *          overwrites the handle.
     */
    [[nodiscard]] auto instance(slot_t *result) noexcept -> interpreter_handle_t
    {
        return static_cast<interpreter_handle_t>(result->ptrVal);
    }

    /**
     * @brief Umka type of a parameter.
     * @param params The parameter buffer.
     * @param index  Zero-based parameter index.
     * @return The parameter's Umka type, suitable for @ref umka::make_arr.
     */
    [[nodiscard]] auto param_type(slot_t *params, int index) noexcept -> type_t
    {
        return umkaGetParamType(params, index);
    }

    /**
     * @brief Umka type of the result.
     * @param params The parameter buffer.
     * @param result The result slot.
     * @return The result's Umka type, suitable for @ref umka::make_arr.
     */
    [[nodiscard]] auto result_type(slot_t *params, slot_t *result) noexcept -> type_t
    {
        return umkaGetResultType(params, result);
    }

    /**
     * @brief Build a reference-counted Umka string.
     * @param interpreter The interpreter that will own the string.
     * @param s           Null-terminated source text, copied.
     * @return A VM-owned string.
     */
    [[nodiscard]] auto make_str(interpreter_handle_t interpreter, str_t s) -> str_t
    {
        return umkaMakeStr(interpreter, s);
    }

    /**
     * @brief Build a reference-counted Umka dynamic array.
     *
     * @tparam T The C++ mirror of the item type.
     * @param interpreter The interpreter that will own the array.
     * @param type        The Umka @c []T type, from @ref umka::param_type,
     *                    @ref umka::result_type or @c umkaGetFieldType. The VM
     *                    needs it to trace the array's references.
     * @param len         Number of items.
     * @return A VM-owned array of @p len default-initialised items.
     * @note Terminates if @p type is null, if @p len is out of range, or if
     *       Umka's item size disagrees with @c sizeof(T). This size check is the
     *       only type verification the wrapper is able to perform.
     */
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

    /**
     * @brief Allocate collected memory an extern function can hand back as a @c ^T.
     * @param interpreter The interpreter that will own and collect the block.
     * @param size        Size in bytes.
     * @param on_free     Optional callback invoked when the block is collected.
     * @return The new block.
     * @note Returning a pointer to C++ memory instead means the VM will try to
     *       collect something it does not own.
     */
    [[nodiscard]] auto alloc_data(interpreter_handle_t interpreter, int size, efunc_t on_free = nullptr) -> ptr_t
    {
        return umkaAllocData(interpreter, size, on_free);
    }

    /**
     * @brief Increment an object's reference count.
     * @param interpreter The owning interpreter.
     * @param ptr         VM-owned memory.
     */
    auto incref(interpreter_handle_t interpreter, const void *ptr) noexcept -> void
    {
        umkaIncRef(interpreter, const_cast<void *>(ptr));
    }

    /**
     * @brief Decrement an object's reference count.
     * @param interpreter The owning interpreter.
     * @param ptr         VM-owned memory.
     * @warning Release the outermost object only. Releasing an outer array or
     *          struct already releases everything nested inside it.
     */
    auto decref(interpreter_handle_t interpreter, const void *ptr) noexcept -> void
    {
        umkaDecRef(interpreter, const_cast<void *>(ptr));
    }

    /** @} */

    /**
     * @defgroup registration Native code registration
     * @brief Describing C++ functions and Umka source to bind before compilation.
     * @{
     */

    /** @brief One C++ function bound into Umka under a given name. */
    class func_t
    {
        public:
            std::string name{};  ///< Name as declared in the Umka module. Must match exactly.
            efunc_t extern_fn{}; ///< The C++ implementation.
    };

    /**
     * @brief An Umka source module supplied as a string, plus the functions it declares.
     *
     * Pairs body-less Umka declarations with their C++ implementations. Because
     * the source is a string, the @c .um file need not exist on disk; @c \#embed
     * bakes it into the binary.
     *
     * @code
     * constexpr char src[] = {
     * #embed "mylib.um"
     *     , 0};
     *
     * export umka::module_t mylib{"mylib.um", src, {{"add", umka_add}}};
     * @endcode
     */
    class module_t
    {
        public:
            std::string name{};              ///< Module path as seen by Umka's @c import.
            std::string source{};            ///< The module's Umka source text.
            std::vector<func_t> functions{}; ///< Functions to bind before compiling.
    };

    /** @} */

    /**
     * @defgroup marshalling Marshalling
     * @brief Moving values between C++ types and Umka stack slots.
     * @{
     */

    /**
     * @brief Write one argument into a parameter slot.
     *
     * Accepts scalars and enums, raw pointers, @ref umka::str_t (converted with
     * @c umkaMakeStr), @ref umka::arr_t, @ref umka::borrowed_t, and trivially
     * copyable mirror structs. Standard-library containers are rejected at
     * compile time by design.
     *
     * @tparam T Deduced argument type.
     * @param interpreter The interpreter, needed to convert strings.
     * @param params      A parameter buffer from a context filled in by
     *                    @c umkaGetFunc; it carries the stack frame layout
     *                    @c umkaGetParam needs.
     * @param index       Zero-based parameter index.
     * @param value       The argument. Taken by value, so arrays and string
     *                    literals have already decayed to pointers.
     */
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

    /**
     * @brief Read one argument inside an extern function.
     *
     * @tparam T The expected C++ type. A fixed array type such as
     *           <tt>int_t[3]</tt> yields a pointer to the first item; a class
     *           type is reinterpreted in place.
     * @param params The parameter buffer, exactly as received.
     * @param index  Zero-based parameter index.
     * @return The argument, read from the correct slot member for @p T.
     */
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

    /**
     * @brief Write a scalar result from an extern function.
     *
     * @tparam T A non-class type. Structured results need the three-argument
     *           overload, because their destination comes from @c umkaGetResult
     *           rather than from @p result itself.
     * @param result The result slot, exactly as received.
     * @param val    The value to return.
     */
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

    /**
     * @brief Write a result of any kind, including structs and dynamic arrays.
     *
     * @tparam T The result type. A class type must be trivially copyable.
     * @param params The parameter buffer, exactly as received.
     * @param result The result slot, exactly as received.
     * @param val    The value to return.
     * @note The interpreter is read out of @p result before @c umkaGetResult
     *       overwrites it, so this must run before anything else writes there.
     */
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

    /** @} */

    /**
     * @brief An Umka function resolved once and callable many times.
     *
     * Obtained from @ref umka::vm_t::function. The parameter buffer lives in the
     * interpreter's arena, which is not reclaimed until the VM is destroyed, so
     * resolve a function once and keep the handle rather than looking it up in a
     * loop.
     *
     * @warning An @c fn_t must not outlive its @ref umka::vm_t. Copying one
     *          shares the parameter buffer rather than duplicating it.
     */
    class fn_t
    {
        public:
            interpreter_handle_t interpreter{}; ///< The interpreter that resolved this function.
            context_t ctx{};                    ///< Entry offset, parameter buffer and result slot.
            std::string name{};                 ///< Function name, used in diagnostics.
            int params{};                       ///< Declared parameter count.

            /** @brief Declared parameter count. */
            [[nodiscard]] auto param_count() const noexcept -> int
            {
                return params;
            }

            /**
             * @brief Umka type of a parameter.
             * @param index Zero-based parameter index.
             * @return The parameter's Umka type, suitable for @ref umka::make_arr.
             */
            [[nodiscard]] auto param_type(int index) const noexcept -> type_t
            {
                return umkaGetParamType(ctx.params, index);
            }

            /**
             * @brief Run the function with whatever is already in the parameter buffer.
             * @note Terminates on a runtime error, reporting the Umka message.
             */
            auto invoke() -> void
            {
                if (umkaCall(interpreter, &ctx) != 0)
                {
                    fail(interpreter, std::format("runtime error in '{}'", name));
                }
            }

            /**
             * @brief Fill the parameters, run the function, and return its result.
             *
             * @code
             * auto add = vm.function(umka::main_module, "add");
             * auto n = add.call<umka::int_t>(40, 2);
             * @endcode
             *
             * @tparam R    The result type; @c void by default. A class type must
             *              be trivially copyable and mirror the Umka layout.
             * @tparam Args Deduced argument types. See @ref umka::set_param for
             *              what is accepted.
             * @param args The arguments, taken by value.
             * @return The function's result, read from the correct slot member.
             * @warning The argument **count** is checked against the signature;
             *          argument **types** are not, because Umka exposes no type
             *          sizes. A mirror struct whose fields are in the wrong order
             *          corrupts the stack silently.
             * @note Terminates on an argument-count mismatch or a runtime error.
             */
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

    /**
     * @brief An Umka interpreter and everything loaded into it.
     *
     * Owns the interpreter for its lifetime. Neither copyable nor movable.
     *
     * @code
     * umka::vm_t vm{"script.um", 1024 * 1024};
     * auto add = vm.function(umka::main_module, "add");
     * auto n = add.call<umka::int_t>(40, 2);
     * @endcode
     */
    class vm_t
    {
        public:
            interpreter_handle_t interpreter{}; ///< The underlying C API handle.

            /**
             * @brief Allocate, initialise, register, compile and run.
             *
             * Performs the whole start-up sequence in the only order that works:
             * every extern function and module source is registered before
             * compilation, then @c main() is run.
             *
             * @param script     Path to the main script.
             * @param stack_size Stack size in **slots, not bytes**;
             *                   <tt>2 * 1024 * 1024</tt> is 16 MiB.
             * @param modules    Native modules to bind. @see umka::module_t
             * @note Terminates on allocation failure, an init error, a rejected
             *       function or module, a compile error, or a runtime error in
             *       @c main().
             */
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

            /** @brief Free the interpreter and everything it owns. */
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

            /**
             * @brief Resolve a function, given a raw module name.
             * @param module Module path, or @c nullptr for the main script.
             * @param name   Function name.
             * @return A callable handle.
             * @note Terminates if the function is not found.
             */
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

            /**
             * @brief Resolve a function in the main script.
             * @param name Function name.
             * @return A callable handle.
             */
            [[nodiscard]] auto function(main_module_t /*main*/, std::string_view name) -> fn_t
            {
                return lookup(nullptr, name);
            }

            /**
             * @brief Resolve a function in a named module.
             * @param module Module path as Umka sees it, for example @c "./util.um".
             * @param name   Function name.
             * @return A callable handle.
             * @note An empty @p module is a hard error rather than a silent
             *       redirect; pass @ref umka::main_module instead.
             */
            [[nodiscard]] auto function(std::string_view module, std::string_view name) -> fn_t
            {
                if (module.empty())
                {
                    fail("empty module name: pass umka::main_module for the main module");
                }
                const std::string path{module};
                return lookup(path.c_str(), name);
            }

            /** @brief Build a reference-counted Umka string. @see umka::make_str */
            [[nodiscard]] auto make_str(str_t s) const -> str_t
            {
                return umka::make_str(interpreter, s);
            }

            /** @brief Build a reference-counted Umka dynamic array. @see umka::make_arr */
            template <typename T> [[nodiscard]] auto make_arr(type_t type, int_t len) const -> arr_t<T>
            {
                return umka::make_arr<T>(interpreter, type, len);
            }

            /** @brief Allocate collected memory. @see umka::alloc_data */
            [[nodiscard]] auto alloc_data(int size, efunc_t on_free = nullptr) const -> ptr_t
            {
                return umka::alloc_data(interpreter, size, on_free);
            }

            /** @brief Retain VM-owned memory. */
            auto incref(const void *ptr) const noexcept -> void
            {
                umka::incref(interpreter, ptr);
            }

            /** @brief Release VM-owned memory. */
            auto decref(const void *ptr) const noexcept -> void
            {
                umka::decref(interpreter, ptr);
            }

            /** @brief Retain a dynamic array. */
            template <typename T> auto incref(const arr_t<T> &array) const noexcept -> void
            {
                umka::incref(interpreter, array.data);
            }

            /**
             * @brief Release a dynamic array.
             * @warning Do not call this on an array nested inside another array
             *          or struct; releasing the outer object already releases the
             *          inner ones.
             */
            template <typename T> auto decref(const arr_t<T> &array) const noexcept -> void
            {
                umka::decref(interpreter, array.data);
            }

            /** @brief Whether the interpreter is still running. */
            [[nodiscard]] auto alive() const noexcept -> bool
            {
                return umkaAlive(interpreter);
            }

            /** @brief Bytes currently allocated by the VM. */
            [[nodiscard]] auto mem_usage() const noexcept -> int_t
            {
                return umkaGetMemUsage(interpreter);
            }

            /** @brief Umka version string. */
            [[nodiscard]] static auto version() noexcept -> str_t
            {
                return umkaGetVersion();
            }
    };

} // namespace umka
