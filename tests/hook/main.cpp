import std;
import umka;
import hooks;
import ut;

int main()
{
    using namespace ut;
    umka::umka umka{"./main.um", 4096, {hooks}};

    "hook add test"_test = [&umka] {
        auto res = umka.call<umka::int_t>("./main.um", "run_add");
        expect(res == 30);
    };

    "hook concat test"_test = [&umka] {
        auto res = umka.call<umka::str_t>("./main.um", "run_concat");
        expect(std::strcmp(res, "Hello, Umka!") == 0);
    };

    "hook mul real test"_test = [&umka] {
        auto res = umka.call<umka::real_t>("./main.um", "run_mul");
        expect(res == 10.0);
    };

    "hook sum arr test"_test = [&umka] {
        auto res = umka.call<umka::int_t>("./main.um", "run_sum");
        expect(res == 60);
    };

    "hook sum dyn arr test"_test = [&umka] {
        auto res = umka.call<umka::int_t>("./main.um", "run_dyn_sum");
        expect(res == 100);
    };

}
