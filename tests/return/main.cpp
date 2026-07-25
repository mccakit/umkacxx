import umka;
import std;
import ut;

int main()
{
    using namespace ut;
    umka::umka umka{"./mod.um", 4096};

    // -----------------------------------------------------------------------
    // Return values
    // -----------------------------------------------------------------------

    "str return"_test = [&umka] {
        umka::str_t result = umka.call<umka::str_t>("./mod.um", "get_str");
        expect(std::strcmp(result, "Hello, World!") == 0);
    };

    "int return"_test = [&umka] {
        umka::int_t result{umka.call<umka::int_t>("./mod.um", "get_int")};
        expect(result == 42);
    };

    "real return"_test = [&umka] {
        double result{umka.call<umka::real_t>("./mod.um", "get_real")};
        expect(result == 3.14);
    };

    "bool return"_test = [&umka] {
        umka::bool_t result{umka.call<umka::bool_t>("./mod.um", "get_bool")};
        expect(result == true);
    };

    "char return"_test = [&umka] {
        char result{umka.call<umka::char_t>("./mod.um", "get_char")};
        expect(result == 'a');
    };

    "uint return"_test = [&umka] {
        auto result{umka.call<umka::uint_t>("./mod.um", "get_uint")};
        expect(result == 42u);
    };

    "real32 return"_test = [&umka] {
        auto result{umka.call<umka::real32_t>("./mod.um", "get_real32")};
        expect(result == 3.14f);
    };

    "fixed arr return"_test = [&umka] {
        auto arr = umka.call<std::array<umka::int_t, 5>>("./mod.um", "get_fixed_arr");
        expect(arr[0] == 10);
        expect(arr[1] == 20);
        expect(arr[2] == 30);
        expect(arr[3] == 40);
        expect(arr[4] == 50);
    };

    "int arr return"_test = [&umka] {
        auto result = umka.call<umka::arr_t<umka::int_t>>("./mod.um", "get_int_arr");
        expect(result.len() == 5);
        expect(result[0] == 1);
        expect(result[1] == 2);
        expect(result[2] == 3);
        expect(result[3] == 4);
        expect(result[4] == 5);
    };

    "str arr return"_test = [&umka] {
        auto result = umka.call<umka::arr_t<umka::str_t>>("./mod.um", "get_str_arr");
        expect(result.len() == 3);
        expect(std::strcmp(result[0], "Hello") == 0);
        expect(std::strcmp(result[1], "World") == 0);
        expect(std::strcmp(result[2], "Umka") == 0);
    };

    struct primitive_struct_umka
    {
        public:
            umka::str_t name;
            umka::int_t age;
    };

    "primitive struct return"_test = [&umka] {
        auto result = umka.call<primitive_struct_umka>("./mod.um", "get_primitive_struct");
        expect(std::strcmp(result.name, "Umka") == 0);
        expect(result.age == 42);
    };

    struct complex_struct_umka
    {
        public:
            umka::str_t name;
            umka::arr_t<umka::str_t> tags;
            umka::arr_t<umka::int_t> values;
    };

    "complex struct return"_test = [&umka] {
        auto complex_struct{umka.call<complex_struct_umka>("./mod.um", "get_complex_struct")};
        expect(std::strcmp(complex_struct.name, "Umka") == 0);
        expect(complex_struct.tags.len() == 3);
        expect(std::strcmp(complex_struct.tags[0], "foo") == 0);
        expect(std::strcmp(complex_struct.tags[1], "bar") == 0);
        expect(std::strcmp(complex_struct.tags[2], "baz") == 0);
        expect(complex_struct.values.len() == 5);
        expect(complex_struct.values[0] == 1);
        expect(complex_struct.values[1] == 2);
        expect(complex_struct.values[2] == 3);
        expect(complex_struct.values[3] == 4);
        expect(complex_struct.values[4] == 5);
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

    "struct arr return"_test = [&umka] {
        auto arr{umka.call<struct_arr_umka>("./mod.um", "get_struct_arr")};
        expect(arr.data.len() == 2);
        expect(std::strcmp(arr.data[0].name, "foo") == 0);
        expect(std::strcmp(arr.data[1].name, "bar") == 0);
    };

    struct nested_struct_umka
    {
        public:
            umka::int_t id;
            inner_struct_umka items;
    };

    "nested struct return"_test = [&umka] {
        auto outer_struct{umka.call<nested_struct_umka>("./mod.um", "get_nested_struct")};
        expect(outer_struct.id == 42);
        expect(std::strcmp(outer_struct.items.name, "foo") == 0);
        expect(outer_struct.items.values.len() == 3);
        expect(outer_struct.items.values[0] == 1);
        expect(outer_struct.items.values[1] == 2);
        expect(outer_struct.items.values[2] == 3);
    };

    struct nested_struct_arr_umka
    {
        public:
            umka::arr_t<nested_struct_umka> data;
    };

    "nested struct arr return"_test = [&umka] {
        auto arr{umka.call<nested_struct_arr_umka>("./mod.um", "get_nested_struct_arr")};
        expect(arr.data.len() == 2);
        expect(arr.data[0].id == 42);
        expect(std::strcmp(arr.data[0].items.name, "foo") == 0);
        expect(arr.data[0].items.values.len() == 3);
        expect(arr.data[0].items.values[0] == 1);
        expect(arr.data[0].items.values[1] == 2);
        expect(arr.data[0].items.values[2] == 3);
        expect(arr.data[1].id == 43);
        expect(std::strcmp(arr.data[1].items.name, "bar") == 0);
        expect(arr.data[1].items.values.len() == 3);
        expect(arr.data[1].items.values[0] == 4);
        expect(arr.data[1].items.values[1] == 5);
        expect(arr.data[1].items.values[2] == 6);
    };

    enum class color : umka::int_t
    {
        red = 0,
        green = 1,
        blue = 2
    };

    "enum return"_test = [&umka] {
        auto result = umka.call<color>("./mod.um", "get_enum");
        expect(result == color::green);
    };

    "int ptr return"_test = [&umka] {
        auto ptr = umka.call<umka::int_t *>("./mod.um", "get_int_ptr");
        expect(ptr != nullptr);
        expect(*ptr == 100);
    };

    "struct ptr return"_test = [&umka] {
        auto ptr = umka.call<primitive_struct_umka*>("./mod.um", "get_struct_ptr");
        expect(ptr != nullptr);
        expect(std::string(ptr->name) == "Heap");
        expect(ptr->age == 10);
    };

    // -----------------------------------------------------------------------
    // Scalar parameters
    // -----------------------------------------------------------------------

    "int params"_test = [&umka] {
        auto result = umka.call<umka::int_t>("./mod.um", "add", 40, 2);
        expect(result == 42);
    };

    "real params"_test = [&umka] {
        auto result = umka.call<umka::real_t>("./mod.um", "div_real", 1.0, 4.0);
        expect(result == 0.25);
    };

    // real32 travels in real32Val on the way in and in realVal on the way out;
    // 2.5 and 4.0 are exact in both, so this compares cleanly.
    "real32 params"_test = [&umka] {
        auto result = umka.call<umka::real32_t>("./mod.um", "scale32", 2.5f, 4.0f);
        expect(result == 10.0f);
    };

    "bool param"_test = [&umka] {
        auto result = umka.call<umka::bool_t>("./mod.um", "negate", true);
        expect(result == false);
    };

    "char param"_test = [&umka] {
        auto result = umka.call<umka::char_t>("./mod.um", "next_char", 'a', 1);
        expect(result == 'b');
    };

    "uint params"_test = [&umka] {
        auto result = umka.call<umka::uint_t>("./mod.um", "add_uint", umka::uint_t{40}, umka::uint_t{2});
        expect(result == 42u);
    };

    "enum param"_test = [&umka] {
        auto result = umka.call<umka::str_t>("./mod.um", "color_name", color::blue);
        expect(std::strcmp(result, "blue") == 0);
    };

    "mixed params"_test = [&umka] {
        auto result = umka.call<umka::str_t>("./mod.um", "format_all", "Umka", 42, 3.14, true);
        expect(std::strcmp(result, "Umka|42|3.14|on") == 0);
    };

    // -----------------------------------------------------------------------
    // String parameters
    // -----------------------------------------------------------------------

    "c string param"_test = [&umka] {
        auto result = umka.call<umka::str_t>("./mod.um", "greet", "World");
        expect(std::strcmp(result, "Hello, World!") == 0);
    };

    // len() reads the Umka string header, so this only passes if the parameter
    // was copied into the VM rather than handed over as a bare pointer.
    "std string param"_test = [&umka] {
        auto result = umka.call<umka::int_t>("./mod.um", "str_len", std::string{"Hello"});
        expect(result == 5);
    };

    "string view param"_test = [&umka] {
        const std::string_view view = std::string_view{"abcdefgh"}.substr(0, 3);
        auto result = umka.call<umka::int_t>("./mod.um", "str_len", view);
        expect(result == 3);
    };

    // A converted string is consumed by the callee. borrow() keeps ours alive
    // across both calls; without it the second call would read freed memory.
    "borrowed str param"_test = [&umka] {
        const umka::str_t owned = umka.make_str("Hello");
        auto first = umka.call<umka::int_t>("./mod.um", "str_len", umka::borrow(owned));
        auto second = umka.call<umka::int_t>("./mod.um", "str_len", umka::borrow(owned));
        expect(first == 5);
        expect(second == 5);
        umka.decref(owned);
    };

    // -----------------------------------------------------------------------
    // Array parameters
    // -----------------------------------------------------------------------

    "vector param"_test = [&umka] {
        auto result = umka.call<umka::int_t>("./mod.um", "sum_int_arr", std::vector<umka::int_t>{1, 2, 3, 4, 5});
        expect(result == 15);
    };

    "span param"_test = [&umka] {
        const std::vector<umka::int_t> values{10, 20, 30};
        auto result = umka.call<umka::int_t>("./mod.um", "sum_int_arr", std::span{values});
        expect(result == 60);
    };

    "string vector param"_test = [&umka] {
        const std::vector<std::string> parts{"a", "b", "c"};
        auto result = umka.call<umka::str_t>("./mod.um", "join_strs", parts, "-");
        expect(std::strcmp(result, "a-b-c") == 0);
    };

    "fixed arr param"_test = [&umka] {
        auto result = umka.call<umka::int_t>("./mod.um", "sum_fixed_arr", std::array<umka::int_t, 5>{10, 20, 30, 40, 50});
        expect(result == 150);
    };

    // -----------------------------------------------------------------------
    // Struct parameters
    // -----------------------------------------------------------------------

    "struct param"_test = [&umka] {
        const primitive_struct_umka person{umka.make_str("Umka"), 42};
        auto result = umka.call<umka::str_t>("./mod.um", "describe", person);
        expect(std::strcmp(result, "Umka is 42") == 0);
    };

    "struct in struct out"_test = [&umka] {
        const primitive_struct_umka person{umka.make_str("Umka"), 42};
        auto result = umka.call<primitive_struct_umka>("./mod.um", "age_by", person, 8);
        expect(std::strcmp(result.name, "Umka") == 0);
        expect(result.age == 50);
    };

    // A struct passed by value carries its fields' references with it, and the
    // callee releases them on return, so `received` must not be used afterwards.
    "struct with arrays param"_test = [&umka] {
        auto received = umka.call<complex_struct_umka>("./mod.um", "get_complex_struct");
        auto result = umka.call<umka::int_t>("./mod.um", "count_tags", received);
        expect(result == 3);
    };

    // -----------------------------------------------------------------------
    // Pointer parameters, multiple results, void calls
    // -----------------------------------------------------------------------

    // The pointer addresses C++ memory, which Umka never reference counts, so
    // the callee's automatic release is a no-op here.
    "ptr param mutation"_test = [&umka] {
        umka::int_t counter = 100;
        umka.call("./mod.um", "inc_int_ptr", &counter, 5);
        expect(counter == 105);
    };

    "struct ptr param"_test = [&umka] {
        const primitive_struct_umka person{nullptr, 42};
        auto result = umka.call<umka::int_t>("./mod.um", "ptr_age", &person);
        expect(result == 42);
    };

    struct pair_umka
    {
        public:
            umka::int_t item0;
            umka::int_t item1;
    };

    // Multiple results are a single anonymous struct of item0, item1, ...
    "multiple results"_test = [&umka] {
        auto result = umka.call<pair_umka>("./mod.um", "make_pair", 7, 9);
        expect(result.item0 == 7);
        expect(result.item1 == 9);
    };

    "void call"_test = [&umka] {
        umka.call("./mod.um", "accumulate", 10);
        umka.call("./mod.um", "accumulate", 32);
        auto result = umka.call<umka::int_t>("./mod.um", "get_total");
        expect(result == 42);
    };
}
