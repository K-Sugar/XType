#pragma once

#include <deque>
#include <optional>
#include <string>

class ContextBuffer {
public:
    void        appendChar(char ch);
    void        backspace();
    void        setSuggestion(std::string text);
    std::string acceptNextWord();
    std::string acceptAll();
    void        dismiss();
    void        reset();

    std::string        contextText()   const;
    const std::string* suggestion()    const;  // nullptr = no active suggestion
    bool               hasSuggestion() const;

private:
    void push(char ch);  // rolling-window insert: drops oldest when at MAX_CHARS

    std::deque<char>           _buf;
    std::optional<std::string> _suggestion;

    static constexpr size_t MAX_CHARS = 500;
};
