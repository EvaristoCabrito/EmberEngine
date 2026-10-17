#include "MeshCooker.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#include <meshoptimizer.h>
#include <stb_image.h>

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstring>
#include <format>
#include <fstream>
#include <map>
#include <set>
#include <string>

#include "TextureCooker.h"
#include "core/Jobs.h"
#include "core/Log.h"
#include "render/MeshBuild.h"
#include "render/MeshFormat.h"

namespace ember {

namespace {

// Bump when the cooked output changes for the same source (forces a re-cook of every mesh).
// 2: protect mask, 3: edge limit, 4: authored LODs skipped, 5: proxy without texture slots, 6: textures shared by content
constexpr uint32_t kMeshCookerRevision = 6;

// One material's triangles, transforms baked in, one entry per unique vertex.
struct Part {
  std::vector<float> positions, normals, uvs, tangents, colors;  // 3, 3, 2, 4, 4 floats per vertex
  std::vector<uint32_t> indices;
  bool hasTangents = false, hasColors = false;
};

struct Cooked {
  std::vector<uint8_t> mesh;
  std::vector<std::string> textureAssets;                                 // every texture the materials use
  std::vector<std::pair<std::string, std::vector<uint8_t>>> newTextures;  // the ones not in the cache yet: asset, bytes
};

uint64_t fnv(uint64_t hash, const void* data, size_t size) {
  const auto* bytes = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 0x100000001b3ull;
  return hash;
}

bool readFile(const std::filesystem::path& path, std::vector<uint8_t>& out) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) return false;
  out.resize(static_cast<size_t>(file.tellg()));
  file.seekg(0);
  return static_cast<bool>(file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size())));
}

bool writeFile(const std::filesystem::path& path, const void* data, size_t size) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
  return static_cast<bool>(file);
}

// Shared files are trusted by name alone (content-addressed): write them whole or not at all.
bool writeFileAtomic(const std::filesystem::path& path, const void* data, size_t size) {
  const std::filesystem::path temp = std::filesystem::path(path).concat(".part");
  if (!writeFile(temp, data, size)) return false;
  std::error_code error;
  std::filesystem::rename(temp, path, error);
  return !error;
}

void normalize3(float* v) {
  const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (len > 0.0f) for (int i = 0; i < 3; ++i) v[i] /= len;
  else v[0] = 0.0f, v[1] = 1.0f, v[2] = 0.0f;
}

const cgltf_accessor* findAttribute(const cgltf_primitive& prim, cgltf_attribute_type type) {
  for (cgltf_size i = 0; i < prim.attributes_count; ++i)
    if (prim.attributes[i].type == type && prim.attributes[i].index == 0) return prim.attributes[i].data;
  return nullptr;
}

// Authored levels of detail beyond the first (glTF extras {"lod": n > 0}, or a name ending in _LOD1.._LOD9):
// the cooker builds its own cluster LOD from the most detailed level, so the others must not be drawn too.
bool isLowerAuthoredLod(const cgltf_data& data, const cgltf_node& node) {
  if (node.name) {
    const std::string name = node.name;
    const size_t at = name.rfind("_LOD");
    if (at != std::string::npos && at + 5 == name.size() && name[at + 4] >= '1' && name[at + 4] <= '9') return true;
  }
  cgltf_size size = 0;
  if (cgltf_copy_extras_json(&data, &node.extras, nullptr, &size) != cgltf_result_success || size == 0) return false;
  std::string json(size, ' ');
  if (cgltf_copy_extras_json(&data, &node.extras, json.data(), &size) != cgltf_result_success) return false;
  const size_t key = json.find("\"lod\"");
  if (key == std::string::npos) return false;
  const size_t value = json.find_first_of("0123456789", json.find(':', key));
  return value != std::string::npos && json[value] != '0';
}

