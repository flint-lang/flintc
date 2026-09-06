#include "parser/hash.hpp"
#include "resolver/resolver.hpp"

Namespace *Hash::get_namespace() const {
    return Resolver::get_namespace_from_hash(*this);
}
