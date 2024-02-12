#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>

#include <nlohmann/json.hpp>

#include <fmt/chrono.h>
#include <fmt/std.h>

#include <algorithm>
#include <string>

#include <ctype.h>

constexpr struct {
    std::string_view url;
    bool (*check)(nlohmann::json &data);

    std::string_view code;
    std::string_view rate;
    std::string_view time;
} apis[] {
    {
        "https://open.er-api.com/v6/latest/",
        [](nlohmann::json &data) -> bool {
            if (data.contains("result") == false)
                return false;
            return data["result"] == "success";
        }, "base_code", "rates", "time_last_update_unix"
    },
    {
        "https://api.exchangerate-api.com/v4/latest/",
        [](nlohmann::json &data) -> bool {
            if (data.contains("provider") == false)
                return false;
            if (data.contains("result") == true && data["result"] == "error")
                return false;
            return data["provider"] == "https://www.exchangerate-api.com";
        }, "base", "rates", "time_last_updated"
    }
};
constexpr auto api = apis[0];

nlohmann::json download(std::string from)
{
    auto url = std::string(api.url) + from;
    std::stringstream data;

    curlpp::Easy request;
    {
        request.setOpt<curlpp::options::HttpHeader>({
            "Accept: application/json",
            "Content-Type: application/json"
        });
        request.setOpt<curlpp::options::Url>(url);
        request.setOpt<curlpp::options::HttpGet>(true);
        request.setOpt<curlpp::options::WriteStream>(&data);

        request.perform();
    }

    return nlohmann::json::parse(data.str());
}

auto main(int argc, char *argv[]) -> int
{
    if (argc < 3)
    {
        fmt::println(stderr, "Usage: exchange <from> <to> <amount:1>");
        return EXIT_FAILURE;
    }

    std::string from { argv[1] };
    std::transform(from.begin(), from.end(), from.begin(), ::toupper);

    std::string to { argv[2] };
    std::transform(to.begin(), to.end(), to.begin(), ::toupper);

    try {
        auto data = download(from);

        if (api.check(data) == false || data[api.code] != from)
        {
            fmt::println(stderr, "Could not fetch exchange rates for source currency: '{}'", from);
            return EXIT_FAILURE;
        }

        auto &rates = data[api.rate];
        if (rates.contains(to) == false)
        {
            fmt::println(stderr, "Could not fetch exchange rates for target currency: '{}'", to);
            return EXIT_FAILURE;
        }
        auto rate = rates[to].template get<double>();

        double amount = 1;
        if (argc >= 4)
        {
            try {
                amount = std::stod(argv[3]);
            }
            catch (...)
            {
                fmt::println(stderr, "Could not convert string to an integer: '{}'", argv[3]);
                return EXIT_FAILURE;
            }
        }

        assert(data.contains(api.time));
        fmt::println("[At: {:%Y-%m-%d %H:%M:%S}]: [Rate: {}]: {} {} = {:.6} {}", fmt::localtime(data[api.time]), rate, amount, from, amount * rate, to);
    }
    catch (const std::exception &err)
    {
        fmt::println(stderr, "{}", err);
        return EXIT_FAILURE;
    }
    catch (...)
    {
        fmt::println(stderr, "Unknown error");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}