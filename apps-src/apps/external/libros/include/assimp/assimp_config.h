// it is from visual studio's "Preprocessor Definitions"
#pragma once
#ifndef AI_ASSIMP_CONFIG_H_INC
#define AI_ASSIMP_CONFIG_H_INC

#define ASSIMP_BUILD_NO_C4D_IMPORTER
#define ASSIMP_BUILD_NO_M3D_IMPORTER
#define ASSIMP_BUILD_NO_M3D_EXPORTER
#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES    0
#define ASSIMP_IMPORTER_GLTF_USE_OPEN3DGC       1
#define RAPIDJSON_HAS_STDSTRING                 1
#define RAPIDJSON_NOMEMBERITERATORCLASS
#define ASSIMP_BUILD_DLL_EXPORT
#define OPENDDLPARSER_BUILD
#define assimp_EXPORTS

// below is rose defined
#define ASSIMP_BUILD_NO_OWN_ZLIB

#endif
