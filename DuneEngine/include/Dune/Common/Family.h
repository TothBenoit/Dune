#pragma once

namespace Dune
{
    class Family 
    {
    public:
        template<typename>
        static dU32 Type()
        {
            static const dU32 value = Identifier();
            return value;
        }
    private:
        static dU32 Identifier()
        {
            static dU32 value = 0;
            return value++;
        }
    };
}