// Every triangle primitive of every node, in mesh space (node transforms baked in), split by material.
// Material index == materials_count collects primitives without a material.
std::vector<Part> gatherParts(const cgltf_data& data, uint64_t& sourceTriangles) {
  std::vector<Part> parts(data.materials_count + 1);
  for (cgltf_size n = 0; n < data.nodes_count; ++n) {
    const cgltf_node& node = data.nodes[n];
    if (!node.mesh) continue;
    if (isLowerAuthoredLod(data, node)) {
      logInfo("  skipped authored LOD node '{}' (cluster LOD replaces it)", node.name ? node.name : "");
      continue;
    }
    float m[16];
    cgltf_node_transform_world(&node, m);
    // Normals use the inverse transpose of the upper 3x3 (cofactor matrix: same direction, no division).
    const float nm[9] = {m[5] * m[10] - m[6] * m[9], m[6] * m[8] - m[4] * m[10], m[4] * m[9] - m[5] * m[8],
                         m[2] * m[9] - m[1] * m[10], m[0] * m[10] - m[2] * m[8], m[1] * m[8] - m[0] * m[9],
                         m[1] * m[6] - m[2] * m[5], m[2] * m[4] - m[0] * m[6], m[0] * m[5] - m[1] * m[4]};
    const float det = m[0] * nm[0] + m[1] * nm[1] + m[2] * nm[2];
    for (cgltf_size p = 0; p < node.mesh->primitives_count; ++p) {
      const cgltf_primitive& prim = node.mesh->primitives[p];
      if (prim.type != cgltf_primitive_type_triangles) {
        logWarn("  skipped a non-triangle primitive in mesh '{}'", node.mesh->name ? node.mesh->name : "");
        continue;
      }
      const cgltf_accessor* pos = findAttribute(prim, cgltf_attribute_type_position);
      if (!pos) continue;
      const cgltf_accessor* nrm = findAttribute(prim, cgltf_attribute_type_normal);
      const cgltf_accessor* uv = findAttribute(prim, cgltf_attribute_type_texcoord);
      const cgltf_accessor* tan = findAttribute(prim, cgltf_attribute_type_tangent);
      const cgltf_accessor* col = findAttribute(prim, cgltf_attribute_type_color);
      Part& part = parts[prim.material ? static_cast<size_t>(prim.material - data.materials) : data.materials_count];
      const uint32_t base = static_cast<uint32_t>(part.positions.size() / 3);
      if (tan) part.hasTangents = true;
      if (col) part.hasColors = true;
      for (cgltf_size v = 0; v < pos->count; ++v) {
        float p3[3] = {}, n3[3] = {0, 1, 0}, t2[2] = {}, t4[4] = {1, 0, 0, 1}, c4[4] = {1, 1, 1, 1};
        cgltf_accessor_read_float(pos, v, p3, 3);
        if (nrm) cgltf_accessor_read_float(nrm, v, n3, 3);
        if (uv) cgltf_accessor_read_float(uv, v, t2, 2);
        if (tan) cgltf_accessor_read_float(tan, v, t4, 4);
        if (col) cgltf_accessor_read_float(col, v, c4, cgltf_num_components(col->type));
        for (int i = 0; i < 3; ++i) part.positions.push_back(m[i] * p3[0] + m[4 + i] * p3[1] + m[8 + i] * p3[2] + m[12 + i]);
        float tn[3] = {nm[0] * n3[0] + nm[3] * n3[1] + nm[6] * n3[2], nm[1] * n3[0] + nm[4] * n3[1] + nm[7] * n3[2],
                       nm[2] * n3[0] + nm[5] * n3[1] + nm[8] * n3[2]};
        if (det < 0.0f) for (float& c : tn) c = -c;
        normalize3(tn);
        part.normals.insert(part.normals.end(), tn, tn + 3);
        part.uvs.insert(part.uvs.end(), t2, t2 + 2);
        float tt[4] = {m[0] * t4[0] + m[4] * t4[1] + m[8] * t4[2], m[1] * t4[0] + m[5] * t4[1] + m[9] * t4[2],
                       m[2] * t4[0] + m[6] * t4[1] + m[10] * t4[2], det < 0.0f ? -t4[3] : t4[3]};
        normalize3(tt);
        part.tangents.insert(part.tangents.end(), tt, tt + 4);
        part.colors.insert(part.colors.end(), c4, c4 + 4);
      }
      const size_t count = prim.indices ? prim.indices->count : pos->count;
      for (size_t i = 0; i + 2 < count; i += 3) {
        uint32_t tri[3];
        for (int k = 0; k < 3; ++k)
          tri[k] = base + static_cast<uint32_t>(prim.indices ? cgltf_accessor_read_index(prim.indices, i + k) : i + k);
        if (det < 0.0f) std::swap(tri[1], tri[2]);  // a mirroring transform flips the winding
        part.indices.insert(part.indices.end(), tri, tri + 3);
      }
      if (!nrm) {
        // No normals in the source: smooth, area-weighted face normals over this primitive's vertices.
        std::fill(part.normals.begin() + base * 3, part.normals.end(), 0.0f);
        const size_t first = part.indices.size() - (count / 3) * 3;
        for (size_t i = first; i < part.indices.size(); i += 3) {
          const float* a = &part.positions[part.indices[i] * 3];
          const float* b = &part.positions[part.indices[i + 1] * 3];
          const float* c = &part.positions[part.indices[i + 2] * 3];
          const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
          const float f[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
          for (int k = 0; k < 3; ++k)
            for (int j = 0; j < 3; ++j) part.normals[part.indices[i + k] * 3 + j] += f[j];
        }
        for (size_t v = base; v < part.positions.size() / 3; ++v) normalize3(&part.normals[v * 3]);
      }
      sourceTriangles += count / 3;
    }
  }
  return parts;
}

// Tangents for normal mapping when the source has none: generated per corner (MikkTSpace-style), then
// vertices are re-shared wherever every attribute matches.
void generateTangents(Part& part) {
  const size_t corners = part.indices.size();
  std::vector<float> cornerTangents(corners * 4);
  meshopt_generateTangents(cornerTangents.data(), part.indices.data(), corners, part.positions.data(), part.positions.size() / 3,
                           12, part.normals.data(), 12, part.uvs.data(), 8, 0);
  // Unindex every stream, then reindex on all of them together.
  Part flat;
  flat.hasColors = part.hasColors;
  flat.hasTangents = true;
  for (size_t c = 0; c < corners; ++c) {
    const uint32_t v = part.indices[c];
    flat.positions.insert(flat.positions.end(), &part.positions[v * 3], &part.positions[v * 3] + 3);
    flat.normals.insert(flat.normals.end(), &part.normals[v * 3], &part.normals[v * 3] + 3);
    flat.uvs.insert(flat.uvs.end(), &part.uvs[v * 2], &part.uvs[v * 2] + 2);
    flat.colors.insert(flat.colors.end(), &part.colors[v * 4], &part.colors[v * 4] + 4);
  }
  flat.tangents = std::move(cornerTangents);
  const meshopt_Stream streams[] = {{flat.positions.data(), 12, 12}, {flat.normals.data(), 12, 12}, {flat.uvs.data(), 8, 8},
                                    {flat.colors.data(), 16, 16}, {flat.tangents.data(), 16, 16}};
  std::vector<uint32_t> remap(corners);
  const size_t unique = meshopt_generateVertexRemapMulti(remap.data(), nullptr, corners, corners, streams, std::size(streams));
  const auto compact = [&](std::vector<float>& stream, size_t width) {
    std::vector<float> out(unique * width);
    meshopt_remapVertexBuffer(out.data(), stream.data(), corners, width * sizeof(float), remap.data());
    stream = std::move(out);
  };
  compact(flat.positions, 3);
  compact(flat.normals, 3);
  compact(flat.uvs, 2);
  compact(flat.colors, 4);
  compact(flat.tangents, 4);
  flat.indices = std::move(remap);  // unindexed input: corner c -> its unique vertex
  part = std::move(flat);
}

struct ImageRgba {
  int width = 0, height = 0;
  std::vector<uint8_t> pixels;
};

bool loadImage(const cgltf_image* image, const std::filesystem::path& gltfDir, ImageRgba& out) {
  int w = 0, h = 0, channels = 0;
  stbi_uc* pixels = nullptr;
  if (image->buffer_view && image->buffer_view->buffer->data) {
    const auto* bytes = static_cast<const stbi_uc*>(image->buffer_view->buffer->data) + image->buffer_view->offset;
    pixels = stbi_load_from_memory(bytes, static_cast<int>(image->buffer_view->size), &w, &h, &channels, 4);
  } else if (image->uri) {
    std::string uri = image->uri;
    cgltf_decode_uri(uri.data());
    uri.resize(std::strlen(uri.c_str()));
    pixels = stbi_load((gltfDir / uri).string().c_str(), &w, &h, &channels, 4);
  }
  if (!pixels) return false;
  out.width = w;
  out.height = h;
  out.pixels.assign(pixels, pixels + size_t(w) * h * 4);
  stbi_image_free(pixels);
  return true;
}

// Proxy-LOD experiment: a part replaced by its voxel remesh (meshopt_remesh, solved positions), welded,
// with smooth area-weighted normals. UVs are zero and colours white (no texture transfer yet).
void remeshPart(Part& part, int resolution) {
  const size_t bound = meshopt_remesh(nullptr, 0, part.indices.data(), part.indices.size(), part.positions.data(),
                                      part.positions.size() / 3, 12, resolution, meshopt_RemeshSolve);
  std::vector<float> corners(bound * 9);
  const size_t tris = meshopt_remesh(corners.data(), bound, part.indices.data(), part.indices.size(), part.positions.data(),
                                     part.positions.size() / 3, 12, resolution, meshopt_RemeshSolve);
  corners.resize(tris * 9);
  std::vector<uint32_t> remap(tris * 3);
  const size_t unique = meshopt_generateVertexRemap(remap.data(), nullptr, tris * 3, corners.data(), tris * 3, 12);
  Part out;
  out.positions.resize(unique * 3);
  meshopt_remapVertexBuffer(out.positions.data(), corners.data(), tris * 3, 12, remap.data());
  out.indices = std::move(remap);
  out.normals.assign(unique * 3, 0.0f);
  for (size_t i = 0; i < out.indices.size(); i += 3) {
    const float* a = &out.positions[out.indices[i] * 3];
    const float* b = &out.positions[out.indices[i + 1] * 3];
    const float* c = &out.positions[out.indices[i + 2] * 3];
    const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    const float n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
    for (int k = 0; k < 3; ++k)
      for (int j = 0; j < 3; ++j) out.normals[out.indices[i + k] * 3 + j] += n[j];
  }
  for (size_t v = 0; v < unique; ++v) normalize3(&out.normals[v * 3]);
  out.uvs.assign(unique * 2, 0.0f);
  out.tangents.assign(unique * 4, 0.0f);
  for (size_t v = 0; v < unique; ++v) out.tangents[v * 4] = out.tangents[v * 4 + 3] = 1.0f;
  out.colors.assign(unique * 4, 1.0f);
  out.hasTangents = true;
  logInfo("  remesh at {}: {} -> {} triangles", resolution, part.indices.size() / 3, tris);
  part = std::move(out);
}

class MeshBuilder {
 public:
  MeshBuilder(JobSystem& jobs, const MeshCookOptions& options, std::filesystem::path gltfDir)
      : jobs_(jobs), options_(options), gltfDir_(std::move(gltfDir)) {}

  bool build(const cgltf_data& data, Cooked& out, MeshCookReport& report) {
    uint64_t sourceTriangles = 0;
    std::vector<Part> parts = gatherParts(data, sourceTriangles);
    materials_.resize(data.materials_count + 1);
    for (size_t m = 0; m < data.materials_count; ++m) materials_[m] = material(data, data.materials[m], m, out);
    materials_[data.materials_count] = defaultMaterial();

    // Materials actually used get dense indices.
    std::vector<MeshMaterial> used;
    for (size_t m = 0; m < parts.size(); ++m) {
      Part& part = parts[m];
      if (part.indices.empty()) continue;
      const uint16_t materialIndex = static_cast<uint16_t>(used.size());
      used.push_back(materials_[m]);
      const bool remeshed = options_.remeshResolution > 0;
      if (remeshed) remeshPart(part, options_.remeshResolution);
      else if (materials_[m].normalTexture && !part.hasTangents) generateTangents(part);
      if (!addPart(part, materialIndex, (materials_[m].flags & kMaterialDoubleSided) != 0, materials_[m].baseColorTexture != 0, remeshed))
        return false;
    }
    if (mesh_.clusterCount() == 0) {
      logError("  no triangles");
      return false;
    }

    out.mesh = mesh_.finish(used, sourceTriangles);

    report.sourceTriangles += sourceTriangles;
    report.dagTriangles += mesh_.dagTriangles();
    report.clusters += mesh_.clusterCount();
    logInfo("  {} triangles -> {} clusters in {} groups, {} levels, {} triangles across all levels, {:.1f} MB",
            sourceTriangles, mesh_.clusterCount(), mesh_.groupCount(), mesh_.levelCount(), mesh_.dagTriangles(), out.mesh.size() / 1048576.0);
    return true;
  }

 private:
  bool addPart(const Part& part, uint16_t materialIndex, bool doubleSided, bool textured, bool positionError) {
    MeshBuildPart p;
    p.positions = part.positions, p.normals = part.normals, p.uvs = part.uvs, p.tangents = part.tangents, p.colors = part.colors;
    p.indices = part.indices;
    p.hasColors = part.hasColors;
    return mesh_.addPart(p, materialIndex, doubleSided, textured, positionError);
  }

  MeshMaterial defaultMaterial() const {
    MeshMaterial m{};
    for (float& c : m.baseColor) c = 1.0f;
    m.roughness = 1.0f;
    m.normalScale = 1.0f;
    m.occlusionStrength = 1.0f;
    m.alphaCutoff = 0.5f;
    return m;
  }

  MeshMaterial material(const cgltf_data& data, const cgltf_material& src, size_t index, Cooked& out) {
    MeshMaterial m = defaultMaterial();
    const std::string name = src.name ? src.name : std::to_string(index);
    if (options_.remeshResolution > 0) {
      // A remeshed proxy has no UVs (yet): factors only, no texture slots that would sample garbage.
      if (src.has_pbr_metallic_roughness) {
        std::memcpy(m.baseColor, src.pbr_metallic_roughness.base_color_factor, 16);
        m.metallic = src.pbr_metallic_roughness.metallic_factor;
        m.roughness = src.pbr_metallic_roughness.roughness_factor;
      }
      return m;
    }
    if (src.has_pbr_metallic_roughness) {
      const cgltf_pbr_metallic_roughness& pbr = src.pbr_metallic_roughness;
      std::memcpy(m.baseColor, pbr.base_color_factor, 16);
      m.metallic = pbr.metallic_factor;
      m.roughness = pbr.roughness_factor;
      m.baseColorTexture = texture(data, pbr.base_color_texture.texture, nullptr, TextureFormat::Bc7Srgb, "color", out);
      m.ormTexture = texture(data, pbr.metallic_roughness_texture.texture, src.occlusion_texture.texture,
                             TextureFormat::Bc7Unorm, "orm", out);
    }
    if (src.normal_texture.texture) {
      m.normalTexture = texture(data, src.normal_texture.texture, nullptr, TextureFormat::Bc5Unorm, "normal", out);
      m.normalScale = src.normal_texture.scale;
    }
    if (src.occlusion_texture.texture) {
      m.occlusionStrength = src.occlusion_texture.scale;
      if (!src.has_pbr_metallic_roughness)
        m.ormTexture = texture(data, nullptr, src.occlusion_texture.texture, TextureFormat::Bc7Unorm, "orm", out);
    }
    const float strength = src.has_emissive_strength ? src.emissive_strength.emissive_strength : 1.0f;
    for (int i = 0; i < 3; ++i) m.emissive[i] = src.emissive_factor[i] * strength;
    if (src.emissive_texture.texture)
      m.emissiveTexture = texture(data, src.emissive_texture.texture, nullptr, TextureFormat::Bc7Srgb, "emissive", out);
    if (src.alpha_mode == cgltf_alpha_mode_mask) m.flags |= kMaterialAlphaMask;
    if (src.alpha_mode == cgltf_alpha_mode_blend) {
      // The visibility buffer is opaque; blended surfaces become alpha-tested until a transparent pass exists.
      logWarn("  material '{}': alpha blend drawn as alpha test (cutoff 0.5)", name);
      m.flags |= kMaterialAlphaMask;
    }
    m.alphaCutoff = src.alpha_cutoff;
    if (src.double_sided) m.flags |= kMaterialDoubleSided;
    if (src.has_clearcoat || src.has_transmission || src.has_ior || src.has_sheen || src.has_specular || src.has_volume)
      logWarn("  material '{}': clearcoat/transmission/ior/sheen/specular/volume extensions ignored (not in the material model yet)", name);
    return m;
  }

  // Cooks one texture role once per mesh. ORM combines glTF's metallic-roughness texture (G, B) with the
  // occlusion texture (R). The cooked texture is named by its content (pixels, size, format, quality):
  // meshes/textures/<hash>.tex, shared by every mesh whose image is the same, cooked once into the cache.
  AssetId texture(const cgltf_data& data, const cgltf_texture* main, const cgltf_texture* occlusion, TextureFormat format,
                  const char* role, Cooked& out) {
    const cgltf_image* a = main ? main->image : nullptr;
    const cgltf_image* b = occlusion ? occlusion->image : nullptr;
    if (!a && !b) return 0;
    const auto key = std::make_tuple(std::string(role), a ? a - data.images : -1, b ? b - data.images : -1);
    if (const auto it = textures_.find(key); it != textures_.end()) return it->second;

    ImageRgba ia, ib;
    if ((a && !loadImage(a, gltfDir_, ia)) || (b && !loadImage(b, gltfDir_, ib))) {
      logError("  cannot decode a {} texture", role);
      return textures_[key] = 0;
    }
    Rgba8 image;
    if (format == TextureFormat::Bc7Unorm) {
      // R occlusion, G roughness, B metallic; a missing source reads as 1 (the factor alone applies).
      const ImageRgba& size = a ? ia : ib;
      image.width = static_cast<uint32_t>(size.width);
      image.height = static_cast<uint32_t>(size.height);
      image.pixels.assign(size_t{image.width} * image.height * 4, 255);
      for (uint32_t y = 0; y < image.height; ++y)
        for (uint32_t x = 0; x < image.width; ++x) {
          uint8_t* o = &image.pixels[(size_t{y} * image.width + x) * 4];
          if (a) {
            const uint8_t* p = &ia.pixels[(size_t{y} * ia.width + x) * 4];
            o[1] = p[1];
            o[2] = p[2];
          }
          if (b) {
            const uint32_t bx = x * static_cast<uint32_t>(ib.width) / image.width, by = y * static_cast<uint32_t>(ib.height) / image.height;
            o[0] = ib.pixels[(size_t{by} * ib.width + bx) * 4];
          }
        }
    } else {
      image.width = static_cast<uint32_t>(ia.width);
      image.height = static_cast<uint32_t>(ia.height);
      image.pixels = std::move(ia.pixels);
    }
    const uint32_t settings[] = {image.width, image.height, static_cast<uint32_t>(format), static_cast<uint32_t>(options_.quality),
                                 kTextureFormatVersion};
    const uint64_t hash = fnv(fnv(0xcbf29ce484222325ull, settings, sizeof(settings)), image.pixels.data(), image.pixels.size());
    const std::string asset = std::format("meshes/textures/{:016x}.tex", hash);
    const AssetId id = assetId(asset);
    if (std::find(out.textureAssets.begin(), out.textureAssets.end(), asset) != out.textureAssets.end()) return textures_[key] = id;
    out.textureAssets.push_back(asset);
    std::error_code error;
    if (std::filesystem::file_size(options_.cacheDir / asset, error) > 0 && !error) {
      logInfo("  texture {} ({}): shared, already cooked", asset, role);
      return textures_[key] = id;
    }
    const auto start = std::chrono::steady_clock::now();
    out.newTextures.emplace_back(asset, cookTexture(jobs_, image, format, options_.quality));
    logInfo("  texture {} ({}): {}x{} -> {:.1f} MB in {:.1f}s", asset, role, image.width, image.height,
            out.newTextures.back().second.size() / 1048576.0,
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
    return textures_[key] = id;
  }

  JobSystem& jobs_;
  const MeshCookOptions& options_;
  std::filesystem::path gltfDir_;
  std::vector<MeshMaterial> materials_;
  std::map<std::tuple<std::string, ptrdiff_t, ptrdiff_t>, AssetId> textures_;
  MeshBuild mesh_;
};

}  // namespace

bool cookMeshes(JobSystem& jobs, const std::vector<std::filesystem::path>& sources, const MeshCookOptions& options,
                std::vector<PakSource>& out, MeshCookReport& report) {
  const auto start = std::chrono::steady_clock::now();
  report = {};
  std::set<std::string> packed;  // assets already appended to `out` (shared textures appear in several manifests)
  for (const std::filesystem::path& relative : sources) {
    ++report.meshes;
    const std::filesystem::path source = options.sourceRoot / relative;
    std::string base = relative.generic_string();
    base = normalizeAssetPath("meshes/" + base.substr(0, base.size() - relative.extension().string().size()));
    const std::filesystem::path cached = options.cacheDir / base;
    logInfo("{}", base);

    std::vector<uint8_t> bytes;
    if (!readFile(source, bytes)) {
      logError("  cannot read {}", source.string());
      ++report.failed;
      continue;
    }
    // The cache key covers the source bytes, the formats and the settings; the manifest lists the cooked files.
    uint64_t key = fnv(0xcbf29ce484222325ull, bytes.data(), bytes.size());
    const uint32_t versions[] = {kMeshCookerRevision, kMeshFormatVersion, kTextureFormatVersion, static_cast<uint32_t>(options.quality),
                                 static_cast<uint32_t>(options.remeshResolution)};
    key = fnv(key, versions, sizeof(versions));
    const std::filesystem::path manifest = std::filesystem::path(cached).concat(".cooked");

    std::vector<std::string> assets;
    if (std::ifstream in(manifest); in) {
      uint64_t stored = 0;
      in >> std::hex >> stored;
      for (std::string line; in >> line;) assets.push_back(line);
      bool complete = stored == key && !assets.empty();
      for (const std::string& a : assets) complete = complete && std::filesystem::exists(options.cacheDir / a);
      if (complete) {
        ++report.fromCache;
        logInfo("  from cache");
      } else {
        assets.clear();
      }
    }
    if (assets.empty()) {
      cgltf_options gltfOptions{};
      cgltf_data* data = nullptr;
      const std::string path = source.string();
      if (cgltf_parse_file(&gltfOptions, path.c_str(), &data) != cgltf_result_success ||
          cgltf_load_buffers(&gltfOptions, data, path.c_str()) != cgltf_result_success || cgltf_validate(data) != cgltf_result_success) {
        logError("  not a valid glTF/GLB file");
        cgltf_free(data);
        ++report.failed;
        continue;
      }
      Cooked cooked;
      MeshBuilder builder(jobs, options, source.parent_path());
      const bool built = builder.build(*data, cooked, report);
      cgltf_free(data);
      if (!built) {
        ++report.failed;
        continue;
      }
      bool written = writeFile(options.cacheDir / (base + ".mesh"), cooked.mesh.data(), cooked.mesh.size());
      assets.push_back(base + ".mesh");
      for (const auto& [asset, texture] : cooked.newTextures)
        written = written && writeFileAtomic(options.cacheDir / asset, texture.data(), texture.size());
      assets.insert(assets.end(), cooked.textureAssets.begin(), cooked.textureAssets.end());
      std::string list = std::format("{:016x}\n", key);
      for (const std::string& a : assets) list += a + "\n";
      if (!written || !writeFile(manifest, list.data(), list.size())) {
        logError("  cannot write to the cook cache {}", options.cacheDir.string());
        ++report.failed;
        continue;
      }
      ++report.cooked;
    }
    for (const std::string& a : assets) {
      if (!packed.insert(a).second) {
        ++report.sharedTextureUses;  // a shared texture already handed to the pak by an earlier mesh
        continue;
      }
      const std::filesystem::path file = options.cacheDir / a;
      const uint64_t size = std::filesystem::file_size(file);
      if (a.ends_with(".tex")) {
        ++report.textures;
        report.textureBytes += size;
      } else {
        report.geometryBytes += size;
      }
      out.push_back({a, file});
    }
  }
  report.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  return report.failed == 0;
}

}  // namespace ember
