# Fcitx5 engine — local rules
- C++17, no exceptions in hot path
- Never call Fcitx5 APIs from std::thread — use eventDispatcher().schedule()
- Build with: cmake -B build -G Ninja && ninja -C build
- Unit tests: ninja -C build test
