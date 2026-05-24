#include "Epilogue.h"

Epilogue::Epilogue(std::function<void()> i_callback)
	: m_callback(std::move(i_callback))
{}

Epilogue::~Epilogue()
{
    if (m_callback)
    {
        m_callback();
    }
}
