import umkacxx;
import std;
import boost.ut;

int main()
{
    using namespace boost::ut;
    umkacxx::umka vm{"./test.um"};

    "str return"_test = [&vm] {
        auto msg = vm.call<const char *>("get_str");
        expect(std::string_view{msg} == std::string_view{"Hello, World!"});
    };

    "int return"_test = [&vm] {
        auto val = vm.call<std::int64_t>("get_int");
        expect(val == 42_i);
    };

    "real return"_test = [&vm] {
        auto val = vm.call<double>("get_real");
        expect(val == 3.14_d);
    };

    "bool return"_test = [&vm] {
        auto val = vm.call<bool>("get_bool");
        expect(val == true);
    };

    "int arr return"_test = [&vm] {
        using raw_t = umkacxx::umka_dynarray_raw<std::int64_t>;
        using raii_t = umkacxx::umka_dynarray<std::int64_t>;
        auto arr = vm.call<raw_t, raii_t>("get_int_arr");
        expect(arr.length == 5_i);
        expect(arr.data[0] == 1_i);
        expect(arr.data[1] == 2_i);
        expect(arr.data[2] == 3_i);
        expect(arr.data[3] == 4_i);
        expect(arr.data[4] == 5_i);
    };

    "str arr return"_test = [&vm] {
        using raw_t = umkacxx::umka_dynarray_raw<const char *>;
        using raii_t = umkacxx::umka_dynarray<const char *>;
        auto arr = vm.call<raw_t, raii_t>("get_str_arr");
        expect(arr.length == 3_i);
        expect(std::string_view{arr.data[0]} == std::string_view{"Hello"});
        expect(std::string_view{arr.data[1]} == std::string_view{"World"});
        expect(std::string_view{arr.data[2]} == std::string_view{"Umka"});
    };
    struct basic_struct
    {
            const char *name;
            std::int64_t age;
    };

    "basic struct return"_test = [&vm] {
        auto s = vm.call<basic_struct>("get_basic_struct");
        expect(std::string_view{s.name} == std::string_view{"Umka"});
        expect(s.age == 42_i);
    };

    struct complex_struct_raw
    {
        public:
            const char *name;
            umkacxx::umka_dynarray_raw<const char *> tags;
            umkacxx::umka_dynarray_raw<std::int64_t> values;
    };

    struct complex_struct
    {
        public:
            const char *name;
            umkacxx::umka_dynarray<const char *> tags;
            umkacxx::umka_dynarray<std::int64_t> values;

            complex_struct(const complex_struct_raw &raw, umkacxx::vm_handle vm)
                : name{raw.name}, tags{raw.tags, vm}, values{raw.values, vm}
            {
            }
    };

    "complex struct return"_test = [&vm] {
        auto s = vm.call<complex_struct_raw, complex_struct>("get_complex_struct");
        expect(std::string_view{s.name} == std::string_view{"Umka"});
        expect(s.tags.length == 3_i);
        expect(std::string_view{s.tags.data[0]} == std::string_view{"foo"});
        expect(std::string_view{s.tags.data[1]} == std::string_view{"bar"});
        expect(std::string_view{s.tags.data[2]} == std::string_view{"baz"});
        expect(s.values.length == 5_i);
        expect(s.values.data[0] == 1_i);
        expect(s.values.data[1] == 2_i);
        expect(s.values.data[2] == 3_i);
        expect(s.values.data[3] == 4_i);
        expect(s.values.data[4] == 5_i);
    };
    struct inner_struct
    {
            const char *name;
            umkacxx::umka_dynarray_raw<std::int64_t> values;
    };

    "nested arr return"_test = [&vm] {
        using raw_t = umkacxx::umka_dynarray_raw<inner_struct>;
        using raii_t = umkacxx::umka_dynarray<inner_struct>;
        auto arr = vm.call<raw_t, raii_t>("get_nested");
    };
}
