# icu4c_wordbreak_benchmark
Measure key metric of word break of ICU4C on Thai word.

Key measurement area are:
1. Average F1-score
1. Variance of F1-score
1. Average time it took
1. Variance of time

## Project structure
- `main.c` - A C main file that perform word break test.
- `main.cpp` - A C++ main file that perform word break test.

## Test result
### C Thai word tokenizer
We use following command to compile C code:
```zsh
gcc main.c -o benchmark `pkg-config --libs --cflags icu-uc icu-i18n icu-io` -O2
```
Here's result of running `benchmark`:
- Average F1 score is 0.165
- Variance F1 is 0
- Best F1 score is 0.167
- Worst F1 score is 0.164
- Margin of error at 95% for F1 is 0.000087
- Margin of error at 99% for F1 is 0.000005
- Mean tokenizer tokenization time is 41.43 ms
- Var tokenizer tokenization time is 22.288 ms
- Margin of error at 95% for tokenization time is 44.219
### C++ Thai word tokenizer
We use following commaand to compile C++ code:
```zsh
g++ main.cpp -o benchmarkpp -std=c++11 `pkg-config --libs --cflags icu-uc icu-i18n icu-io` -O2
```
Here's result of running `benchmarkpp`:
- Average F1 score is 0.221
- F1 variance is 0
- Best F1 score is 0.223
- Worst F1 score is 0.22
- Margin of error at 95% for F1 is 0.000118065
- Margin of error at 99% for F1 is 9.16019e-06
- Mean tokenizer tokenization time is 42.95 ms
- Var tokenizer tokenization time is 49.987 ms
- Margin of error at 95% for tokenization time is 99.175
## Test result interpretation
It is surprisingly that C and C++ tokenizer have very large different F1-score. Either there's a bug that I didn't notice in my code or ICU has a bug, otherwise, the different should be subtle. The F1 score for C is 0.165 while F1 score for C++ is 0.221. It take about 41 ms for each tokenization in C. It take about 43 ms for each tokenization in C++. However, both have very large variance considering the average time is about 41ms, it has 22.28 variance in C and 49.99 in C++. We can assume that it take the same amount of time for both C and C++.

## Found a defect ?
Please file in issue if you find anything wrong with the code.