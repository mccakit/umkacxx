import umka;
import std;
import ut;

// Layout mirrors of the Umka types in mod.um. Plain fields, str_t and arr_t
// only - nothing that owns memory, nothing non-trivially-copyable.

struct primitive_struct_umka
{
    public:
        umka::str_t name;
        umka::int_t age;
};

struct complex_struct_umka
{
    public:
        umka::str_t name;
        umka::arr_t<umka::str_t> tags;
        umka::arr_t<umka::int_t> values;
};

struct inner_struct_umka
{
    public:
        umka::str_t name;
        umka::arr_t<umka::int_t> values;
};

struct struct_arr_umka
{
    public:
        umka::arr_t<inner_struct_umka> data;
};

struct nested_struct_umka
{
    public:
        umka::int_t id;
        inner_struct_umka items;
};

struct nested_struct_arr_umka
{
    public:
        umka::arr_t<nested_struct_umka> data;
};

// Multiple results arrive as a single anonymous struct of item0, item1, ...
struct pair_umka
{
    public:
        umka::int_t item0;
        umka::int_t item1;
};

// A fixed [5]int can't be returned as a bare C array, so it travels wrapped.
struct fixed_arr_umka
{
    public:
        umka::int_t items[5];
};

enum class color : umka::int_t
{
    red = 0,
    green = 1,
    blue = 2
};

