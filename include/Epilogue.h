#pragma once

#include <functional>

class Epilogue
{
public:
    explicit Epilogue(std::function<void()> i_callback);

    // Destructor executes the callback when the object goes out of scope
    ~Epilogue();

    // Prevent copying to avoid multiple triggers on the same callback
    Epilogue(const Epilogue&) = delete;
    Epilogue& operator=(const Epilogue&) = delete;

    // Allow moving so it can be returned or transferred if necessary
    Epilogue(Epilogue&&) noexcept = default;
    Epilogue& operator=(Epilogue&&) noexcept = default;

private:
    std::function<void()> m_callback;
};


