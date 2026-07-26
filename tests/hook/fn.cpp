module;
export module hooks;
import umka;
import std;

// Mirrors fn_wrap.um's Color.
enum class color : umka::int_t
{
    red = 0,
    green = 1,
    blue = 2
};

// Mirrors fn_wrap.um's point.
struct point_umka
{
    public:
        umka::str_t name;
        umka::int_t age;
};

// ------------------------------------------------------- scalar parameters

void umka_add(umka::slot_t *params, umka::slot_t *result)
{
    const auto a = umka::get_param<umka::int_t>(params, 0);
    const auto b = umka::get_param<umka::int_t>(params, 1);
    umka::set_result(result, a + b);
}

void umka_concat(umka::slot_t *params, umka::slot_t *result)
{
    const auto a = umka::get_param<umka::str_t>(params, 0);
    const auto b = umka::get_param<umka::str_t>(params, 1);
    const std::string joined = std::string{a} + b;
    umka::set_result(result, joined.c_str());
}

void umka_mul_real(umka::slot_t *params, umka::slot_t *result)
{
    const auto a = umka::get_param<umka::real_t>(params, 0);
    const auto b = umka::get_param<umka::real_t>(params, 1);
    umka::set_result(result, a * b);
}

// real32 arrives in real32Val and leaves in realVal - the two slots are not
// interchangeable, which is what this exercises.
void umka_scale32(umka::slot_t *params, umka::slot_t *result)
{
    const auto x = umka::get_param<umka::real32_t>(params, 0);
    const auto k = umka::get_param<umka::real32_t>(params, 1);
    umka::set_result(result, umka::real32_t{x * k});
}

void umka_add_uint(umka::slot_t *params, umka::slot_t *result)
{
    const auto a = umka::get_param<umka::uint_t>(params, 0);
    const auto b = umka::get_param<umka::uint_t>(params, 1);
    umka::set_result(result, umka::uint_t{a + b});
}

void umka_negate(umka::slot_t *params, umka::slot_t *result)
{
    const auto b = umka::get_param<umka::bool_t>(params, 0);
    umka::set_result(result, umka::bool_t{!b});
}

void umka_next_char(umka::slot_t *params, umka::slot_t *result)
{
    const auto c = umka::get_param<umka::char_t>(params, 0);
    const auto by = umka::get_param<umka::int_t>(params, 1);
    umka::set_result(result, static_cast<umka::char_t>(c + by));
}

void umka_color_name(umka::slot_t *params, umka::slot_t *result)
{
    switch (umka::get_param<color>(params, 0))
    {
        case color::red:
            umka::set_result(result, "red");
            return;
        case color::green:
            umka::set_result(result, "green");
            return;
        case color::blue:
            umka::set_result(result, "blue");
            return;
    }
    umka::set_result(result, "unknown");
}

// -------------------------------------------------------- array parameters

void umka_sum_arr(umka::slot_t *params, umka::slot_t *result)
{
    auto *items = umka::get_param<umka::int_t[3]>(params, 0);
    umka::int_t sum = 0;
    for (int i = 0; i < 3; ++i)
    {
        sum += items[i];
    }
    umka::set_result(result, sum);
}

void umka_sum_dyn_arr(umka::slot_t *params, umka::slot_t *result)
{
    const auto arr = umka::get_param<umka::arr_t<umka::int_t>>(params, 0);
    umka::int_t sum = 0;
    for (auto value : arr)
    {
        sum += value;
    }
    umka::set_result(result, sum);
}

// ------------------------------------------------------------ array results

// A dynamic array is structured, so its destination comes from umkaGetResult -
// the three-argument set_result. result_type supplies the Umka []int.
void umka_make_int_arr(umka::slot_t *params, umka::slot_t *result)
{
    auto *interpreter = umka::instance(result);
    const auto n = umka::get_param<umka::int_t>(params, 0);

    auto arr = umka::make_arr<umka::int_t>(interpreter, umka::result_type(params, result), n);
    for (umka::int_t i = 0; i < n; ++i)
    {
        arr[i] = (i + 1) * 10;
    }
    umka::set_result(params, result, arr);
}

void umka_make_str_arr(umka::slot_t *params, umka::slot_t *result)
{
    auto *interpreter = umka::instance(result);
    const auto n = umka::get_param<umka::int_t>(params, 0);

    auto arr = umka::make_arr<umka::str_t>(interpreter, umka::result_type(params, result), n);
    for (umka::int_t i = 0; i < n; ++i)
    {
        arr[i] = umka::make_str(interpreter, std::format("item{}", i).c_str());
    }
    umka::set_result(params, result, arr);
}

// ------------------------------------------------------------------ structs

void umka_describe(umka::slot_t *params, umka::slot_t *result)
{
    const auto p = umka::get_param<point_umka>(params, 0);
    const std::string text = std::format("{} is {}", p.name, p.age);
    umka::set_result(result, text.c_str());
}

void umka_make_point(umka::slot_t *params, umka::slot_t *result)
{
    auto *interpreter = umka::instance(result);
    const auto name = umka::get_param<umka::str_t>(params, 0);
    const auto age = umka::get_param<umka::int_t>(params, 1);

    // The incoming string is released when this returns, so the struct gets its
    // own copy rather than borrowing that one.
    const point_umka p{umka::make_str(interpreter, name), age};
    umka::set_result(params, result, p);
}

// ----------------------------------------------------------------- pointers

// Void extern: nothing is ever written to result.
void umka_inc_ptr(umka::slot_t *params, umka::slot_t * /*result*/)
{
    auto *p = umka::get_param<umka::int_t *>(params, 0);
    const auto by = umka::get_param<umka::int_t>(params, 1);
    *p += by;
}

void umka_alloc_int(umka::slot_t *params, umka::slot_t *result)
{
    auto *interpreter = umka::instance(result);
    const auto v = umka::get_param<umka::int_t>(params, 0);

    auto *p = static_cast<umka::int_t *>(umka::alloc_data(interpreter, sizeof(umka::int_t)));
    *p = v;
    umka::set_result(result, p);
}

// --------------------------------------------------------------- the module

constexpr char src[] = {
#embed "fn_wrap.um"
    , 0};

export umka::module_t hooks{
    "fn_wrap.um",
    src,
    {
        {"add", umka_add},
        {"concat", umka_concat},
        {"mul_real", umka_mul_real},
        {"scale32", umka_scale32},
        {"add_uint", umka_add_uint},
        {"negate", umka_negate},
        {"next_char", umka_next_char},
        {"color_name", umka_color_name},
        {"sum_arr", umka_sum_arr},
        {"sum_dyn_arr", umka_sum_dyn_arr},
        {"make_int_arr", umka_make_int_arr},
        {"make_str_arr", umka_make_str_arr},
        {"describe", umka_describe},
        {"make_point", umka_make_point},
        {"inc_ptr", umka_inc_ptr},
        {"alloc_int", umka_alloc_int},
    }};
