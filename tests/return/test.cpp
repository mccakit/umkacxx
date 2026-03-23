import umkacxx;
import std;
import boost.ut;

int main()
{
    using namespace boost::ut;
    umkacxx::umka umka{"./test.um", 4096};

    "str return"_test = [&umka] {
        std::string msg{umka.call<umkacxx::types::str_t>("get_str")};
        expect(msg == "Hello, World!");
    };

    "int return"_test = [&umka] {
        int val{static_cast<int>(umka.call<umkacxx::types::int_t>("get_int"))};
        expect(val == 42_i);
    };

    "real return"_test = [&umka] {
        double val{static_cast<double>(umka.call<umkacxx::types::real_t>("get_real"))};
        expect(val == 3.14_d);
    };

    "bool return"_test = [&umka] {
        bool val{static_cast<bool>(umka.call<umkacxx::types::bool_t>("get_bool"))};
        expect(val == true);
    };

    struct umka_intarr_cxx
    {
        public:
            std::vector<int> data;
            umka_intarr_cxx(umkacxx::types::vm_handle handle, umkacxx::types::arr_t<umkacxx::types::int_t> arr)
            {
                data.resize(arr.len());
                for (int i = 0; i < arr.len(); ++i)
                {
                    data[i] = static_cast<int>(arr.data[i]);
                }
                arr.decref(handle);
            }
    };

    "int arr return"_test = [&umka] {
        umka_intarr_cxx int_arr{umka.vm, umka.call<umkacxx::types::arr_t<umkacxx::types::int_t>>("get_int_arr")};
        expect(int_arr.data.size() == 5);
        expect(int_arr.data[0] == 1_i);
        expect(int_arr.data[1] == 2_i);
        expect(int_arr.data[2] == 3_i);
        expect(int_arr.data[3] == 4_i);
        expect(int_arr.data[4] == 5_i);
    };

    struct umka_strarr_cxx
    {
        public:
            std::vector<std::string> data;
            umka_strarr_cxx(umkacxx::types::vm_handle vm, umkacxx::types::arr_t<umkacxx::types::str_t> arr)
            {
                data.resize(arr.len());
                for (int i = 0; i < arr.len(); ++i)
                {
                    data[i] = arr.data[i];
                }
                arr.decref(vm);
            }
    };

    "str arr return"_test = [&umka] {
        umka_strarr_cxx arr{umka.vm, umka.call<umkacxx::types::arr_t<umkacxx::types::str_t>>("get_str_arr")};
        expect(arr.data.size() == 3);
        expect(std::string_view{arr.data[0]} == "Hello");
        expect(std::string_view{arr.data[1]} == "World");
        expect(std::string_view{arr.data[2]} == "Umka");
    };

    struct basic_struct_umka
    {
        public:
            umkacxx::types::str_t name;
            umkacxx::types::int_t age;
    };

    struct basic_struct_cxx
    {
        public:
            std::string name{};
            int age{};
            basic_struct_cxx(basic_struct_umka raw) : name{raw.name}, age{static_cast<int>(raw.age)}
            {
            }
    };

    "basic struct return"_test = [&umka] {
        basic_struct_cxx basic_struct{umka.call<basic_struct_umka>("get_basic_struct")};
        expect(basic_struct.name == "Umka");
        expect(basic_struct.age == 42_i);
    };

    struct complex_struct_umka
    {
        public:
            umkacxx::types::str_t name;
            umkacxx::types::arr_t<umkacxx::types::str_t> tags;
            umkacxx::types::arr_t<umkacxx::types::int_t> values;
    };

    struct complex_struct_cxx
    {
        public:
            std::string name{};
            std::vector<std::string> tags{};
            std::vector<int> values{};
            complex_struct_cxx(umkacxx::types::vm_handle vm, complex_struct_umka umka_struct) : name{umka_struct.name}
            {
                tags.resize(umka_struct.tags.len());
                for (int i = 0; i < umka_struct.tags.len(); ++i)
                {
                    tags[i] = umka_struct.tags.data[i];
                }
                values.resize(umka_struct.values.len());
                for (int i = 0; i < umka_struct.values.len(); ++i)
                {
                    values[i] = umka_struct.values.data[i];
                }
                umka_struct.tags.decref(vm);
                umka_struct.values.decref(vm);
            }
    };

    "complex struct return"_test = [&umka] {
        complex_struct_cxx complex_struct{umka.vm, umka.call<complex_struct_umka>("get_complex_struct")};
        expect(complex_struct.name == "Umka");
        expect(complex_struct.tags.size() == 3);
        expect(complex_struct.tags[0] == "foo");
        expect(complex_struct.tags[1] == "bar");
        expect(complex_struct.tags[2] == "baz");
        expect(complex_struct.values.size() == 5);
        expect(complex_struct.values[0] == 1_i);
        expect(complex_struct.values[1] == 2_i);
        expect(complex_struct.values[2] == 3_i);
        expect(complex_struct.values[3] == 4_i);
        expect(complex_struct.values[4] == 5_i);
    };

    struct inner_umka
    {
        public:
            umkacxx::types::str_t name;
            umkacxx::types::arr_t<umkacxx::types::int_t> values;
    };

    struct inner_cxx
    {
        public:
            std::string name{};
            std::vector<int> values{};
    };

    struct nested_arr_umka
    {
        public:
            umkacxx::types::arr_t<inner_umka> data;
    };

    struct nested_arr_cxx
    {
        public:
            std::vector<inner_cxx> data;
            nested_arr_cxx(umkacxx::types::vm_handle vm, nested_arr_umka raw)
            {
                auto len = raw.data.len();
                data.reserve(len);
                for (int i = 0; i < len; ++i)
                {
                    auto &src = raw.data.data[i];
                    data.push_back({.name = src.name,
                                    .values = std::vector<int>(src.values.data, src.values.data + src.values.len())});
                }
                raw.data.decref(vm);
            }
    };

    "nested arr return"_test = [&umka] {
        nested_arr_cxx arr{umka.vm, umka.call<nested_arr_umka>("get_nested")};
        expect(arr.data.size() == 2);
        expect(arr.data[0].name == "foo");
        expect(arr.data[1].name == "bar");
    };
}
