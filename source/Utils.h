#pragma once
#include <array>
#include <cassert>
#include <fstream>
#include "Math.h"
#include "DataTypes.h"

namespace dae
{
	namespace GeometryUtils
	{
#pragma region Sphere HitTest
		//SPHERE HIT-TESTS
		inline bool HitTest_Sphere(const Sphere& sphere, const Ray& ray, HitRecord& hitRecord, bool ignoreHitRecord = false)
		{
			//todo W1
			// d = ray.direction

			const Vector3 cameraToSphere{ ray.origin - sphere.origin };	
			const float b{ Vector3::Dot(2 * ray.direction, cameraToSphere) };
			const float c{ Vector3::Dot(cameraToSphere, cameraToSphere) - Square(sphere.radius) };

			const float discriminant{ Square(b) - 4 * c };

			float t{};

			// no hit if discriminant is 0
			hitRecord.didHit = discriminant > 0;

			if (!hitRecord.didHit)
				return false;

			t = (-b - sqrtf(discriminant)) * 0.5f;

			// check if inside of min and max
			hitRecord.didHit = t > ray.min && t < ray.max;

			if (!hitRecord.didHit)
				return false;


			if (t < ray.min)
				t = (-b + sqrtf(discriminant)) * 0.5f;

			hitRecord.origin = ray.origin + t * ray.direction;
			hitRecord.normal = (hitRecord.origin - sphere.origin).Normalized();
			hitRecord.t = t;
			hitRecord.materialIndex = sphere.materialIndex;

			return true;
		}

		inline bool HitTest_Sphere(const Sphere& sphere, const Ray& ray)
		{
			HitRecord temp{};
			return HitTest_Sphere(sphere, ray, temp, true);
		}
#pragma endregion
#pragma region Plane HitTest
		//PLANE HIT-TESTS
		inline bool HitTest_Plane(const Plane& plane, const Ray& ray, HitRecord& hitRecord, bool ignoreHitRecord = false)
		{
			//todo W1

			// calculate dot between ray and normal
			const float unitProjection{ Vector3::Dot(ray.direction, plane.normal) };

			// if ray is coming from behind, ignore
			if (unitProjection >= 0)
				return false;

			// calculate the distance to the intersection
			const float height{ Vector3::Dot((plane.origin - ray.origin) , plane.normal) };

			const float t{ height / unitProjection };

			// check distance in range
			hitRecord.didHit = t > ray.min && t < ray.max;
			if (!hitRecord.didHit)
				return false;

			hitRecord.origin = ray.origin + t * ray.direction;
			hitRecord.normal = plane.normal;
			hitRecord.materialIndex = plane.materialIndex;
			hitRecord.t = t;

			return true;
		}

		inline bool HitTest_Plane(const Plane& plane, const Ray& ray)
		{
			HitRecord temp{};
			return HitTest_Plane(plane, ray, temp, true);
		}
#pragma endregion
#pragma region Triangle HitTest
		//TRIANGLE HIT-TESTS
		inline bool HitTest_Triangle(const Triangle& triangle, const Ray& ray, HitRecord& hitRecord, bool ignoreHitRecord = false)
		{
			//todo W5
			// Get intersection point with plane
			float normalViewDot{ Vector3::Dot(triangle.normal, ray.direction) };

			// in case of shadows inverse the dot
			if (ignoreHitRecord)
				normalViewDot *= -1;

			switch (triangle.cullMode)
			{
			case TriangleCullMode::FrontFaceCulling:
				if ( normalViewDot <= 0)
					return false;
				break;
			case TriangleCullMode::BackFaceCulling:
				if (normalViewDot >= 0)
					return false;
				break;
			case TriangleCullMode::NoCulling:
				if (abs(normalViewDot) < FLT_EPSILON)
					return false;
				break;
			}

			const Vector3 L{ triangle.v0 - ray.origin };
			const float t{ Vector3::Dot(L, triangle.normal) / Vector3::Dot(ray.direction, triangle.normal) };

			if (t < ray.min || t > ray.max)
				return false;

			const Vector3 intersection{ ray.origin + ray.direction * t };

			// Check if intersection point is in the triangle
			std::array vertexVec{ triangle.v0, triangle.v1, triangle.v2 };
			const int nrVertices{ static_cast<int>(vertexVec.size()) };

			for (int index{}; index < nrVertices; ++index)
			{
				const Vector3 e{ vertexVec[(index + 1) % nrVertices] - vertexVec[index] };
				const Vector3 p{ intersection - vertexVec[index] };
				if (Vector3::Dot(Vector3::Cross(e, p), triangle.normal) < 0)
					return false;
			}

			hitRecord.origin = intersection;
			hitRecord.didHit = true;
			hitRecord.normal = triangle.normal;
			hitRecord.materialIndex = triangle.materialIndex;
			hitRecord.t = t;
			return true;
		}

