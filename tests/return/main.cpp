import umka;
import std;
import ut;

int main()
{
    using namespace ut;
    umka::umka umka{"./mod.um", 4096};

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
}
