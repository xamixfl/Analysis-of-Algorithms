#include "algorithms.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <time.h>
#endif

using Algorithm = Matrix (*)(const Matrix&, const Matrix&);
const std::array<Algorithm,3> functions{classical, winograd, optimized};
const std::array<const char*,3> names{"classical", "winograd", "optimized"};
volatile Value checksum_sink = 0;

double cpu_seconds() {
#ifdef _WIN32
    FILETIME creation, exit, kernel, user;
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user))
        throw std::runtime_error("GetProcessTimes failed");
    auto ticks = [](FILETIME t) {
        return (std::uint64_t(t.dwHighDateTime) << 32) | t.dwLowDateTime;
    };
    return static_cast<double>(ticks(kernel) + ticks(user)) * 1e-7;
#else
    timespec ts{};
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) != 0)
        throw std::runtime_error("clock_gettime failed");
    return static_cast<double>(ts.tv_sec) + ts.tv_nsec * 1e-9;
#endif
}
Matrix random_matrix(std::size_t r, std::size_t c, std::mt19937& rng) {
    Matrix a(r,c);
    std::uniform_int_distribution<int> dist(-10,10);
    for (auto& x : a.data) x = dist(rng);
    return a;
}
// Independent oracle: accumulate rank-one outer products in a different order.
Matrix oracle(const Matrix& a, const Matrix& b) {
    Matrix c(a.rows,b.cols);
    for (std::size_t k=0;k<a.cols;++k)
        for (std::size_t i=0;i<a.rows;++i)
            for (std::size_t j=0;j<b.cols;++j)
                c(i,j) += a(i,k)*b(k,j);
    return c;
}
int tests() {
    int passed=0;
    auto check=[&](const Matrix& a,const Matrix& b,const Matrix& expected) {
        for (auto f:functions)
            if (!(f(a,b)==expected)) throw std::runtime_error("Wrong product");
        ++passed;
    };
    Matrix a(2,3), b(3,2), expected(2,2);
    a.data={1,2,3,4,5,6}; b.data={7,8,9,10,11,12};
    expected.data={58,64,139,154}; check(a,b,expected);
    Matrix x(1,1),y(1,1),z(1,1);
    x.data={-3}; y.data={7}; z.data={-21}; check(x,y,z);
    Matrix zero(3,4),rect(4,2); check(zero,rect,Matrix(3,2));
    Matrix identity(3,3); for(std::size_t i=0;i<3;++i) identity(i,i)=1;
    Matrix square(3,3); square.data={1,-2,3,4,0,-6,7,8,9};
    check(square,identity,square); check(identity,square,square);
    Matrix row(1,3),col(3,1),scalar(1,1);
    row.data={1,-2,3};col.data={4,5,-6};scalar.data={-24};check(row,col,scalar);
    Matrix wide(1,2),tall(2,1);wide.data={1000000,-1000000};
    tall.data={1000000,-1000000};scalar.data={2000000000000LL};check(wide,tall,scalar);
    std::mt19937 rng(20261023);
    for(int t=0;t<1000;++t) {
        auto m=1+rng()%10,n=1+rng()%10,q=1+rng()%10;
        auto aa=random_matrix(m,n,rng),bb=random_matrix(n,q,rng);
        check(aa,bb,oracle(aa,bb));
    }
    auto rejects=[&](auto action) {
        bool rejected=false;
        try {action();} catch(const std::invalid_argument&) {rejected=true;}
        if(!rejected) throw std::runtime_error("Invalid input accepted");
        ++passed;
    };
    rejects([]{Matrix bad(0,2);});rejects([]{Matrix bad(2001,1);});
    for(auto f:functions) {
        rejects([&]{f(Matrix(2,3),Matrix(2,2));});
        rejects([&]{Matrix aa(1,1),bb(1,1);aa.data[0]=1000001;f(aa,bb);});
        rejects([&]{Matrix aa(1,1),bb(1,1);aa.data.clear();f(aa,bb);});
    }
    std::cout << "PASS: "<<passed<<" test cases, three algorithms; seed 20261023\n";
    return 0;
}
double batch(Algorithm f,const Matrix& a,const Matrix& b,std::size_t k) {
    const auto start=cpu_seconds();
    for(std::size_t t=0;t<k;++t) {
        auto c=f(a,b);
        // Observable result on each invocation prevents dead-code elimination.
        checksum_sink = c.data[t % c.data.size()];
    }
    return cpu_seconds()-start;
}
void benchmark(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir);
    std::ofstream raw(dir/"raw.csv"),summary(dir/"summary.csv");
    if(!raw || !summary) throw std::runtime_error("Cannot open results");
    raw<<"n,algorithm,series,k,total_cpu_s,mean_ms\n";
    summary<<"n,algorithm,k,median_ms,min_ms,max_ms\n";
    raw<<std::setprecision(12);summary<<std::setprecision(12);
    std::mt19937 rng(20261023);
    const std::array<int,16> sizes{32,33,64,65,96,97,128,129,192,193,256,257,384,385,512,513};
    for(int n:sizes) {
        const auto a=random_matrix(n,n,rng),b=random_matrix(n,n,rng);
        auto reference=classical(a,b);
        std::array<std::size_t,3> counts{};
        for(int alg=0;alg<3;++alg) {
            if(!(functions[alg](a,b)==reference)) throw std::runtime_error("Benchmark verification failed");
            std::size_t k=10; double elapsed;
            do {elapsed=batch(functions[alg],a,b,k);if(elapsed<0.25) k*=2;} while(elapsed<0.25);
            counts[alg]=k;
        }
        std::array<std::vector<double>,3> values;
        for(int series=0;series<5;++series)
            for(int offset=0;offset<3;++offset) {
                const int alg=(offset+series)%3;
                const double elapsed=batch(functions[alg],a,b,counts[alg]);
                if(elapsed<=0) throw std::runtime_error("Zero CPU duration");
                const double ms=elapsed*1000/static_cast<double>(counts[alg]);
                raw<<n<<','<<names[alg]<<','<<series+1<<','<<counts[alg]<<','<<elapsed<<','<<ms<<'\n';
                values[alg].push_back(ms);
            }
        for(int alg=0;alg<3;++alg) {
            std::sort(values[alg].begin(),values[alg].end());
            summary<<n<<','<<names[alg]<<','<<counts[alg]<<','<<values[alg][2]<<','<<values[alg].front()<<','<<values[alg].back()<<'\n';
        }
        raw.flush();summary.flush();
        std::cout<<"n="<<n<<" done\n"<<std::flush;
    }
}
long long input(const char* prompt,long long low,long long high) {
    for(;;) {
        std::cout<<prompt;
        std::string token;
        if(!(std::cin>>token)) throw std::runtime_error("End of input");
        try {
            std::size_t used=0;
            const auto v=std::stoll(token,&used);
            if(used==token.size() && v>=low && v<=high) return v;
        } catch(const std::invalid_argument&) {
        } catch(const std::out_of_range&) {
        }
        std::cout<<"Введите целое число в указанном диапазоне.\n";
    }
}
void print(const Matrix& a) {
    for(std::size_t i=0;i<a.rows;++i) {
        for(std::size_t j=0;j<a.cols;++j) std::cout<<a(i,j)<<' ';
        std::cout<<'\n';
    }
}
void interactive() {
    std::mt19937 rng(20261023);
    for(;;) {
        const auto choice=input("\n0 Выход; 1 Ручной ввод; 2 Случайные матрицы; 3 Тесты\n> ",0,3);
        if(choice==0)return;
        if(choice==3){tests();continue;}
        const auto m=static_cast<std::size_t>(input("Число строк A [1..2000]: ",1,2000));
        const auto n=static_cast<std::size_t>(input("Число столбцов A = число строк B [1..2000]: ",1,2000));
        const auto q=static_cast<std::size_t>(input("Число столбцов B [1..2000]: ",1,2000));
        Matrix a(m,n),b(n,q);
        if(choice==2){a=random_matrix(m,n,rng);b=random_matrix(n,q,rng);}
        else {
            std::cout<<"Матрица A построчно, элементы от -1000000 до 1000000:\n";
            for(auto& v:a.data)v=input("",-1000000,1000000);
            std::cout<<"Матрица B построчно, элементы от -1000000 до 1000000:\n";
            for(auto& v:b.data)v=input("",-1000000,1000000);
        }
        const int alg=static_cast<int>(input("1 Классический; 2 Винограда; 3 Оптимизированный: ",1,3))-1;
        const auto c=functions[alg](a,b);
        if(m*q<=400)print(c);
        else std::cout<<"Result "<<m<<" x "<<q<<", first element: "<<c(0,0)<<'\n';
    }
}
int main(int argc,char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    try {
        if(argc==2 && std::string(argv[1])=="--test")return tests();
        if(argc>=2 && std::string(argv[1])=="--benchmark") {
            tests();benchmark(argc==3?argv[2]:"results");return 0;
        }
        if(argc!=1) {std::cerr<<"Usage: matrix_lab [--test | --benchmark [directory]]\n";return 2;}
        interactive();
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