		inline bool HitTest_Triangle_Moller(const Triangle& triangle, const Ray& ray, HitRecord& hitRecord, bool ignoreHitRecord = false)
		{
			// source : https://cadxfem.org/inf/Fast%20MinimumStorage%20RayTriangle%20Intersection.pdf
			// Get intersection point with plane
			float normalViewDot{ Vector3::Dot(triangle.normal, ray.direction) };

			// in case of shadows inverse the dot
			if (ignoreHitRecord)
				normalViewDot *= -1;

			switch (triangle.cullMode)
			{
			case TriangleCullMode::FrontFaceCulling:
				if (normalViewDot <= 0)
					return false;
				break;
			case TriangleCullMode::BackFaceCulling:
				if (normalViewDot >= 0)
					return false;
				break;
			case TriangleCullMode::NoCulling:
				if (abs(normalViewDot) < FLT_EPSILON)
					return false;
				break;
			}

			// Get edges sharing v0
			const Vector3 edge1{ triangle.v1 - triangle.v0 };
			const Vector3 edge2{ triangle.v2 - triangle.v0 };

			// used to calculate U param
			const Vector3 pVec{ Vector3::Cross(ray.direction, edge2) };

			// If det is near zero,
			const float det{ Vector3::Dot(edge1, pVec) };

			if (det > -FLT_EPSILON && det < FLT_EPSILON)
				return false;

			const float invDet{ 1.f / det };

			const Vector3 tVec{ ray.origin - triangle.v0 };

			// calculate u and test bounds
			const float u{ Vector3::Dot(tVec, pVec) * invDet };
			if (u < 0 || u > 1.f)
				return false;

			// used to calculate V param
			const Vector3 qVec{ Vector3::Cross(tVec, edge1) };

			// calculate v and test bounds
			const float v{ Vector3::Dot(ray.direction, qVec) * invDet };
			if (v < 0 || u + v > 1.f)
				return false;

			const float t{ Vector3::Dot(edge2, qVec) * invDet };

			// ray doesn't go the opposite way or beyond its max extent
			if (t < ray.min || t > ray.max)
				return false;

			hitRecord.origin = (1 - u - v) * triangle.v0 + u * triangle.v1 + v * triangle.v2;
			hitRecord.didHit = true;
			hitRecord.normal = triangle.normal;
			hitRecord.materialIndex = triangle.materialIndex;
			hitRecord.t = t;

			return true;
		}

