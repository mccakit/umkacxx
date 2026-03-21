import umkacxx;
import std;
import boost.ut;

int main()
{
    using namespace boost::ut;
    umkacxx::umka vm{"./test.um"};

    "str return"_test = [&vm] {
        auto msg = vm.call<const char *, std::string>("get_str");
        expect(msg == "Hello, World!");
    };

    "int return"_test = [&vm] {
        auto val = vm.call<std::int64_t, int>("get_int");
        expect(val == 42_i);
    };

    "real return"_test = [&vm] {
        auto val = vm.call<double, double>("get_real");
        expect(val == 3.14_d);
    };

    "bool return"_test = [&vm] {
        auto val = vm.call<bool, bool>("get_bool");
        expect(val == true);
    };

    struct umka_int_arr
    {
        public:
            umkacxx::vm_handle vm;
            std::vector<int> data;
            umka_int_arr(umkacxx::vm_handle vm, umkacxx::umka_dynarray<std::int64_t> arr) : vm{vm}
            {
                data.resize(arr.len());
                for (int i = 0; i < arr.len(); ++i)
                {
                    data[i] = static_cast<int>(arr.data[i]);
                }
                umkacxx::umka_decref(vm, arr.data);
            }
    };

    "int arr return"_test = [&vm] {
        auto int_arr = vm.call<umkacxx::umka_dynarray<std::int64_t>, umka_int_arr>("get_int_arr");
        expect(int_arr.data.size() == 5);
        expect(int_arr.data[0] == 1_i);
        expect(int_arr.data[1] == 2_i);
        expect(int_arr.data[2] == 3_i);
        expect(int_arr.data[3] == 4_i);
        expect(int_arr.data[4] == 5_i);
    };

    struct umka_str_arr
    {
        public:
            umkacxx::vm_handle vm;
            std::vector<const char *> data;
            umka_str_arr(umkacxx::vm_handle vm, umkacxx::umka_dynarray<const char *> arr) : vm{vm}
            {
                data.resize(arr.len());
                for (int i = 0; i < arr.len(); ++i)
                {
                    data[i] = arr.data[i];
                }
                umkacxx::umka_decref(vm, arr.data);
            }
    };

    "str arr return"_test = [&vm] {
        auto arr = vm.call<umkacxx::umka_dynarray<const char *>, umka_str_arr>("get_str_arr");
        expect(arr.data.size() == 3);
        expect(std::string_view{arr.data[0]} == std::string_view{"Hello"});
        expect(std::string_view{arr.data[1]} == std::string_view{"World"});
        expect(std::string_view{arr.data[2]} == std::string_view{"Umka"});
    };

    struct basic_struct_raw
    {
        public:
            const char *name;
            std::int64_t age;
    };

    struct basic_struct
    {
        public:
            umkacxx::vm_handle vm;
            std::string name;
            std::int64_t age;
            basic_struct(umkacxx::vm_handle vm, basic_struct_raw raw) : vm{vm}, name{raw.name}, age{raw.age}
            {
            }
    };

    "basic struct return"_test = [&vm] {
        auto s = vm.call<basic_struct_raw, basic_struct>("get_basic_struct");
        expect(std::string_view{s.name} == std::string_view{"Umka"});
        expect(s.age == 42_i);
    };

    struct complex_struct_raw
    {
        public:
            const char *name;
            umkacxx::umka_dynarray<const char *> tags;
            umkacxx::umka_dynarray<std::int64_t> values;
    };

    struct complex_struct
    {
        public:
            umkacxx::vm_handle vm;
            std::string name;
            std::vector<const char *> tags;
            std::vector<std::int64_t> values;
            complex_struct(umkacxx::vm_handle vm, complex_struct_raw raw) : vm{vm}, name{raw.name}
            {
                tags.resize(raw.tags.len());
                for (int i = 0; i < raw.tags.len(); ++i)
                {
                    tags[i] = raw.tags.data[i];
                }
                umkacxx::umka_decref(vm, raw.tags.data);

                values.resize(raw.values.len());
                for (int i = 0; i < raw.values.len(); ++i)
                {
                    values[i] = raw.values.data[i];
                }
                umkacxx::umka_decref(vm, raw.values.data);
            }
    };

    "complex struct return"_test = [&vm] {
        auto s = vm.call<complex_struct_raw, complex_struct>("get_complex_struct");
        expect(std::string_view{s.name} == std::string_view{"Umka"});
        expect(s.tags.size() == 3);
        expect(std::string_view{s.tags[0]} == std::string_view{"foo"});
        expect(std::string_view{s.tags[1]} == std::string_view{"bar"});
        expect(std::string_view{s.tags[2]} == std::string_view{"baz"});
        expect(s.values.size() == 5);
        expect(s.values[0] == 1_i);
        expect(s.values[1] == 2_i);
        expect(s.values[2] == 3_i);
        expect(s.values[3] == 4_i);
        expect(s.values[4] == 5_i);
    };

    struct inner_raw
    {
        public:
            const char *name;
            umkacxx::umka_dynarray<std::int64_t> values;
    };

    struct inner
    {
        public:
            std::string name;
            std::vector<std::int64_t> values;
    };

    struct nested_arr_raw
    {
        public:
            umkacxx::umka_dynarray<inner_raw> data;
    };

    struct nested_arr
    {
        public:
            umkacxx::vm_handle vm;
            std::vector<inner> data;
            nested_arr(umkacxx::vm_handle vm, nested_arr_raw raw) : vm{vm}
            {
                data.resize(raw.data.len());
                for (int i = 0; i < raw.data.len(); ++i)
                {
                    auto &src = raw.data.data[i];
                    inner dst;
                    dst.name = src.name;
                    dst.values.resize(src.values.len());
                    for (int j = 0; j < src.values.len(); ++j)
                    {
                        dst.values[j] = src.values.data[j];
                    }
                    data[i] = std::move(dst);
                }
                umkacxx::umka_decref(vm, raw.data.data);
            }
    };

    "nested arr return"_test = [&vm] {
        auto arr = vm.call<nested_arr_raw, nested_arr>("get_nested");
        expect(arr.data.size() == 2);
        expect(std::string_view{arr.data[0].name} == std::string_view{"foo"});
        expect(std::string_view{arr.data[1].name} == std::string_view{"bar"});
    };
}
