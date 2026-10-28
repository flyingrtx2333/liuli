// Blocking stdin host: no timer, polling, browser, runtime compilation or JSON bridge.
#include "model.h"
#include <charconv>
#include <filesystem>
#include <iomanip>
#include <sstream>
#ifdef EXPLICIT_UI_GLASS
#include "glass_renderer.h"
#endif

namespace {
// Bound retained command bytes while still draining an oversized line for recovery.
// istream::get blocks on input; this is not a polling loop.
bool read_command(std::istream& input, std::string& line, bool& oversized) {
    constexpr std::size_t limit = 1024 * 1024;
    line.clear(); oversized = false;
    char byte;
    while (input.get(byte)) {
        if (byte == '\n') return true;
        if (line.size() < limit) line.push_back(byte);
        else oversized = true;
    }
    return !line.empty() || oversized;  // Process a final line without newline.
}
void end_of_command(std::istringstream& input) {
    input >> std::ws;
    if (!input.eof()) throw std::invalid_argument("unexpected trailing input");
}
void ack(const char* status) {
    std::cout << "{\"status\":\"" << status << "\"}\n" << std::flush;
}
}
int main(int argc, char** argv) {
    try {
        if (argc > 2) throw std::invalid_argument("usage: live-host [PNG_OUTPUT_DIRECTORY]");
#ifndef EXPLICIT_UI_GLASS
        if (argc == 2) throw std::invalid_argument("this build has no native renderer");
#endif
        explicit_ui::Model model;
        uint64_t renders = 0;
        auto render = [&] {
#ifdef EXPLICIT_UI_GLASS
            if (argc == 2) {
                std::filesystem::create_directories(argv[1]);
                explicit_ui::render_glass(model, (std::filesystem::path(argv[1]) /
                    ("redraw-" + std::to_string(renders) + ".png")).string());
            }
#else
            (void)argv;
#endif
            // Counts renderer seam invocations, including the console-only sink.
            ++renders;
        };
        render();
        model.snapshot("initial", renders);
        std::vector<explicit_ui::Update> pending;
        bool batching = false;
        std::string line;
        bool oversized = false;
        while (read_command(std::cin, line, oversized)) {
            bool changed = false;
            bool committed = false;
            std::string command;
            try {
                if (oversized) throw std::invalid_argument("command size limit exceeded");
                std::istringstream input(line);
                if (!(input >> command)) throw std::invalid_argument("empty command");
                if (command == "quit") {
                    end_of_command(input);
                    ack("bye"); return 0;  // Any uncommitted batch is discarded.
                } else if (command == "begin") {
                    end_of_command(input);
                    if (batching) throw std::invalid_argument("batch already open");
                    batching = true; pending.clear(); ack("begun");
                } else if (command == "abort") {
                    end_of_command(input);
                    if (!batching) throw std::invalid_argument("no open batch");
                    batching = false; pending.clear(); ack("aborted");
                } else if (command == "commit") {
                    end_of_command(input);
                    if (!batching) throw std::invalid_argument("no open batch");
                    changed = model.apply(pending);
                    batching = false; pending.clear(); committed = true;
                } else if (command == "set-int" || command == "set-text") {
                    std::string id;
                    if (!(input >> id)) throw std::invalid_argument("missing state id");
                    explicit_ui::Value value;
                    if (command == "set-int") {
                        std::string token;
                        if (!(input >> token)) throw std::invalid_argument("missing integer");
                        int64_t number = 0;
                        auto parsed = std::from_chars(token.data(), token.data() + token.size(), number);
                        if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size())
                            throw std::invalid_argument("expected signed 64-bit integer");
                        value = number;
                    } else {
                        input >> std::ws;
                        if (input.peek() != '"') throw std::invalid_argument("text must be double quoted");
                        std::string text;
                        if (!(input >> std::quoted(text))) throw std::invalid_argument("unterminated quoted text");
                        value = std::move(text);
                    }
                    end_of_command(input);
                    explicit_ui::Update update{id, std::move(value)};
                    if (batching) {
                        if (pending.size() >= 1000) throw std::invalid_argument("batch size limit exceeded");
                        pending.push_back(std::move(update)); ack("queued");
                    } else {
                        changed = model.apply({update}); committed = true;
                    }
                } else if (command == "renderer-stats") {
                    end_of_command(input);
#ifdef EXPLICIT_UI_GLASS
                    const auto metrics = explicit_ui::glass_metrics();
                    std::cout << "{\"cache_builds\":" << metrics.cache_builds
                              << ",\"render_frames\":" << metrics.render_frames << "}\n" << std::flush;
#else
                    std::cout << "{\"cache_builds\":0,\"render_frames\":0}\n" << std::flush;
#endif
                } else if (command == "snapshot") {
                    end_of_command(input); model.snapshot("snapshot", renders);
                } else throw std::invalid_argument("unknown command");
            } catch (const std::exception& error) {
                // Model apply uses copy + no-throw pointer swap. Any command error aborts a batch.
                batching = false; pending.clear();
                std::cout << "{\"error\":"; explicit_ui::detail::emit(std::string(error.what()));
                std::cout << ",\"batch_aborted\":true}\n" << std::flush;
                continue;
            }
            if (committed) {
                // Rendering failure is fatal, not misreported as a rolled-back state commit.
                if (changed) render();
                model.snapshot(command, renders);
            }
        }
        if (!std::cin.eof()) throw std::runtime_error("stdin read failure");
        ack("eof"); return 0;
    } catch (const std::exception& error) {
        std::cerr << "host error: " << error.what() << '\n'; return 2;
    }
}