		inline bool HitTest_Triangle_Moller(	const Vector3& v0, const Vector3& v1, const Vector3& v2, const Vector3& normal,
												TriangleCullMode cullmode, unsigned char materialIndex,
												const Ray& ray, HitRecord& hitRecord, bool ignoreHitRecord = false)
		{
			// source : https://cadxfem.org/inf/Fast%20MinimumStorage%20RayTriangle%20Intersection.pdf
			// Get intersection point with plane
			float normalViewDot{ Vector3::Dot(normal, ray.direction) };

			// in case of shadows inverse the dot
			if (ignoreHitRecord)
				normalViewDot *= -1;

			switch (cullmode)
			{
			case TriangleCullMode::FrontFaceCulling:
				if (normalViewDot <= 0)
					return false;
				break;
			case TriangleCullMode::BackFaceCulling:
				if (normalViewDot >= 0)
					return false;
				break;
			case TriangleCullMode::NoCulling:
				if (abs(normalViewDot) < FLT_EPSILON)
					return false;
				break;
			}

			// Get edges sharing v0
			const Vector3 edge1{ v1 - v0 };
			const Vector3 edge2{ v2 - v0 };

			// used to calculate U param
			const Vector3 pVec{ Vector3::Cross(ray.direction, edge2) };

			// If det is near zero,
			const float det{ Vector3::Dot(edge1, pVec) };

			if (abs(det) < FLT_EPSILON)
				return false;

			const float invDet{ 1.f / det };

			const Vector3 tVec{ ray.origin - v0 };

			// calculate u and test bounds
			const float u{ Vector3::Dot(tVec, pVec) * invDet };
			if (u < 0 || u > 1.f)
				return false;

			// used to calculate V param
			const Vector3 qVec{ Vector3::Cross(tVec, edge1) };

			// calculate v and test bounds
			const float v{ Vector3::Dot(ray.direction, qVec) * invDet };
			if (v < 0 || u + v > 1.f)
				return false;

			const float t{ Vector3::Dot(edge2, qVec) * invDet };

			// ray doesn't go the opposite way or beyond its max extent
			if (t < ray.min || t > ray.max)
				return false;

			hitRecord.origin = (1 - u - v) * v0 + u * v1 + v * v2;
			hitRecord.didHit = true;
			hitRecord.normal = normal;
			hitRecord.materialIndex = materialIndex;
			hitRecord.t = t;

			return true;
		}


		inline bool HitTest_Triangle(const Triangle& triangle, const Ray& ray)
		{
			HitRecord temp{};
			return HitTest_Triangle_Moller(triangle, ray, temp, true);
		}
#pragma endregion
#pragma region TriangeMesh HitTest

		inline bool SlabTest_TriangleMesh(const TriangleMesh& mesh, const Ray& ray)
		{
			const float tx1{ (mesh.transformedMinAABB.x - ray.origin.x) / ray.direction.x };
			const float tx2{ (mesh.transformedMaxAABB.x - ray.origin.x) / ray.direction.x };

			float tmin{ std::min(tx1, tx2) };
			float tmax{ std::max(tx1, tx2) };

			const float ty1{ (mesh.transformedMinAABB.y - ray.origin.y) / ray.direction.y };
			const float ty2{ (mesh.transformedMaxAABB.y - ray.origin.y) / ray.direction.y };

			tmin = std::max(tmin, std::min(ty1, ty2));
			tmax = std::min(tmax, std::max(ty1, ty2));

			const float tz1{ (mesh.transformedMinAABB.z - ray.origin.z) / ray.direction.z };
			const float tz2{ (mesh.transformedMaxAABB.z - ray.origin.z) / ray.direction.z };

			tmin = std::max(tmin, std::min(tz1, tz2));
			tmax = std::min(tmax, std::max(tz1, tz2));

			return tmax > 0 && tmax >= tmin;
		}

