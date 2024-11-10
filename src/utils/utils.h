#pragma once

namespace TL::utils {
    static std::optional<std::string> getEnvSimple( const char* name ) {
        const char* value = std::getenv( name );
        if ( value ) {
            return std::string( value );
        }
        return std::nullopt;
    }
} // namespace TL::utils