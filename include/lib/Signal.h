template<typename... Args>
class Signal {
public:
    using Slot = std::function<void(Args...)>;

    void connect(Slot slot) {
        slots_.push_back(std::move(slot));
    }

    void emit(Args... args) const {
        for (auto& slot : slots_) slot(args...);
    }

private:
    // Small-buffer: most signals have 1–3 listeners in the engine.
    std::vector<Slot> slots_;
};