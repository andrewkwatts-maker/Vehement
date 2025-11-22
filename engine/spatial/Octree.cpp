#include "Octree.hpp"

// Template instantiations for common types
namespace Nova {

// Explicit instantiation for uint64_t
template class Octree<uint64_t>;
template class Octree<uint32_t>;

} // namespace Nova
