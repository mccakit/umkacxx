import std;
import umka;
import hooks;
import ut;

// Mirrors fn_wrap.um's point.
struct point_umka
{
    public:
        umka::str_t name;
        umka::int_t age;
};

int main()
{
    using namespace ut;

    umka::vm_t vm{"./main.um", 4096, {hooks}};

    // =======================================================================
    // Scalar parameters and results
    // =======================================================================

    auto run_add = vm.function(umka::main_module, "run_add");
    auto run_concat = vm.function(umka::main_module, "run_concat");
    auto run_mul = vm.function(umka::main_module, "run_mul");
    auto run_scale32 = vm.function(umka::main_module, "run_scale32");
    auto run_add_uint = vm.function(umka::main_module, "run_add_uint");
    auto run_negate = vm.function(umka::main_module, "run_negate");
    auto run_next_char = vm.function(umka::main_module, "run_next_char");
    auto run_color_name = vm.function(umka::main_module, "run_color_name");

    "hook int"_test = [&] { expect(run_add.call<umka::int_t>() == 30); };

    "hook str"_test = [&] {
        expect(std::strcmp(run_concat.call<umka::str_t>(), "Hello, Umka!") == 0);
    };

    "hook real"_test = [&] { expect(run_mul.call<umka::real_t>() == 10.0); };

    // real32 enters the extern in real32Val and leaves in realVal. 2.5 and 4.0
    // are exact in both, so a mix-up shows as a wildly wrong value, not rounding.
    "hook real32"_test = [&] { expect(run_scale32.call<umka::real32_t>() == 10.0f); };

    "hook uint"_test = [&] { expect(run_add_uint.call<umka::uint_t>() == 42u); };
    "hook bool"_test = [&] { expect(run_negate.call<umka::bool_t>() == false); };
    "hook char"_test = [&] { expect(run_next_char.call<umka::char_t>() == 'b'); };

    "hook enum"_test = [&] {
        expect(std::strcmp(run_color_name.call<umka::str_t>(), "blue") == 0);
    };

    // =======================================================================
    // Array parameters
    // =======================================================================

    auto run_sum = vm.function(umka::main_module, "run_sum");
    auto run_dyn_sum = vm.function(umka::main_module, "run_dyn_sum");

    "hook fixed arr param"_test = [&] { expect(run_sum.call<umka::int_t>() == 60); };
    "hook dyn arr param"_test = [&] { expect(run_dyn_sum.call<umka::int_t>() == 100); };

    // =======================================================================
    // Array results - the three-argument set_result
    // =======================================================================

    auto run_make_int_arr = vm.function(umka::main_module, "run_make_int_arr");
    auto run_make_int_arr_sum = vm.function(umka::main_module, "run_make_int_arr_sum");
    auto run_make_str_arr = vm.function(umka::main_module, "run_make_str_arr");
    auto run_make_str_arr_lens = vm.function(umka::main_module, "run_make_str_arr_lens");

    "hook builds int arr"_test = [&] {
        auto arr = run_make_int_arr.call<umka::arr_t<umka::int_t>>();
        expect(arr.len() == 4);
        expect(arr[0] == 10);
        expect(arr[3] == 40);
    };

    // Summed inside Umka rather than in C++, so the VM has to accept the array
    // as a real dynamic array and not just as bytes that happen to read back.
    "umka consumes built int arr"_test = [&] {
        expect(run_make_int_arr_sum.call<umka::int_t>() == 100);
    };

    "hook builds str arr"_test = [&] {
        auto arr = run_make_str_arr.call<umka::arr_t<umka::str_t>>();
        expect(arr.len() == 3);
        expect(std::strcmp(arr[0], "item0") == 0);
        expect(std::strcmp(arr[2], "item2") == 0);
    };

    // len() reads the Umka string header: 3 x len("itemN") == 15.
    "umka measures built strs"_test = [&] {
        expect(run_make_str_arr_lens.call<umka::int_t>() == 15);
    };

    // =======================================================================
    // Struct parameters and results
    // =======================================================================

    auto run_describe = vm.function(umka::main_module, "run_describe");
    auto run_make_point = vm.function(umka::main_module, "run_make_point");
    auto run_make_point_name_len = vm.function(umka::main_module, "run_make_point_name_len");

    "hook struct param"_test = [&] {
        expect(std::strcmp(run_describe.call<umka::str_t>(), "Umka is 42") == 0);
    };

    "hook struct result"_test = [&] {
        auto p = run_make_point.call<point_umka>();
        expect(std::strcmp(p.name, "Umka") == 0);
        expect(p.age == 42);
    };

    "umka measures built struct str"_test = [&] {
        expect(run_make_point_name_len.call<umka::int_t>() == 4);
    };

    // =======================================================================
    // Pointers
    // =======================================================================

    auto run_inc_ptr = vm.function(umka::main_module, "run_inc_ptr");
    auto run_alloc_int = vm.function(umka::main_module, "run_alloc_int");

    // Void extern mutating Umka-allocated memory through a ^int.
    "hook void extern via ptr"_test = [&] { expect(run_inc_ptr.call<umka::int_t>() == 105); };

    // Pointer result: the extern allocates with alloc_data so the VM owns and
    // collects it, rather than handing back C++ memory.
    "hook ptr result"_test = [&] { expect(run_alloc_int.call<umka::int_t>() == 77); };

    // =======================================================================
    // Interpreter state
    // =======================================================================

    "interpreter is alive"_test = [&] {
        expect(vm.alive());
        expect(vm.mem_usage() >= 0);
    };
}
