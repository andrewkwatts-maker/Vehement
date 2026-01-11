#include "VoxelTerrain.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <queue>

namespace Nova {

// ============================================================================
// Marching Cubes Tables
// ============================================================================

// Edge table indicates which edges are intersected for each cube configuration
const int MarchingCubes::edgeTable[256] = {
    0x0,   0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
    0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
    0x190, 0x99,  0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
    0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
    0x230, 0x339, 0x33,  0x13a, 0x636, 0x73f, 0x435, 0x53c,
    0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
    0x3a0, 0x2a9, 0x1a3, 0xaa,  0x7a6, 0x6af, 0x5a5, 0x4ac,
    0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
    0x460, 0x569, 0x663, 0x76a, 0x66,  0x16f, 0x265, 0x36c,
    0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
    0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff,  0x3f5, 0x2fc,
    0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
    0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55,  0x15c,
    0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
    0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc,
    0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
    0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
    0xcc,  0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
    0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
    0x15c, 0x55,  0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
    0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
    0x2fc, 0x3f5, 0xff,  0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
    0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
    0x36c, 0x265, 0x16f, 0x66,  0x76a, 0x663, 0x569, 0x460,
    0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
    0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa,  0x1a3, 0x2a9, 0x3a0,
    0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
    0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33,  0x339, 0x230,
    0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
    0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99,  0x190,
    0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
    0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0
};

// Triangle table indicates which triangles to create for each configuration
// -1 terminates the list
const int MarchingCubes::triTable[256][16] = {
    {-1},
    {0, 8, 3, -1},
    {0, 1, 9, -1},
    {1, 8, 3, 9, 8, 1, -1},
    {1, 2, 10, -1},
    {0, 8, 3, 1, 2, 10, -1},
    {9, 2, 10, 0, 2, 9, -1},
    {2, 8, 3, 2, 10, 8, 10, 9, 8, -1},
    {3, 11, 2, -1},
    {0, 11, 2, 8, 11, 0, -1},
    {1, 9, 0, 2, 3, 11, -1},
    {1, 11, 2, 1, 9, 11, 9, 8, 11, -1},
    {3, 10, 1, 11, 10, 3, -1},
    {0, 10, 1, 0, 8, 10, 8, 11, 10, -1},
    {3, 9, 0, 3, 11, 9, 11, 10, 9, -1},
    {9, 8, 10, 10, 8, 11, -1},
    {4, 7, 8, -1},
    {4, 3, 0, 7, 3, 4, -1},
    {0, 1, 9, 8, 4, 7, -1},
    {4, 1, 9, 4, 7, 1, 7, 3, 1, -1},
    {1, 2, 10, 8, 4, 7, -1},
    {3, 4, 7, 3, 0, 4, 1, 2, 10, -1},
    {9, 2, 10, 9, 0, 2, 8, 4, 7, -1},
    {2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1},
    {8, 4, 7, 3, 11, 2, -1},
    {11, 4, 7, 11, 2, 4, 2, 0, 4, -1},
    {9, 0, 1, 8, 4, 7, 2, 3, 11, -1},
    {4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1},
    {3, 10, 1, 3, 11, 10, 7, 8, 4, -1},
    {1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1},
    {4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1},
    {4, 7, 11, 4, 11, 9, 9, 11, 10, -1},
    {9, 5, 4, -1},
    {9, 5, 4, 0, 8, 3, -1},
    {0, 5, 4, 1, 5, 0, -1},
    {8, 5, 4, 8, 3, 5, 3, 1, 5, -1},
    {1, 2, 10, 9, 5, 4, -1},
    {3, 0, 8, 1, 2, 10, 4, 9, 5, -1},
    {5, 2, 10, 5, 4, 2, 4, 0, 2, -1},
    {2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1},
    {9, 5, 4, 2, 3, 11, -1},
    {0, 11, 2, 0, 8, 11, 4, 9, 5, -1},
    {0, 5, 4, 0, 1, 5, 2, 3, 11, -1},
    {2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1},
    {10, 3, 11, 10, 1, 3, 9, 5, 4, -1},
    {4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1},
    {5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1},
    {5, 4, 8, 5, 8, 10, 10, 8, 11, -1},
    {9, 7, 8, 5, 7, 9, -1},
    {9, 3, 0, 9, 5, 3, 5, 7, 3, -1},
    {0, 7, 8, 0, 1, 7, 1, 5, 7, -1},
    {1, 5, 3, 3, 5, 7, -1},
    {9, 7, 8, 9, 5, 7, 10, 1, 2, -1},
    {10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1},
    {8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1},
    {2, 10, 5, 2, 5, 3, 3, 5, 7, -1},
    {7, 9, 5, 7, 8, 9, 3, 11, 2, -1},
    {9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1},
    {2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1},
    {11, 2, 1, 11, 1, 7, 7, 1, 5, -1},
    {9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1},
    {5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
    {11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
    {11, 10, 5, 7, 11, 5, -1},
    {10, 6, 5, -1},
    {0, 8, 3, 5, 10, 6, -1},
    {9, 0, 1, 5, 10, 6, -1},
    {1, 8, 3, 1, 9, 8, 5, 10, 6, -1},
    {1, 6, 5, 2, 6, 1, -1},
    {1, 6, 5, 1, 2, 6, 3, 0, 8, -1},
    {9, 6, 5, 9, 0, 6, 0, 2, 6, -1},
    {5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1},
    {2, 3, 11, 10, 6, 5, -1},
    {11, 0, 8, 11, 2, 0, 10, 6, 5, -1},
    {0, 1, 9, 2, 3, 11, 5, 10, 6, -1},
    {5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1},
    {6, 3, 11, 6, 5, 3, 5, 1, 3, -1},
    {0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1},
    {3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1},
    {6, 5, 9, 6, 9, 11, 11, 9, 8, -1},
    {5, 10, 6, 4, 7, 8, -1},
    {4, 3, 0, 4, 7, 3, 6, 5, 10, -1},
    {1, 9, 0, 5, 10, 6, 8, 4, 7, -1},
    {10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1},
    {6, 1, 2, 6, 5, 1, 4, 7, 8, -1},
    {1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1},
    {8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1},
    {7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
    {3, 11, 2, 7, 8, 4, 10, 6, 5, -1},
    {5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1},
    {0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1},
    {9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
    {8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1},
    {5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
    {0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
    {6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1},
    {10, 4, 9, 6, 4, 10, -1},
    {4, 10, 6, 4, 9, 10, 0, 8, 3, -1},
    {10, 0, 1, 10, 6, 0, 6, 4, 0, -1},
    {8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1},
    {1, 4, 9, 1, 2, 4, 2, 6, 4, -1},
    {3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1},
    {0, 2, 4, 4, 2, 6, -1},
    {8, 3, 2, 8, 2, 4, 4, 2, 6, -1},
    {10, 4, 9, 10, 6, 4, 11, 2, 3, -1},
    {0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1},
    {3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1},
    {6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
    {9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1},
    {8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
    {3, 11, 6, 3, 6, 0, 0, 6, 4, -1},
    {6, 4, 8, 11, 6, 8, -1},
    {7, 10, 6, 7, 8, 10, 8, 9, 10, -1},
    {0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1},
    {10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1},
    {10, 6, 7, 10, 7, 1, 1, 7, 3, -1},
    {1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1},
    {2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
    {7, 8, 0, 7, 0, 6, 6, 0, 2, -1},
    {7, 3, 2, 6, 7, 2, -1},
    {2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1},
    {2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
    {1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
    {11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1},
    {8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
    {0, 9, 1, 11, 6, 7, -1},
    {7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1},
    {7, 11, 6, -1},
    {7, 6, 11, -1},
    {3, 0, 8, 11, 7, 6, -1},
    {0, 1, 9, 11, 7, 6, -1},
    {8, 1, 9, 8, 3, 1, 11, 7, 6, -1},
    {10, 1, 2, 6, 11, 7, -1},
    {1, 2, 10, 3, 0, 8, 6, 11, 7, -1},
    {2, 9, 0, 2, 10, 9, 6, 11, 7, -1},
    {6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1},
    {7, 2, 3, 6, 2, 7, -1},
    {7, 0, 8, 7, 6, 0, 6, 2, 0, -1},
    {2, 7, 6, 2, 3, 7, 0, 1, 9, -1},
    {1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1},
    {10, 7, 6, 10, 1, 7, 1, 3, 7, -1},
    {10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1},
    {0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1},
    {7, 6, 10, 7, 10, 8, 8, 10, 9, -1},
    {6, 8, 4, 11, 8, 6, -1},
    {3, 6, 11, 3, 0, 6, 0, 4, 6, -1},
    {8, 6, 11, 8, 4, 6, 9, 0, 1, -1},
    {9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1},
    {6, 8, 4, 6, 11, 8, 2, 10, 1, -1},
    {1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1},
    {4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1},
    {10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
    {8, 2, 3, 8, 4, 2, 4, 6, 2, -1},
    {0, 4, 2, 4, 6, 2, -1},
    {1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1},
    {1, 9, 4, 1, 4, 2, 2, 4, 6, -1},
    {8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1},
    {10, 1, 0, 10, 0, 6, 6, 0, 4, -1},
    {4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
    {10, 9, 4, 6, 10, 4, -1},
    {4, 9, 5, 7, 6, 11, -1},
    {0, 8, 3, 4, 9, 5, 11, 7, 6, -1},
    {5, 0, 1, 5, 4, 0, 7, 6, 11, -1},
    {11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1},
    {9, 5, 4, 10, 1, 2, 7, 6, 11, -1},
    {6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1},
    {7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1},
    {3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
    {7, 2, 3, 7, 6, 2, 5, 4, 9, -1},
    {9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1},
    {3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1},
    {6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
    {9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1},
    {1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
    {4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
    {7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1},
    {6, 9, 5, 6, 11, 9, 11, 8, 9, -1},
    {3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1},
    {0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1},
    {6, 11, 3, 6, 3, 5, 5, 3, 1, -1},
    {1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1},
    {0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
    {11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
    {6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1},
    {5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1},
    {9, 5, 6, 9, 6, 0, 0, 6, 2, -1},
    {1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
    {1, 5, 6, 2, 1, 6, -1},
    {1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
    {10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1},
    {0, 3, 8, 5, 6, 10, -1},
    {10, 5, 6, -1},
    {11, 5, 10, 7, 5, 11, -1},
    {11, 5, 10, 11, 7, 5, 8, 3, 0, -1},
    {5, 11, 7, 5, 10, 11, 1, 9, 0, -1},
    {10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1},
    {11, 1, 2, 11, 7, 1, 7, 5, 1, -1},
    {0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1},
    {9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1},
    {7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
    {2, 5, 10, 2, 3, 5, 3, 7, 5, -1},
    {8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1},
    {9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1},
    {9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
    {1, 3, 5, 3, 7, 5, -1},
    {0, 8, 7, 0, 7, 1, 1, 7, 5, -1},
    {9, 0, 3, 9, 3, 5, 5, 3, 7, -1},
    {9, 8, 7, 5, 9, 7, -1},
    {5, 8, 4, 5, 10, 8, 10, 11, 8, -1},
    {5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1},
    {0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1},
    {10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
    {2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1},
    {0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
    {0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
    {9, 4, 5, 2, 11, 3, -1},
    {2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1},
    {5, 10, 2, 5, 2, 4, 4, 2, 0, -1},
    {3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
    {5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1},
    {8, 4, 5, 8, 5, 3, 3, 5, 1, -1},
    {0, 4, 5, 1, 0, 5, -1},
    {8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1},
    {9, 4, 5, -1},
    {4, 11, 7, 4, 9, 11, 9, 10, 11, -1},
    {0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1},
    {1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1},
    {3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
    {4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1},
    {9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
    {11, 7, 4, 11, 4, 2, 2, 4, 0, -1},
    {11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1},
    {2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1},
    {9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
    {3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
    {1, 10, 2, 8, 7, 4, -1},
    {4, 9, 1, 4, 1, 7, 7, 1, 3, -1},
    {4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1},
    {4, 0, 3, 7, 4, 3, -1},
    {4, 8, 7, -1},
    {9, 10, 8, 10, 11, 8, -1},
    {3, 0, 9, 3, 9, 11, 11, 9, 10, -1},
    {0, 1, 10, 0, 10, 8, 8, 10, 11, -1},
    {3, 1, 10, 11, 3, 10, -1},
    {1, 2, 11, 1, 11, 9, 9, 11, 8, -1},
    {3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1},
    {0, 2, 11, 8, 0, 11, -1},
    {3, 2, 11, -1},
    {2, 3, 8, 2, 8, 10, 10, 8, 9, -1},
    {9, 10, 2, 0, 9, 2, -1},
    {2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1},
    {1, 10, 2, -1},
    {1, 3, 8, 9, 1, 8, -1},
    {0, 9, 1, -1},
    {0, 3, 8, -1},
    {-1}
};

// ============================================================================
// VoxelChunk Implementation
// ============================================================================

VoxelChunk::VoxelChunk(const glm::ivec3& position) : m_position(position) {
    // Initialize all voxels to air
    for (auto& voxel : m_voxels) {
        voxel.density = 1.0f;  // Positive = outside = air
        voxel.material = VoxelMaterial::Air;
    }
}

Voxel& VoxelChunk::GetVoxel(int x, int y, int z) {
    return m_voxels[GetIndex(x, y, z)];
}

const Voxel& VoxelChunk::GetVoxel(int x, int y, int z) const {
    return m_voxels[GetIndex(x, y, z)];
}

void VoxelChunk::SetVoxel(int x, int y, int z, const Voxel& voxel) {
    m_voxels[GetIndex(x, y, z)] = voxel;
    m_needsMeshRebuild = true;
}

bool VoxelChunk::IsEmpty() const {
    for (const auto& voxel : m_voxels) {
        if (voxel.IsSolid()) return false;
    }
    return true;
}

bool VoxelChunk::IsSolid() const {
    for (const auto& voxel : m_voxels) {
        if (!voxel.IsSolid()) return false;
    }
    return true;
}

// ============================================================================
// MarchingCubes Implementation
// ============================================================================

MarchingCubes::MarchingCubes() = default;

void MarchingCubes::GenerateMesh(VoxelChunk& chunk, float isoLevel) {
    chunk.vertices.clear();
    chunk.normals.clear();
    chunk.uvs.clear();
    chunk.colors.clear();
    chunk.indices.clear();

    // Process each cell in the chunk
    for (int z = 0; z < VoxelChunk::SIZE - 1; z++) {
        for (int y = 0; y < VoxelChunk::SIZE - 1; y++) {
            for (int x = 0; x < VoxelChunk::SIZE - 1; x++) {
                // Get density values at cube corners
                float d[8];
                d[0] = chunk.GetVoxel(x, y, z).density;
                d[1] = chunk.GetVoxel(x + 1, y, z).density;
                d[2] = chunk.GetVoxel(x + 1, y, z + 1).density;
                d[3] = chunk.GetVoxel(x, y, z + 1).density;
                d[4] = chunk.GetVoxel(x, y + 1, z).density;
                d[5] = chunk.GetVoxel(x + 1, y + 1, z).density;
                d[6] = chunk.GetVoxel(x + 1, y + 1, z + 1).density;
                d[7] = chunk.GetVoxel(x, y + 1, z + 1).density;

                // Calculate cube index
                int cubeIndex = 0;
                if (d[0] < isoLevel) cubeIndex |= 1;
                if (d[1] < isoLevel) cubeIndex |= 2;
                if (d[2] < isoLevel) cubeIndex |= 4;
                if (d[3] < isoLevel) cubeIndex |= 8;
                if (d[4] < isoLevel) cubeIndex |= 16;
                if (d[5] < isoLevel) cubeIndex |= 32;
                if (d[6] < isoLevel) cubeIndex |= 64;
                if (d[7] < isoLevel) cubeIndex |= 128;

                // Skip if cube is entirely inside or outside
                if (edgeTable[cubeIndex] == 0) continue;

                // Corner positions
                glm::vec3 p[8] = {
                    glm::vec3(x, y, z),
                    glm::vec3(x + 1, y, z),
                    glm::vec3(x + 1, y, z + 1),
                    glm::vec3(x, y, z + 1),
                    glm::vec3(x, y + 1, z),
                    glm::vec3(x + 1, y + 1, z),
                    glm::vec3(x + 1, y + 1, z + 1),
                    glm::vec3(x, y + 1, z + 1)
                };

                // Calculate vertex positions on edges
                glm::vec3 vertList[12];

                if (edgeTable[cubeIndex] & 1)
                    vertList[0] = InterpolateVertex(p[0], p[1], d[0], d[1], isoLevel);
                if (edgeTable[cubeIndex] & 2)
                    vertList[1] = InterpolateVertex(p[1], p[2], d[1], d[2], isoLevel);
                if (edgeTable[cubeIndex] & 4)
                    vertList[2] = InterpolateVertex(p[2], p[3], d[2], d[3], isoLevel);
                if (edgeTable[cubeIndex] & 8)
                    vertList[3] = InterpolateVertex(p[3], p[0], d[3], d[0], isoLevel);
                if (edgeTable[cubeIndex] & 16)
                    vertList[4] = InterpolateVertex(p[4], p[5], d[4], d[5], isoLevel);
                if (edgeTable[cubeIndex] & 32)
                    vertList[5] = InterpolateVertex(p[5], p[6], d[5], d[6], isoLevel);
                if (edgeTable[cubeIndex] & 64)
                    vertList[6] = InterpolateVertex(p[6], p[7], d[6], d[7], isoLevel);
                if (edgeTable[cubeIndex] & 128)
                    vertList[7] = InterpolateVertex(p[7], p[4], d[7], d[4], isoLevel);
                if (edgeTable[cubeIndex] & 256)
                    vertList[8] = InterpolateVertex(p[0], p[4], d[0], d[4], isoLevel);
                if (edgeTable[cubeIndex] & 512)
                    vertList[9] = InterpolateVertex(p[1], p[5], d[1], d[5], isoLevel);
                if (edgeTable[cubeIndex] & 1024)
                    vertList[10] = InterpolateVertex(p[2], p[6], d[2], d[6], isoLevel);
                if (edgeTable[cubeIndex] & 2048)
                    vertList[11] = InterpolateVertex(p[3], p[7], d[3], d[7], isoLevel);

                // Get color from dominant voxel
                glm::vec3 color = chunk.GetVoxel(x, y, z).color;

                // Generate triangles
                for (int i = 0; triTable[cubeIndex][i] != -1; i += 3) {
                    uint32_t baseIndex = static_cast<uint32_t>(chunk.vertices.size());

                    glm::vec3 v0 = vertList[triTable[cubeIndex][i]];
                    glm::vec3 v1 = vertList[triTable[cubeIndex][i + 1]];
                    glm::vec3 v2 = vertList[triTable[cubeIndex][i + 2]];

                    chunk.vertices.push_back(v0);
                    chunk.vertices.push_back(v1);
                    chunk.vertices.push_back(v2);

                    // Calculate normal
                    glm::vec3 normal;
                    if (m_smoothNormals) {
                        // Use gradient for smooth normals
                        normal = CalculateNormal(chunk, (v0 + v1 + v2) / 3.0f);
                    } else {
                        // Flat shading
                        glm::vec3 edge1 = v1 - v0;
                        glm::vec3 edge2 = v2 - v0;
                        normal = glm::normalize(glm::cross(edge1, edge2));
                    }

                    chunk.normals.push_back(normal);
                    chunk.normals.push_back(normal);
                    chunk.normals.push_back(normal);

                    // UVs based on world position
                    chunk.uvs.push_back(glm::vec2(v0.x, v0.z));
                    chunk.uvs.push_back(glm::vec2(v1.x, v1.z));
                    chunk.uvs.push_back(glm::vec2(v2.x, v2.z));

                    // Colors
                    chunk.colors.push_back(color);
                    chunk.colors.push_back(color);
                    chunk.colors.push_back(color);

                    chunk.indices.push_back(baseIndex);
                    chunk.indices.push_back(baseIndex + 1);
                    chunk.indices.push_back(baseIndex + 2);
                }
            }
        }
    }

    chunk.SetNeedsMeshRebuild(false);
}

glm::vec3 MarchingCubes::InterpolateVertex(const glm::vec3& p1, const glm::vec3& p2, float v1, float v2, float isoLevel) {
    if (!m_useInterpolation) {
        return (p1 + p2) * 0.5f;
    }

    if (std::abs(isoLevel - v1) < 0.00001f) return p1;
    if (std::abs(isoLevel - v2) < 0.00001f) return p2;
    if (std::abs(v1 - v2) < 0.00001f) return p1;

    float t = (isoLevel - v1) / (v2 - v1);
    return p1 + t * (p2 - p1);
}

glm::vec3 MarchingCubes::CalculateNormal(VoxelChunk& chunk, const glm::vec3& pos) {
    float h = 0.5f;

    auto sample = [&chunk](int x, int y, int z) -> float {
        x = std::clamp(x, 0, VoxelChunk::SIZE - 1);
        y = std::clamp(y, 0, VoxelChunk::SIZE - 1);
        z = std::clamp(z, 0, VoxelChunk::SIZE - 1);
        return chunk.GetVoxel(x, y, z).density;
    };

    int ix = static_cast<int>(pos.x);
    int iy = static_cast<int>(pos.y);
    int iz = static_cast<int>(pos.z);

    glm::vec3 normal;
    normal.x = sample(ix + 1, iy, iz) - sample(ix - 1, iy, iz);
    normal.y = sample(ix, iy + 1, iz) - sample(ix, iy - 1, iz);
    normal.z = sample(ix, iy, iz + 1) - sample(ix, iy, iz - 1);

    float len = glm::length(normal);
    if (len > 0.0001f) {
        return normal / len;
    }
    return glm::vec3(0, 1, 0);
}

// ============================================================================
// SDFBrush Implementation
// ============================================================================

float SDFBrush::Evaluate(const glm::vec3& worldPos) const {
    // Transform to local space
    glm::vec3 localPos = glm::inverse(rotation) * (worldPos - position);

    float dist = 0.0f;

    switch (shape) {
        case SDFBrushShape::Sphere:
            dist = glm::length(localPos) - size.x;
            break;

        case SDFBrushShape::Box: {
            glm::vec3 d = glm::abs(localPos) - size;
            dist = glm::length(glm::max(d, glm::vec3(0.0f))) + std::min(std::max(d.x, std::max(d.y, d.z)), 0.0f);
            break;
        }

        case SDFBrushShape::Cylinder: {
            glm::vec2 d = glm::vec2(glm::length(glm::vec2(localPos.x, localPos.z)) - size.x, std::abs(localPos.y) - size.y);
            dist = std::min(std::max(d.x, d.y), 0.0f) + glm::length(glm::max(d, glm::vec2(0.0f)));
            break;
        }

        case SDFBrushShape::Capsule: {
            glm::vec3 a(0, -size.y, 0);
            glm::vec3 b(0, size.y, 0);
            glm::vec3 pa = localPos - a;
            glm::vec3 ba = b - a;
            float h = std::clamp(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0f, 1.0f);
            dist = glm::length(pa - ba * h) - size.x;
            break;
        }

        case SDFBrushShape::Cone: {
            glm::vec2 q = glm::vec2(glm::length(glm::vec2(localPos.x, localPos.z)), localPos.y);
            glm::vec2 tip = glm::vec2(0, size.y);
            glm::vec2 base = glm::vec2(size.x, 0);
            glm::vec2 e = base - tip;
            glm::vec2 w = q - tip;
            glm::vec2 d1 = w - e * std::clamp(glm::dot(w, e) / glm::dot(e, e), 0.0f, 1.0f);
            glm::vec2 d2 = w - glm::vec2(std::clamp(w.x, 0.0f, base.x), 0.0f);
            float s = (e.x * w.y - e.y * w.x < 0.0f) ? -1.0f : 1.0f;
            dist = s * std::sqrt(std::min(glm::dot(d1, d1), glm::dot(d2, d2)));
            break;
        }

        case SDFBrushShape::Torus: {
            glm::vec2 q = glm::vec2(glm::length(glm::vec2(localPos.x, localPos.z)) - size.x, localPos.y);
            dist = glm::length(q) - size.y;
            break;
        }

        case SDFBrushShape::Custom:
            if (customSDF) {
                dist = customSDF(localPos);
            }
            break;
    }

    return dist;
}

// ============================================================================
// VoxelTerrain Implementation
// ============================================================================

VoxelTerrain::VoxelTerrain() = default;
VoxelTerrain::~VoxelTerrain() = default;

void VoxelTerrain::Initialize(const Config& config) {
    m_config = config;
    m_initialized = true;
}

void VoxelTerrain::Update(const glm::vec3& cameraPosition, float deltaTime) {
    UpdateLoadedChunks(cameraPosition);
    ProcessMeshQueue();
}

// ============================================================================
// Voxel Access
// ============================================================================

Voxel VoxelTerrain::GetVoxel(const glm::vec3& worldPos) const {
    glm::ivec3 chunkPos = WorldToChunk(worldPos);
    const VoxelChunk* chunk = GetChunk(chunkPos);

    if (!chunk) {
        Voxel air;
        air.density = 1.0f;
        return air;
    }

    glm::ivec3 local = WorldToLocal(worldPos, chunkPos);
    return chunk->GetVoxel(local.x, local.y, local.z);
}

Voxel VoxelTerrain::GetVoxel(int x, int y, int z) const {
    return GetVoxel(glm::vec3(x, y, z) * m_config.voxelSize);
}

void VoxelTerrain::SetVoxel(const glm::vec3& worldPos, const Voxel& voxel) {
    glm::ivec3 chunkPos = WorldToChunk(worldPos);
    VoxelChunk* chunk = GetChunk(chunkPos);

    if (!chunk) {
        chunk = CreateChunk(chunkPos);
    }

    glm::ivec3 local = WorldToLocal(worldPos, chunkPos);
    chunk->SetVoxel(local.x, local.y, local.z, voxel);
}

void VoxelTerrain::SetVoxel(int x, int y, int z, const Voxel& voxel) {
    SetVoxel(glm::vec3(x, y, z) * m_config.voxelSize, voxel);
}

float VoxelTerrain::SampleDensity(const glm::vec3& worldPos) const {
    // Trilinear interpolation
    glm::vec3 voxelPos = worldPos / m_config.voxelSize;
    glm::ivec3 basePos = glm::floor(voxelPos);
    glm::vec3 frac = voxelPos - glm::vec3(basePos);

    float d000 = GetVoxel(basePos.x, basePos.y, basePos.z).density;
    float d100 = GetVoxel(basePos.x + 1, basePos.y, basePos.z).density;
    float d010 = GetVoxel(basePos.x, basePos.y + 1, basePos.z).density;
    float d110 = GetVoxel(basePos.x + 1, basePos.y + 1, basePos.z).density;
    float d001 = GetVoxel(basePos.x, basePos.y, basePos.z + 1).density;
    float d101 = GetVoxel(basePos.x + 1, basePos.y, basePos.z + 1).density;
    float d011 = GetVoxel(basePos.x, basePos.y + 1, basePos.z + 1).density;
    float d111 = GetVoxel(basePos.x + 1, basePos.y + 1, basePos.z + 1).density;

    float d00 = glm::mix(d000, d100, frac.x);
    float d10 = glm::mix(d010, d110, frac.x);
    float d01 = glm::mix(d001, d101, frac.x);
    float d11 = glm::mix(d011, d111, frac.x);

    float d0 = glm::mix(d00, d10, frac.y);
    float d1 = glm::mix(d01, d11, frac.y);

    return glm::mix(d0, d1, frac.z);
}

float VoxelTerrain::GetHeightAt(float x, float z) const {
    // Raycast down from high
    glm::vec3 origin(x, 1000.0f, z);
    glm::vec3 direction(0, -1, 0);
    glm::vec3 hitPoint, hitNormal;

    if (Raycast(origin, direction, 2000.0f, hitPoint, hitNormal)) {
        return hitPoint.y;
    }

    return 0.0f;
}

glm::vec3 VoxelTerrain::GetNormalAt(const glm::vec3& worldPos) const {
    float h = m_config.voxelSize * 0.5f;
    glm::vec3 normal;
    normal.x = SampleDensity(worldPos + glm::vec3(h, 0, 0)) - SampleDensity(worldPos - glm::vec3(h, 0, 0));
    normal.y = SampleDensity(worldPos + glm::vec3(0, h, 0)) - SampleDensity(worldPos - glm::vec3(0, h, 0));
    normal.z = SampleDensity(worldPos + glm::vec3(0, 0, h)) - SampleDensity(worldPos - glm::vec3(0, 0, h));
    return glm::normalize(normal);
}

// ============================================================================
// SDF Modifications
// ============================================================================

TerrainModification VoxelTerrain::ApplyBrush(const SDFBrush& brush) {
    TerrainModification mod;

    // Calculate affected region
    glm::vec3 brushExtent = brush.size + glm::vec3(brush.smoothness);
    glm::vec3 minWorld = brush.position - brushExtent;
    glm::vec3 maxWorld = brush.position + brushExtent;

    glm::ivec3 minChunk = WorldToChunk(minWorld);
    glm::ivec3 maxChunk = WorldToChunk(maxWorld);

    // Process affected chunks
    for (int cz = minChunk.z; cz <= maxChunk.z; cz++) {
        for (int cy = minChunk.y; cy <= maxChunk.y; cy++) {
            for (int cx = minChunk.x; cx <= maxChunk.x; cx++) {
                glm::ivec3 chunkPos(cx, cy, cz);
                VoxelChunk* chunk = GetChunk(chunkPos);
                if (!chunk) {
                    chunk = CreateChunk(chunkPos);
                }

                mod.chunkPos = chunkPos;

                // Process voxels in chunk
                for (int z = 0; z < VoxelChunk::SIZE; z++) {
                    for (int y = 0; y < VoxelChunk::SIZE; y++) {
                        for (int x = 0; x < VoxelChunk::SIZE; x++) {
                            glm::vec3 worldPos = LocalToWorld(glm::ivec3(x, y, z), chunkPos);

                            float brushDist = brush.Evaluate(worldPos);

                            // Check if within brush influence
                            if (std::abs(brushDist) > brush.size.x + brush.smoothness) continue;

                            Voxel& voxel = chunk->GetVoxel(x, y, z);
                            float originalDensity = voxel.density;

                            // Store original for undo
                            mod.originalVoxels.push_back({glm::ivec3(x, y, z), voxel});

                            // Apply SDF operation
                            float newDensity = originalDensity;

                            switch (brush.operation) {
                                case SDFOperation::Union:
                                    newDensity = SDFUnion(originalDensity, brushDist);
                                    break;
                                case SDFOperation::Subtract:
                                    newDensity = SDFSubtract(originalDensity, brushDist);
                                    break;
                                case SDFOperation::Intersect:
                                    newDensity = SDFIntersect(originalDensity, brushDist);
                                    break;
                                case SDFOperation::SmoothUnion:
                                    newDensity = SDFSmoothUnion(originalDensity, brushDist, brush.smoothness);
                                    break;
                                case SDFOperation::SmoothSubtract:
                                    newDensity = SDFSmoothSubtract(originalDensity, brushDist, brush.smoothness);
                                    break;
                                case SDFOperation::SmoothIntersect:
                                    newDensity = SDFSmoothIntersect(originalDensity, brushDist, brush.smoothness);
                                    break;
                            }

                            voxel.density = newDensity;

                            // Update material if adding
                            if (newDensity < 0 && (brush.operation == SDFOperation::Union ||
                                brush.operation == SDFOperation::SmoothUnion)) {
                                voxel.material = brush.material;
                                voxel.color = brush.color;
                            }

                            mod.newVoxels.push_back({glm::ivec3(x, y, z), voxel});
                        }
                    }
                }

                chunk->SetNeedsMeshRebuild(true);
            }
        }
    }

    // Add to undo stack
    m_undoStack.push_back(mod);
    if (m_undoStack.size() > MAX_UNDO_HISTORY) {
        m_undoStack.erase(m_undoStack.begin());
    }
    m_redoStack.clear();

    if (OnTerrainModified) OnTerrainModified(mod);

    return mod;
}

TerrainModification VoxelTerrain::ApplySphere(const glm::vec3& center, float radius, SDFOperation op, VoxelMaterial material) {
    SDFBrush brush;
    brush.shape = SDFBrushShape::Sphere;
    brush.operation = op;
    brush.position = center;
    brush.size = glm::vec3(radius);
    brush.material = material;
    return ApplyBrush(brush);
}

TerrainModification VoxelTerrain::ApplyBox(const glm::vec3& center, const glm::vec3& size, const glm::quat& rotation, SDFOperation op, VoxelMaterial material) {
    SDFBrush brush;
    brush.shape = SDFBrushShape::Box;
    brush.operation = op;
    brush.position = center;
    brush.size = size;
    brush.rotation = rotation;
    brush.material = material;
    return ApplyBrush(brush);
}

TerrainModification VoxelTerrain::ApplyCylinder(const glm::vec3& base, float height, float radius, SDFOperation op, VoxelMaterial material) {
    SDFBrush brush;
    brush.shape = SDFBrushShape::Cylinder;
    brush.operation = op;
    brush.position = base + glm::vec3(0, height * 0.5f, 0);
    brush.size = glm::vec3(radius, height * 0.5f, radius);
    brush.material = material;
    return ApplyBrush(brush);
}

TerrainModification VoxelTerrain::DigTunnel(const glm::vec3& start, const glm::vec3& end, float radius, float smoothness) {
    SDFBrush brush;
    brush.shape = SDFBrushShape::Capsule;
    brush.operation = SDFOperation::SmoothSubtract;
    brush.position = (start + end) * 0.5f;
    brush.size = glm::vec3(radius, glm::length(end - start) * 0.5f, radius);
    brush.smoothness = smoothness;

    // Orient capsule along tunnel direction
    glm::vec3 dir = glm::normalize(end - start);
    if (std::abs(dir.y) < 0.999f) {
        // Construct quaternion to rotate from up (0,1,0) to dir
        glm::vec3 up(0, 1, 0);
        glm::vec3 axis = glm::cross(up, dir);
        float angle = std::acos(glm::clamp(glm::dot(up, dir), -1.0f, 1.0f));
        if (glm::length(axis) > 0.001f) {
            brush.rotation = glm::angleAxis(angle, glm::normalize(axis));
        }
    }

    return ApplyBrush(brush);
}

TerrainModification VoxelTerrain::CreateCave(const glm::vec3& center, const glm::vec3& size, float noiseScale, int seed) {
    SDFBrush brush;
    brush.shape = SDFBrushShape::Custom;
    brush.operation = SDFOperation::SmoothSubtract;
    brush.position = center;
    brush.size = size;
    brush.smoothness = 0.5f;

    // Custom SDF with noise
    brush.customSDF = [this, size, noiseScale, seed](const glm::vec3& p) -> float {
        float baseDist = glm::length(p / size) - 1.0f;
        float noise = FBM(p.x * noiseScale, p.y * noiseScale, p.z * noiseScale, 4, 0.5f, 2.0f, seed);
        return baseDist + noise * 0.3f;
    };

    return ApplyBrush(brush);
}

void VoxelTerrain::SmoothTerrain(const glm::vec3& center, float radius, float strength) {
    // Gather voxels in radius and smooth them
    glm::ivec3 minChunk = WorldToChunk(center - glm::vec3(radius));
    glm::ivec3 maxChunk = WorldToChunk(center + glm::vec3(radius));

    for (int cz = minChunk.z; cz <= maxChunk.z; cz++) {
        for (int cy = minChunk.y; cy <= maxChunk.y; cy++) {
            for (int cx = minChunk.x; cx <= maxChunk.x; cx++) {
                glm::ivec3 chunkPos(cx, cy, cz);
                VoxelChunk* chunk = GetChunk(chunkPos);
                if (!chunk) continue;

                for (int z = 1; z < VoxelChunk::SIZE - 1; z++) {
                    for (int y = 1; y < VoxelChunk::SIZE - 1; y++) {
                        for (int x = 1; x < VoxelChunk::SIZE - 1; x++) {
                            glm::vec3 worldPos = LocalToWorld(glm::ivec3(x, y, z), chunkPos);
                            float dist = glm::length(worldPos - center);

                            if (dist > radius) continue;

                            float weight = 1.0f - (dist / radius);
                            weight *= strength;

                            // Average neighbors
                            float avg = 0.0f;
                            avg += chunk->GetVoxel(x - 1, y, z).density;
                            avg += chunk->GetVoxel(x + 1, y, z).density;
                            avg += chunk->GetVoxel(x, y - 1, z).density;
                            avg += chunk->GetVoxel(x, y + 1, z).density;
                            avg += chunk->GetVoxel(x, y, z - 1).density;
                            avg += chunk->GetVoxel(x, y, z + 1).density;
                            avg /= 6.0f;

                            Voxel& voxel = chunk->GetVoxel(x, y, z);
                            voxel.density = glm::mix(voxel.density, avg, weight);
                        }
                    }
                }

                chunk->SetNeedsMeshRebuild(true);
            }
        }
    }
}

void VoxelTerrain::FlattenTerrain(const glm::vec3& center, float radius, float targetHeight, float strength) {
    glm::ivec3 minChunk = WorldToChunk(center - glm::vec3(radius, 100, radius));
    glm::ivec3 maxChunk = WorldToChunk(center + glm::vec3(radius, 100, radius));

    for (int cz = minChunk.z; cz <= maxChunk.z; cz++) {
        for (int cy = minChunk.y; cy <= maxChunk.y; cy++) {
            for (int cx = minChunk.x; cx <= maxChunk.x; cx++) {
                glm::ivec3 chunkPos(cx, cy, cz);
                VoxelChunk* chunk = GetChunk(chunkPos);
                if (!chunk) chunk = CreateChunk(chunkPos);

                for (int z = 0; z < VoxelChunk::SIZE; z++) {
                    for (int y = 0; y < VoxelChunk::SIZE; y++) {
                        for (int x = 0; x < VoxelChunk::SIZE; x++) {
                            glm::vec3 worldPos = LocalToWorld(glm::ivec3(x, y, z), chunkPos);
                            float dist2D = glm::length(glm::vec2(worldPos.x - center.x, worldPos.z - center.z));

                            if (dist2D > radius) continue;

                            float weight = 1.0f - (dist2D / radius);
                            weight *= strength;

                            // Target density based on height
                            float targetDensity = worldPos.y - targetHeight;

                            Voxel& voxel = chunk->GetVoxel(x, y, z);
                            voxel.density = glm::mix(voxel.density, targetDensity, weight);
                        }
                    }
                }

                chunk->SetNeedsMeshRebuild(true);
            }
        }
    }
}

void VoxelTerrain::PaintMaterial(const glm::vec3& center, float radius, VoxelMaterial material, const glm::vec3& color) {
    glm::ivec3 minChunk = WorldToChunk(center - glm::vec3(radius));
    glm::ivec3 maxChunk = WorldToChunk(center + glm::vec3(radius));

    for (int cz = minChunk.z; cz <= maxChunk.z; cz++) {
        for (int cy = minChunk.y; cy <= maxChunk.y; cy++) {
            for (int cx = minChunk.x; cx <= maxChunk.x; cx++) {
                glm::ivec3 chunkPos(cx, cy, cz);
                VoxelChunk* chunk = GetChunk(chunkPos);
                if (!chunk) continue;

                for (int z = 0; z < VoxelChunk::SIZE; z++) {
                    for (int y = 0; y < VoxelChunk::SIZE; y++) {
                        for (int x = 0; x < VoxelChunk::SIZE; x++) {
                            glm::vec3 worldPos = LocalToWorld(glm::ivec3(x, y, z), chunkPos);
                            float dist = glm::length(worldPos - center);

                            if (dist > radius) continue;

                            Voxel& voxel = chunk->GetVoxel(x, y, z);
                            if (voxel.IsSolid()) {
                                voxel.material = material;
                                voxel.color = color;
                            }
                        }
                    }
                }

                chunk->SetNeedsMeshRebuild(true);
            }
        }
    }
}

// ============================================================================
// Chunk Management
// ============================================================================

VoxelChunk* VoxelTerrain::GetChunk(const glm::ivec3& chunkPos) {
    uint64_t key = GetChunkKey(chunkPos);
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    auto it = m_chunks.find(key);
    return it != m_chunks.end() ? it->second.get() : nullptr;
}

const VoxelChunk* VoxelTerrain::GetChunk(const glm::ivec3& chunkPos) const {
    uint64_t key = GetChunkKey(chunkPos);
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    auto it = m_chunks.find(key);
    return it != m_chunks.end() ? it->second.get() : nullptr;
}

VoxelChunk* VoxelTerrain::CreateChunk(const glm::ivec3& chunkPos) {
    uint64_t key = GetChunkKey(chunkPos);

    std::lock_guard<std::mutex> lock(m_chunkMutex);
    auto& chunk = m_chunks[key];
    if (!chunk) {
        chunk = std::make_unique<VoxelChunk>(chunkPos);

        // Generate terrain if generator is set
        if (m_terrainGenerator) {
            for (int z = 0; z < VoxelChunk::SIZE; z++) {
                for (int y = 0; y < VoxelChunk::SIZE; y++) {
                    for (int x = 0; x < VoxelChunk::SIZE; x++) {
                        glm::vec3 worldPos = LocalToWorld(glm::ivec3(x, y, z), chunkPos);
                        Voxel voxel;
                        voxel.density = m_terrainGenerator(worldPos);
                        if (voxel.IsSolid()) {
                            voxel.material = VoxelMaterial::Dirt;
                            voxel.color = glm::vec3(0.5f, 0.4f, 0.3f);
                        }
                        chunk->SetVoxel(x, y, z, voxel);
                    }
                }
            }
        }

        if (OnChunkCreated) OnChunkCreated(chunk.get());
    }

    return chunk.get();
}

void VoxelTerrain::RemoveChunk(const glm::ivec3& chunkPos) {
    uint64_t key = GetChunkKey(chunkPos);

    std::lock_guard<std::mutex> lock(m_chunkMutex);
    m_chunks.erase(key);

    if (OnChunkRemoved) OnChunkRemoved(chunkPos);
}

void VoxelTerrain::RebuildMeshes(const glm::vec3& minWorld, const glm::vec3& maxWorld) {
    glm::ivec3 minChunk = WorldToChunk(minWorld);
    glm::ivec3 maxChunk = WorldToChunk(maxWorld);

    for (int z = minChunk.z; z <= maxChunk.z; z++) {
        for (int y = minChunk.y; y <= maxChunk.y; y++) {
            for (int x = minChunk.x; x <= maxChunk.x; x++) {
                if (VoxelChunk* chunk = GetChunk(glm::ivec3(x, y, z))) {
                    chunk->SetNeedsMeshRebuild(true);
                }
            }
        }
    }
}

void VoxelTerrain::RebuildAllMeshes() {
    std::lock_guard<std::mutex> lock(m_chunkMutex);
    for (auto& [key, chunk] : m_chunks) {
        chunk->SetNeedsMeshRebuild(true);
    }
}

// ============================================================================
// Terrain Generation
// ============================================================================

void VoxelTerrain::GenerateTerrain(int seed, float scale, int octaves, float persistence, float lacunarity) {
    m_terrainGenerator = [this, seed, scale, octaves, persistence, lacunarity](const glm::vec3& pos) -> float {
        float height = FBM(pos.x * scale, pos.z * scale, 0.0f, octaves, persistence, lacunarity, seed) * 50.0f;
        return pos.y - height;
    };
}

void VoxelTerrain::GenerateFlatTerrain(float height) {
    m_terrainGenerator = [height](const glm::vec3& pos) -> float {
        return pos.y - height;
    };
}

// ============================================================================
// Undo/Redo
// ============================================================================

void VoxelTerrain::Undo() {
    if (m_undoStack.empty()) return;

    TerrainModification& mod = m_undoStack.back();

    // Restore original voxels
    VoxelChunk* chunk = GetChunk(mod.chunkPos);
    if (chunk) {
        for (const auto& [pos, voxel] : mod.originalVoxels) {
            chunk->SetVoxel(pos.x, pos.y, pos.z, voxel);
        }
        chunk->SetNeedsMeshRebuild(true);
    }

    m_redoStack.push_back(std::move(mod));
    m_undoStack.pop_back();
}

void VoxelTerrain::Redo() {
    if (m_redoStack.empty()) return;

    TerrainModification& mod = m_redoStack.back();

    // Apply new voxels
    VoxelChunk* chunk = GetChunk(mod.chunkPos);
    if (chunk) {
        for (const auto& [pos, voxel] : mod.newVoxels) {
            chunk->SetVoxel(pos.x, pos.y, pos.z, voxel);
        }
        chunk->SetNeedsMeshRebuild(true);
    }

    m_undoStack.push_back(std::move(mod));
    m_redoStack.pop_back();
}

// ============================================================================
// Raycasting
// ============================================================================

bool VoxelTerrain::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, glm::vec3& hitPoint, glm::vec3& hitNormal) const {
    // DDA-style raymarching
    glm::vec3 pos = origin;
    glm::vec3 dir = glm::normalize(direction);
    float stepSize = m_config.voxelSize * 0.5f;

    for (float t = 0; t < maxDistance; t += stepSize) {
        pos = origin + dir * t;
        float density = SampleDensity(pos);

        if (density < 0.0f) {
            // Refine hit point with binary search
            float lo = t - stepSize;
            float hi = t;

            for (int i = 0; i < 8; i++) {
                float mid = (lo + hi) * 0.5f;
                pos = origin + dir * mid;
                density = SampleDensity(pos);

                if (density < 0.0f) {
                    hi = mid;
                } else {
                    lo = mid;
                }
            }

            hitPoint = origin + dir * hi;
            hitNormal = GetNormalAt(hitPoint);
            return true;
        }
    }

    return false;
}

// ============================================================================
// SDF Functions
// ============================================================================

float VoxelTerrain::SDFSphere(const glm::vec3& p, const glm::vec3& center, float radius) const {
    return glm::length(p - center) - radius;
}

float VoxelTerrain::SDFBox(const glm::vec3& p, const glm::vec3& center, const glm::vec3& size, const glm::quat& rotation) const {
    glm::vec3 local = glm::inverse(rotation) * (p - center);
    glm::vec3 d = glm::abs(local) - size;
    return glm::length(glm::max(d, glm::vec3(0.0f))) + std::min(std::max(d.x, std::max(d.y, d.z)), 0.0f);
}

float VoxelTerrain::SDFCylinder(const glm::vec3& p, const glm::vec3& base, float height, float radius) const {
    glm::vec3 local = p - base - glm::vec3(0, height * 0.5f, 0);
    glm::vec2 d = glm::vec2(glm::length(glm::vec2(local.x, local.z)) - radius, std::abs(local.y) - height * 0.5f);
    return std::min(std::max(d.x, d.y), 0.0f) + glm::length(glm::max(d, glm::vec2(0.0f)));
}

float VoxelTerrain::SDFCapsule(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, float radius) const {
    glm::vec3 pa = p - a;
    glm::vec3 ba = b - a;
    float h = std::clamp(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0f, 1.0f);
    return glm::length(pa - ba * h) - radius;
}

float VoxelTerrain::SDFUnion(float d1, float d2) const {
    return std::min(d1, d2);
}

float VoxelTerrain::SDFSubtract(float d1, float d2) const {
    return std::max(d1, -d2);
}

float VoxelTerrain::SDFIntersect(float d1, float d2) const {
    return std::max(d1, d2);
}

float VoxelTerrain::SDFSmoothUnion(float d1, float d2, float k) const {
    float h = std::clamp(0.5f + 0.5f * (d2 - d1) / k, 0.0f, 1.0f);
    return glm::mix(d2, d1, h) - k * h * (1.0f - h);
}

float VoxelTerrain::SDFSmoothSubtract(float d1, float d2, float k) const {
    float h = std::clamp(0.5f - 0.5f * (d2 + d1) / k, 0.0f, 1.0f);
    return glm::mix(d1, -d2, h) + k * h * (1.0f - h);
}

float VoxelTerrain::SDFSmoothIntersect(float d1, float d2, float k) const {
    float h = std::clamp(0.5f - 0.5f * (d2 - d1) / k, 0.0f, 1.0f);
    return glm::mix(d2, d1, h) + k * h * (1.0f - h);
}

// ============================================================================
// Private Methods
// ============================================================================

void VoxelTerrain::UpdateLoadedChunks(const glm::vec3& cameraPosition) {
    glm::ivec3 cameraChunk = WorldToChunk(cameraPosition);

    // Unload far chunks
    std::vector<uint64_t> toRemove;
    {
        std::lock_guard<std::mutex> lock(m_chunkMutex);
        for (auto& [key, chunk] : m_chunks) {
            glm::ivec3 dist = chunk->GetPosition() - cameraChunk;
            if (std::abs(dist.x) > m_config.viewDistance ||
                std::abs(dist.y) > m_config.viewDistance ||
                std::abs(dist.z) > m_config.viewDistance) {
                toRemove.push_back(key);
            }
        }
    }

    for (uint64_t key : toRemove) {
        std::lock_guard<std::mutex> lock(m_chunkMutex);
        m_chunks.erase(key);
    }

    // Queue chunks needing mesh rebuild
    m_meshQueue.clear();
    {
        std::lock_guard<std::mutex> lock(m_chunkMutex);
        for (auto& [key, chunk] : m_chunks) {
            if (chunk->NeedsMeshRebuild()) {
                m_meshQueue.push_back(chunk->GetPosition());
            }
        }
    }
}

void VoxelTerrain::ProcessMeshQueue() {
    int processed = 0;

    for (const auto& chunkPos : m_meshQueue) {
        if (processed >= m_config.maxMeshesPerFrame) break;

        VoxelChunk* chunk = GetChunk(chunkPos);
        if (chunk && chunk->NeedsMeshRebuild()) {
            m_marchingCubes.GenerateMesh(*chunk);
            if (OnChunkMeshUpdated) OnChunkMeshUpdated(chunk);
            processed++;
        }
    }
}

float VoxelTerrain::PerlinNoise3D(float x, float y, float z, int seed) const {
    // Simple hash-based noise
    auto fade = [](float t) { return t * t * t * (t * (t * 6 - 15) + 10); };
    auto lerp = [](float a, float b, float t) { return a + t * (b - a); };

    auto hash = [seed](int x, int y, int z) -> float {
        int n = x + y * 57 + z * 131 + seed * 1000;
        n = (n << 13) ^ n;
        return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
    };

    int xi = static_cast<int>(std::floor(x));
    int yi = static_cast<int>(std::floor(y));
    int zi = static_cast<int>(std::floor(z));

    float xf = x - xi;
    float yf = y - yi;
    float zf = z - zi;

    float u = fade(xf);
    float v = fade(yf);
    float w = fade(zf);

    return lerp(
        lerp(lerp(hash(xi, yi, zi), hash(xi + 1, yi, zi), u),
             lerp(hash(xi, yi + 1, zi), hash(xi + 1, yi + 1, zi), u), v),
        lerp(lerp(hash(xi, yi, zi + 1), hash(xi + 1, yi, zi + 1), u),
             lerp(hash(xi, yi + 1, zi + 1), hash(xi + 1, yi + 1, zi + 1), u), v), w);
}

float VoxelTerrain::FBM(float x, float y, float z, int octaves, float persistence, float lacunarity, int seed) const {
    float total = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; i++) {
        total += PerlinNoise3D(x * frequency, y * frequency, z * frequency, seed) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    return total / maxValue;
}

// ============================================================================
// Utility
// ============================================================================

glm::ivec3 VoxelTerrain::WorldToChunk(const glm::vec3& worldPos) const {
    glm::vec3 voxelPos = worldPos / m_config.voxelSize;
    return glm::ivec3(
        static_cast<int>(std::floor(voxelPos.x / VoxelChunk::SIZE)),
        static_cast<int>(std::floor(voxelPos.y / VoxelChunk::SIZE)),
        static_cast<int>(std::floor(voxelPos.z / VoxelChunk::SIZE))
    );
}

glm::ivec3 VoxelTerrain::WorldToLocal(const glm::vec3& worldPos, const glm::ivec3& chunkPos) const {
    glm::vec3 voxelPos = worldPos / m_config.voxelSize;
    glm::vec3 chunkOrigin = glm::vec3(chunkPos) * static_cast<float>(VoxelChunk::SIZE);
    glm::vec3 local = voxelPos - chunkOrigin;
    return glm::ivec3(
        static_cast<int>(std::floor(local.x)) % VoxelChunk::SIZE,
        static_cast<int>(std::floor(local.y)) % VoxelChunk::SIZE,
        static_cast<int>(std::floor(local.z)) % VoxelChunk::SIZE
    );
}

glm::vec3 VoxelTerrain::LocalToWorld(const glm::ivec3& localPos, const glm::ivec3& chunkPos) const {
    glm::vec3 chunkOrigin = glm::vec3(chunkPos) * static_cast<float>(VoxelChunk::SIZE);
    return (chunkOrigin + glm::vec3(localPos)) * m_config.voxelSize;
}

uint64_t VoxelTerrain::GetChunkKey(const glm::ivec3& pos) const {
    // Pack three 21-bit integers into 64 bits
    uint64_t x = static_cast<uint64_t>(pos.x + (1 << 20)) & 0x1FFFFF;
    uint64_t y = static_cast<uint64_t>(pos.y + (1 << 20)) & 0x1FFFFF;
    uint64_t z = static_cast<uint64_t>(pos.z + (1 << 20)) & 0x1FFFFF;
    return x | (y << 21) | (z << 42);
}

// Serialization stubs
bool VoxelTerrain::SaveTerrain(const std::string& path) const { return false; }
bool VoxelTerrain::LoadTerrain(const std::string& path) { return false; }
bool VoxelTerrain::SaveChunk(const glm::ivec3& chunkPos, const std::string& path) const { return false; }
bool VoxelTerrain::LoadChunk(const std::string& path) { return false; }

std::vector<glm::ivec3> VoxelTerrain::GetChunksAlongRay(const glm::vec3& origin, const glm::vec3& direction, float maxDistance) const {
    std::vector<glm::ivec3> chunks;

    glm::vec3 dir = glm::normalize(direction);
    float chunkWorldSize = VoxelChunk::SIZE * m_config.voxelSize;

    for (float t = 0; t < maxDistance; t += chunkWorldSize * 0.5f) {
        glm::vec3 pos = origin + dir * t;
        glm::ivec3 chunkPos = WorldToChunk(pos);

        if (chunks.empty() || chunks.back() != chunkPos) {
            chunks.push_back(chunkPos);
        }
    }

    return chunks;
}

} // namespace Nova
