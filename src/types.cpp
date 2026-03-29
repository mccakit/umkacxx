module;
#include <umka_api.h>
export module umkacxx:types;
import std;
export namespace umkacxx::types
{
    /// Shared ownership handle to the Umka VM.
    using vm_handle = std::shared_ptr<Umka>;

    /// Mirrors Umka's internal dynamic array layout.
    /// T must match the element type of the corresponding Umka []T slice.
    /// Use `.len()` to get the number of elements and `.data` to iterate.
    template <typename T> class arr_t
    {
        public:
            const UmkaType *type;
            int64_t itemsize;
            T *data;

            auto len() const -> int64_t
            {
                return umkaGetDynArrayLen(this);
            }

            /// Decrements the reference count of an Umka-managed heap pointer.
            /// Call this on the `.data` pointer of any `umka_dynarray` you received
            /// directly from the VM after you are done copying its contents.
            /// Do NOT call this on nested arrays inside an outer array — Umka frees
            /// those automatically when the outer array is decreffed.
            auto decref(const vm_handle vm) const -> void
            {
                umkaDecRef(vm.get(), data);
            }
    };

    /// Mirrors Umka types.
    using str_t = const char *;
    using int_t = std::int64_t;
    using uint_t = std::uint64_t;
    using real_t = double;
    using bool_t = bool;

    /// Mirrors Umka's internal stack slot layout.
    using umka_slot = UmkaStackSlot;
    using umka_efunc = UmkaExternFunc;
    struct func_t
    {
        public:
            std::string name;
            UmkaExternFunc fn;
    };

    class module_t
    {
        public:
            std::string name{};
            std::string src{};
            std::vector<func_t> funcs{};

            module_t(std::string_view name, std::string_view src, std::vector<func_t> funcs = {})
                : name{name}, src{src}, funcs{std::move(funcs)}
            {
            }
    };
    using slot_t = std::variant<int64_t, uint64_t, void*, double, float, const char*>;
} // namespace umkacxx::types
