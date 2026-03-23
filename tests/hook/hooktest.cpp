import std;
import umkacxx;
import cppfuncs;
import boost.ut;

int main()
{
    using namespace boost::ut;
    umkacxx::umka umka{"./hooktest.um", 4096, {cppfuncs}};

    "add func"_test = [&umka] {
        int r = static_cast<int>(umka.call<umkacxx::types::int_t>("run"));
        expect(r == 3_i);
    };
}