		inline bool HitTest_TriangleMesh(const TriangleMesh& mesh, const Ray& ray, HitRecord& hitRecord, bool ignoreHitRecord = false)
		{
			//todo W5
			// slabtest
			if (!SlabTest_TriangleMesh(mesh, ray))
				return false;

			//Triangle triangle{};
			//triangle.cullMode = mesh.cullMode;
			//triangle.materialIndex = mesh.materialIndex;

			HitRecord temp{};
			bool didHit{ false };

			for (int index{}; index < static_cast<int>(mesh.normals.size()); ++index)
			{
				const int offset{ index * 3 };

				//triangle.v0 = mesh.transformedPositions[mesh.indices[offset]];
				//triangle.v1 = mesh.transformedPositions[mesh.indices[offset + 1]];
				//triangle.v2 = mesh.transformedPositions[mesh.indices[offset + 2]];
				//triangle.normal = mesh.transformedNormals[index];
				const Vector3& v0{ mesh.transformedPositions[mesh.indices[offset]] };
				const Vector3& v1{ mesh.transformedPositions[mesh.indices[offset + 1]] };
				const Vector3& v2{ mesh.transformedPositions[mesh.indices[offset + 2]] };
				const Vector3& normal{ mesh.transformedNormals[index] };

				if (HitTest_Triangle_Moller(v0, v1, v2, normal, mesh.cullMode, 
											mesh.materialIndex, ray, temp, ignoreHitRecord))
				//if (HitTest_Triangle_Moller(triangle, ray, temp, ignoreHitRecord))
				{
					if (ignoreHitRecord)
						return true;

					if (temp.t < hitRecord.t)
					{
						hitRecord = temp;
						didHit = true;
					}
				}
			}
			
			return didHit;
		}

		inline bool HitTest_TriangleMesh(const TriangleMesh& mesh, const Ray& ray)
		{
			HitRecord temp{};
			return HitTest_TriangleMesh(mesh, ray, temp, true);
		}

#pragma endregion
	}

	namespace LightUtils
	{
		//Direction from target to light
		inline Vector3 GetDirectionToLight(const Light& light, const Vector3 origin)
		{
			//todo W3
			return { light.origin - origin };
		}

		inline ColorRGB GetRadiance(const Light& light, const Vector3& target)
		{
			//todo W3
			switch (light.type)
			{
			case LightType::Point:
				return { light.color * (light.intensity / (light.origin - target).SqrMagnitude()) };
			case LightType::Directional:
				return { light.color * light.intensity };
			}
			return{};
		}
	}

	namespace Utils
	{
		//Just parses vertices and indices
#pragma warning(push)
#pragma warning(disable : 4505) //Warning unreferenced local function
		static bool ParseOBJ(const std::string& filename, std::vector<Vector3>& positions, std::vector<Vector3>& normals, std::vector<int>& indices)
		{
			std::ifstream file(filename);
			if (!file)
				return false;

			std::string sCommand;
			// start a while iteration ending when the end of file is reached (ios::eof)
			while (!file.eof())
			{
				//read the first word of the string, use the >> operator (istream::operator>>) 
				file >> sCommand;
				//use conditional statements to process the different commands	
				if (sCommand == "#")
				{
					// Ignore Comment
				}
				else if (sCommand == "v")
				{
					//Vertex
					float x, y, z;
					file >> x >> y >> z;
					positions.push_back({ x, y, z });
				}
				else if (sCommand == "f")
				{
					float i0, i1, i2;
					file >> i0 >> i1 >> i2;

					indices.push_back((int)i0 - 1);
					indices.push_back((int)i1 - 1);
					indices.push_back((int)i2 - 1);
				}
				//read till end of line and ignore all remaining chars
				file.ignore(1000, '\n');

				if (file.eof()) 
					break;
			}

			//Precompute normals
			for (uint64_t index = 0; index < indices.size(); index += 3)
			{
				uint32_t i0 = indices[index];
				uint32_t i1 = indices[index + 1];
				uint32_t i2 = indices[index + 2];

				Vector3 edgeV0V1 = positions[i1] - positions[i0];
				Vector3 edgeV0V2 = positions[i2] - positions[i0];
				Vector3 normal = Vector3::Cross(edgeV0V1, edgeV0V2);

				if(isnan(normal.x))
				{
					int k = 0;
				}

				normal.Normalize();
				if (isnan(normal.x))
				{
					int k = 0;
				}

				normals.push_back(normal);
			}

			return true;
		}
#pragma warning(pop)
	}
}