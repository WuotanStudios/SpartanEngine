/*
Copyright(c) 2016-2025 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

//= INCLUDES =====================
#include <vector>
#include <mutex>
#include "Material.h"
#include "../RHI/RHI_Vertex.h"
#include "../Math/BoundingBox.h"
#include "../Resource/IResource.h"
//================================

namespace spartan
{
    enum class MeshFlags : uint32_t
    {
        ImportRemoveRedundantData = 1 << 0,
        ImportLights              = 1 << 1,
        ImportCombineMeshes       = 1 << 2,
        PostProcessNormalizeScale = 1 << 3,
        PostProcessOptimize       = 1 << 4
    };

    enum class MeshType
    {
        Cube,
        Quad,
        Grid,
        Sphere,
        Cylinder,
        Cone,
        Custom,
        Max
    };

    class Mesh : public IResource
    {
    public:
        Mesh();
        ~Mesh();

        // iresource
        void LoadFromFile(const std::string& file_path) override;
        void SaveToFile(const std::string& file_path) override;

        // geometry
        void Clear();
        void GetGeometry(
            uint32_t indexOffset,
            uint32_t indexCount,
            uint32_t vertexOffset,
            uint32_t vertexCount,
            std::vector<uint32_t>* indices,
            std::vector<RHI_Vertex_PosTexNorTan>* vertices
        );
        uint32_t GetMemoryUsage() const;

        // geometry
        void AddGeometry(std::vector<RHI_Vertex_PosTexNorTan>& vertices, std::vector<uint32_t>& indices, uint32_t* vertex_offset_out = nullptr, uint32_t* index_offset_out = nullptr);
        std::vector<RHI_Vertex_PosTexNorTan>& GetVertices() { return m_vertices; }
        std::vector<uint32_t>& GetIndices()                 { return m_indices; }

        // get counts
        uint32_t GetVertexCount() const;
        uint32_t GetIndexCount() const;

        // aabb
        const math::BoundingBox& GetAabb() const { return m_aabb; }

        // gpu buffers
        void CreateGpuBuffers();
        RHI_Buffer* GetIndexBuffer()  { return m_index_buffer.get();  }
        RHI_Buffer* GetVertexBuffer() { return m_vertex_buffer.get(); }

        // root entity
        std::weak_ptr<Entity> GetRootEntity() { return m_root_entity; }
        void SetRootEntity(std::shared_ptr<Entity>& entity) { m_root_entity = entity; }

        // mesh type
        MeshType GetType() const          { return m_type; }
        void SetType(const MeshType type) { m_type = type; }

        // flags
        uint32_t GetFlags() const { return m_flags; }
        static uint32_t GetDefaultFlags();

        void PostProcess();
        void SetMaterial(std::shared_ptr<Material>& material, Entity* entity) const;

    private:
        // geometry
        std::vector<RHI_Vertex_PosTexNorTan> m_vertices;
        std::vector<uint32_t> m_indices;

        // gpu buffers
        std::shared_ptr<RHI_Buffer> m_vertex_buffer;
        std::shared_ptr<RHI_Buffer> m_index_buffer;

        // aabb
        math::BoundingBox m_aabb;

        // misc
        std::mutex m_mutex;
        std::weak_ptr<Entity> m_root_entity;
        MeshType m_type = MeshType::Custom;
    };
}
