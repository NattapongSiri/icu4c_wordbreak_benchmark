#include "unicode/brkiter.h"
#include "unicode/ustream.h"
#include "unicode/locid.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <vector>

#define MONTE_CARLO 100

int main(void) {
    std::ifstream in("data/lexitron_mod.txt");
    std::string line;
    std::vector<std::string> tokens;
    while(getline(in, line)) {
        tokens.push_back(line);
    }
    std::srand(unsigned (std::time(0)));
    in.close();

    icu::Locale locale = icu::Locale("th", "TH");
    UErrorCode err = U_ZERO_ERROR;
    auto bi = icu::BreakIterator::createWordInstance(locale, err);

    if (U_FAILURE(err)) {
        std::cout << "Fail to create Word Break Iterator for Thai language" << std::endl;
    }

    double best_f1 = 0;
    double worst_f1 = 1;
    double mean_f1 = 0;
    double var_f1 = 0;
    double mean_tokenization_time = 0;
    double var_tokenization_time = 0;

    for (size_t sim = 0; sim < MONTE_CARLO; sim++) {
        std::random_shuffle(tokens.begin(), tokens.end());
        icu::UnicodeString contents;
        std::vector<std::tuple<int32_t, int32_t>> indices;
        indices.reserve(tokens.size());
        int32_t offset = 0;

        for (auto t: tokens) {
            auto unicode_str = icu::UnicodeString::fromUTF8(t);
            contents.append(unicode_str);
            auto new_offset = offset + unicode_str.length();
            indices.push_back(std::make_tuple(offset, new_offset));
            offset = new_offset;
        }

        auto begin = std::chrono::system_clock::now();

        bi->setText(contents);
        int32_t cur = 0;
        int32_t next = bi->next();
        auto expected = indices.begin();

        int32_t expected_positive = indices.size();
        int32_t predicted_positive = 0;
        int32_t true_positive = 0;

        do {
            if (cur == std::get<0>(*expected) && next == std::get<1>(*expected)) {
                // match
                true_positive++;
                cur = next;
                next = bi->next();
                expected++;

                if (next != icu::BreakIterator::DONE) {
                    predicted_positive++;
                } else {
                    break;
                }
            } else if (next < std::get<1>(*expected)) {
                cur = next;
                next = bi->next();

                if (next != icu::BreakIterator::DONE) {
                    predicted_positive++;
                } else {
                    break;
                }
            } else if (next >= std::get<1>(*expected)) {
                expected++;
            }
        } while(expected != indices.end());

        auto end = std::chrono::system_clock::now();
        auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
        
        double precision = (double) true_positive / predicted_positive;
        double recall = (double) true_positive / expected_positive;
        double f1 = 2 * precision * recall / (precision + recall);

        if (f1 > best_f1) {
            best_f1 = f1;
        }

        if (f1 < worst_f1) {
            worst_f1 = f1;
        }

        std::cout << "Time took to tokenize sim " << sim << " is " << delta_ms << " ms" <<std::endl;
        std::cout << "F1 score of sim " << sim << " is " << f1 <<std::endl;

        if (sim > 0) {
            double prev_mean = mean_f1;
            double prev_tokenize_time = mean_tokenization_time;
            mean_f1 = ((mean_f1 * sim) + f1) / (sim + 1); 
            mean_tokenization_time = ((mean_tokenization_time * sim) + delta_ms) / (sim + 1);
            var_f1 = var_f1 + (f1 - prev_mean) * (f1 - mean_f1);
            var_tokenization_time = var_tokenization_time + (delta_ms - prev_tokenize_time) * (delta_ms - mean_tokenization_time);
        } else {
            mean_f1 = f1;
            mean_tokenization_time = delta_ms;
        }
    }

    std::cout << "Average F1 score " << mean_f1 << std::endl;
    std::cout << "F1 variance " << (var_f1 / ((double) MONTE_CARLO - 1)) << std::endl;
    std::cout << "Best F1 score " << best_f1 << std::endl;
    std::cout << "Worst F1 score " << worst_f1 << std::endl;
    std::cout << "Margin of error at 95% for F1 " << (1.984 * std::sqrt(var_f1 / ((double) MONTE_CARLO - 1)) / std::sqrt((double) MONTE_CARLO - 1)) << std::endl;
    std::cout << "Margin of error at 99% for F1 " << 2.626 * (var_f1 / std::sqrt((double) MONTE_CARLO - 1)) << std::endl;
    std::cout << "Mean tokenizer tokenization time " << mean_tokenization_time << "ms" << std::endl;
    std::cout << "Var tokenizer tokenization time " << (var_tokenization_time/ ((double) MONTE_CARLO - 1)) << "ms" << std::endl;
    std::cout << "Margin of error at 95% for tokenization time " << 1.984 * (var_tokenization_time / std::sqrt((double) MONTE_CARLO - 1)) / std::sqrt(((double) MONTE_CARLO - 1)) << std::endl;
}