int main()
{
    using namespace ut;

    umka::vm_t vm{"./mod.um", 4096};

    // =======================================================================
    // Module naming
    // =======================================================================

    // mod.um is the script itself, so it is reachable both as the main module
    // and by path. "" is not a spelling of either - it is a hard error.
    "main module and path agree"_test = [&] {
        auto by_tag = vm.function(umka::main_module, "get_int");
        auto by_path = vm.function("./mod.um", "get_int");
        expect(by_tag.call<umka::int_t>() == 42);
        expect(by_path.call<umka::int_t>() == 42);
    };

    // =======================================================================
    // Scalar returns
    // =======================================================================

    auto get_str = vm.function(umka::main_module, "get_str");
    auto get_int = vm.function(umka::main_module, "get_int");
    auto get_real = vm.function(umka::main_module, "get_real");
    auto get_bool = vm.function(umka::main_module, "get_bool");
    auto get_char = vm.function(umka::main_module, "get_char");
    auto get_uint = vm.function(umka::main_module, "get_uint");
    auto get_real32 = vm.function(umka::main_module, "get_real32");
    auto get_enum = vm.function(umka::main_module, "get_enum");

    "str return"_test = [&] { expect(std::strcmp(get_str.call<umka::str_t>(), "Hello, World!") == 0); };
    "int return"_test = [&] { expect(get_int.call<umka::int_t>() == 42); };
    "real return"_test = [&] { expect(get_real.call<umka::real_t>() == 3.14); };
    "bool return"_test = [&] { expect(get_bool.call<umka::bool_t>() == true); };
    "char return"_test = [&] { expect(get_char.call<umka::char_t>() == 'a'); };
    "uint return"_test = [&] { expect(get_uint.call<umka::uint_t>() == 42u); };
    "real32 return"_test = [&] { expect(get_real32.call<umka::real32_t>() == 3.14f); };
    "enum return"_test = [&] { expect(get_enum.call<color>() == color::green); };

    // A handle is looked up once and called many times; this also proves the
    // params buffer survives repeated use.
    "repeated calls on one handle"_test = [&] {
        for (int i = 0; i < 3; ++i)
        {
            expect(get_int.call<umka::int_t>() == 42);
        }
    };

    // =======================================================================
    // Array returns
    // =======================================================================

    auto get_fixed_arr = vm.function(umka::main_module, "get_fixed_arr");
    auto get_int_arr = vm.function(umka::main_module, "get_int_arr");
    auto get_str_arr = vm.function(umka::main_module, "get_str_arr");

    "fixed arr return"_test = [&] {
        auto arr = get_fixed_arr.call<fixed_arr_umka>();
        expect(arr.items[0] == 10);
        expect(arr.items[2] == 30);
        expect(arr.items[4] == 50);
    };

    "int arr return"_test = [&] {
        auto arr = get_int_arr.call<umka::arr_t<umka::int_t>>();
        expect(arr.len() == 5);
        expect(!arr.empty());
        expect(arr[0] == 1);
        expect(arr[4] == 5);
        expect(arr.front() == 1);
        expect(arr.back() == 5);
        expect(arr.at(2) == 3);

        umka::int_t sum = 0;
        for (auto value : arr)
        {
            sum += value;
        }
        expect(sum == 15);
    };

    "str arr return"_test = [&] {
        auto arr = get_str_arr.call<umka::arr_t<umka::str_t>>();
        expect(arr.len() == 3);
        expect(std::strcmp(arr[0], "Hello") == 0);
        expect(std::strcmp(arr[2], "Umka") == 0);
    };

    // =======================================================================
    // Struct returns
    // =======================================================================

    auto get_primitive_struct = vm.function(umka::main_module, "get_primitive_struct");
    auto get_complex_struct = vm.function(umka::main_module, "get_complex_struct");
    auto get_struct_arr = vm.function(umka::main_module, "get_struct_arr");
    auto get_nested_struct = vm.function(umka::main_module, "get_nested_struct");
    auto get_nested_struct_arr = vm.function(umka::main_module, "get_nested_struct_arr");

    "primitive struct return"_test = [&] {
        auto result = get_primitive_struct.call<primitive_struct_umka>();
        expect(std::strcmp(result.name, "Umka") == 0);
        expect(result.age == 42);
    };

    "complex struct return"_test = [&] {
        auto result = get_complex_struct.call<complex_struct_umka>();
        expect(std::strcmp(result.name, "Umka") == 0);
        expect(result.tags.len() == 3);
        expect(std::strcmp(result.tags[1], "bar") == 0);
        expect(result.values.len() == 5);
        expect(result.values[4] == 5);
    };

    "struct arr return"_test = [&] {
        auto result = get_struct_arr.call<struct_arr_umka>();
        expect(result.data.len() == 2);
        expect(std::strcmp(result.data[0].name, "foo") == 0);
        expect(std::strcmp(result.data[1].name, "bar") == 0);
        expect(result.data[1].values[2] == 6);
    };

    "nested struct return"_test = [&] {
        auto result = get_nested_struct.call<nested_struct_umka>();
        expect(result.id == 42);
        expect(std::strcmp(result.items.name, "foo") == 0);
        expect(result.items.values.len() == 3);
        expect(result.items.values[2] == 3);
    };

    "nested struct arr return"_test = [&] {
        auto result = get_nested_struct_arr.call<nested_struct_arr_umka>();
        expect(result.data.len() == 2);
        expect(result.data[0].id == 42);
        expect(result.data[1].id == 43);
        expect(std::strcmp(result.data[1].items.name, "bar") == 0);
        expect(result.data[1].items.values[0] == 4);
    };

    // =======================================================================
    // Pointer returns
    // =======================================================================

    auto get_int_ptr = vm.function(umka::main_module, "get_int_ptr");
    auto get_struct_ptr = vm.function(umka::main_module, "get_struct_ptr");

    "int ptr return"_test = [&] {
        auto *ptr = get_int_ptr.call<umka::int_t *>();
        expect(ptr != nullptr);
        expect(*ptr == 100);
    };

    "struct ptr return"_test = [&] {
        auto *ptr = get_struct_ptr.call<primitive_struct_umka *>();
        expect(ptr != nullptr);
        expect(std::strcmp(ptr->name, "Heap") == 0);
        expect(ptr->age == 10);
    };

    // =======================================================================
    // Scalar parameters
    // =======================================================================

    auto add = vm.function(umka::main_module, "add");
    auto div_real = vm.function(umka::main_module, "div_real");
    auto scale32 = vm.function(umka::main_module, "scale32");
    auto negate = vm.function(umka::main_module, "negate");
    auto next_char = vm.function(umka::main_module, "next_char");
    auto add_uint = vm.function(umka::main_module, "add_uint");
    auto color_name = vm.function(umka::main_module, "color_name");
    auto format_all = vm.function(umka::main_module, "format_all");

    "int params"_test = [&] { expect(add.call<umka::int_t>(40, 2) == 42); };
    "real params"_test = [&] { expect(div_real.call<umka::real_t>(1.0, 4.0) == 0.25); };

    // real32 travels in real32Val on the way in and in realVal on the way out;
    // 2.5 and 4.0 are exact in both, so this compares cleanly.
    "real32 params"_test = [&] { expect(scale32.call<umka::real32_t>(2.5f, 4.0f) == 10.0f); };

    "bool param"_test = [&] { expect(negate.call<umka::bool_t>(true) == false); };
    "char param"_test = [&] { expect(next_char.call<umka::char_t>('a', 1) == 'b'); };

    "uint params"_test = [&] { expect(add_uint.call<umka::uint_t>(umka::uint_t{40}, umka::uint_t{2}) == 42u); };

    "enum param"_test = [&] { expect(std::strcmp(color_name.call<umka::str_t>(color::blue), "blue") == 0); };

    "mixed params"_test = [&] {
        auto result = format_all.call<umka::str_t>("Umka", 42, 3.14, true);
        expect(std::strcmp(result, "Umka|42|3.14|on") == 0);
    };

    // Arity is checked against the signature, so param_count is observable.
    "param count"_test = [&] {
        expect(add.param_count() == 2);
        expect(negate.param_count() == 1);
        expect(get_int.param_count() == 0);
    };

    // =======================================================================
    // String parameters
    // =======================================================================

    auto greet = vm.function(umka::main_module, "greet");
    auto str_len = vm.function(umka::main_module, "str_len");

    // A const char* is converted with umkaMakeStr on the way in; the callee
    // owns the result and releases it on return.
    "c string param"_test = [&] { expect(std::strcmp(greet.call<umka::str_t>("World"), "Hello, World!") == 0); };

    // len() reads the Umka string header, so this only passes if the parameter
    // really was converted rather than handed over as a bare pointer.
    "converted string param"_test = [&] { expect(str_len.call<umka::int_t>("Hello") == 5); };

    // A converted string is consumed by the callee. borrow() keeps ours alive
    // across both calls; without it the second call would read freed memory.
    "borrowed str param"_test = [&] {
        const umka::str_t owned = vm.make_str("Hello");
        expect(str_len.call<umka::int_t>(umka::borrow(owned)) == 5);
        expect(str_len.call<umka::int_t>(umka::borrow(owned)) == 5);
        vm.decref(owned);
    };

    // =======================================================================
    // Array parameters
    // =======================================================================

    auto sum_int_arr = vm.function(umka::main_module, "sum_int_arr");
    auto join_strs = vm.function(umka::main_module, "join_strs");
    auto sum_fixed_arr = vm.function(umka::main_module, "sum_fixed_arr");

    // Dynamic arrays are built explicitly: the wrapper never converts a C++
    // container. param_type supplies the Umka []int the VM needs to trace it.
    "dyn arr param"_test = [&] {
        auto values = vm.make_arr<umka::int_t>(sum_int_arr.param_type(0), 5);
        for (umka::int_t i = 0; i < values.len(); ++i)
        {
            values[i] = i + 1;
        }
        expect(sum_int_arr.call<umka::int_t>(values) == 15);
    };

    "borrowed dyn arr param"_test = [&] {
        auto values = vm.make_arr<umka::int_t>(sum_int_arr.param_type(0), 3);
        values[0] = 10;
        values[1] = 20;
        values[2] = 30;
        expect(sum_int_arr.call<umka::int_t>(umka::borrow(values)) == 60);
        expect(sum_int_arr.call<umka::int_t>(umka::borrow(values)) == 60);
        vm.decref(values);
    };

    "str arr param"_test = [&] {
        auto parts = vm.make_arr<umka::str_t>(join_strs.param_type(0), 3);
        parts[0] = vm.make_str("a");
        parts[1] = vm.make_str("b");
        parts[2] = vm.make_str("c");
        expect(std::strcmp(join_strs.call<umka::str_t>(parts, "-"), "a-b-c") == 0);
    };

    "fixed arr param"_test = [&] {
        const fixed_arr_umka values{{10, 20, 30, 40, 50}};
        expect(sum_fixed_arr.call<umka::int_t>(values) == 150);
    };

    // =======================================================================
    // Struct parameters
    // =======================================================================

    auto describe = vm.function(umka::main_module, "describe");
    auto age_by = vm.function(umka::main_module, "age_by");
    auto count_tags = vm.function(umka::main_module, "count_tags");

    "struct param"_test = [&] {
        const primitive_struct_umka person{vm.make_str("Umka"), 42};
        expect(std::strcmp(describe.call<umka::str_t>(person), "Umka is 42") == 0);
    };

    "struct in struct out"_test = [&] {
        const primitive_struct_umka person{vm.make_str("Umka"), 42};
        auto result = age_by.call<primitive_struct_umka>(person, 8);
        expect(std::strcmp(result.name, "Umka") == 0);
        expect(result.age == 50);
    };

    // A struct passed by value carries its fields' references with it and the
    // callee releases them on return, so `received` must not be used afterwards.
    "struct with arrays param"_test = [&] {
        auto received = get_complex_struct.call<complex_struct_umka>();
        expect(count_tags.call<umka::int_t>(received) == 3);
    };

    // =======================================================================
    // Pointer parameters, multiple results, void calls
    // =======================================================================

    auto inc_int_ptr = vm.function(umka::main_module, "inc_int_ptr");
    auto ptr_age = vm.function(umka::main_module, "ptr_age");
    auto make_pair = vm.function(umka::main_module, "make_pair");
    auto accumulate = vm.function(umka::main_module, "accumulate");
    auto get_total = vm.function(umka::main_module, "get_total");

    // The pointer addresses C++ memory, which Umka never reference counts, so
    // the callee's automatic release is a no-op here.
    "ptr param mutation"_test = [&] {
        umka::int_t counter = 100;
        inc_int_ptr.call(&counter, 5);
        expect(counter == 105);
    };

    "struct ptr param"_test = [&] {
        const primitive_struct_umka person{nullptr, 42};
        expect(ptr_age.call<umka::int_t>(&person) == 42);
    };

    "multiple results"_test = [&] {
        auto result = make_pair.call<pair_umka>(7, 9);
        expect(result.item0 == 7);
        expect(result.item1 == 9);
    };

    "void call"_test = [&] {
        accumulate.call(10);
        accumulate.call(32);
        expect(get_total.call<umka::int_t>() == 42);
    };

    // =======================================================================
    // Interpreter state
    // =======================================================================

    "interpreter is alive"_test = [&] {
        expect(vm.alive());
        expect(vm.mem_usage() >= 0);
        expect(umka::vm_t::version() != nullptr);
    };
}
