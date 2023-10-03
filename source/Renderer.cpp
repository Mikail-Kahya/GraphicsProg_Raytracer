//External includes
#include "SDL.h"
#include "SDL_surface.h"

//Project includes
#include "Renderer.h"
#include "Math.h"
#include "Matrix.h"
#include "Material.h"
#include "Scene.h"
#include "Utils.h"

using namespace dae;

Renderer::Renderer(SDL_Window * pWindow) :
	m_pWindow(pWindow),
	m_pBuffer(SDL_GetWindowSurface(pWindow))
{
	//Initialize
	SDL_GetWindowSize(pWindow, &m_Width, &m_Height);
	m_pBufferPixels = static_cast<uint32_t*>(m_pBuffer->pixels);
}

void Renderer::Render(Scene* pScene) const
{
	Camera& camera = pScene->GetCamera();
	auto& materials = pScene->GetMaterials();
	auto& lights = pScene->GetLights();

	const float aspectRatio{ m_Width / float(m_Height) };
	const float FOV{ tanf(TO_RADIANS * camera.fovAngle / 2) };

	const Matrix cameraToWorld = camera.CalculateCameraToWorld();

	// W3 steps to take
	// 1. Calculate observed area (light cone) for each light


	for (float px{ 0.5f }; px < m_Width; ++px)
	{
		const float x{ (2 * px / m_Width - 1) * aspectRatio * FOV };

		for (float py{ 0.5f }; py < m_Height; ++py)
		{
			const float y{ (1 - 2 * py / m_Height) * FOV };

			Vector3 rayDirection{ x, y, 1};
			rayDirection = cameraToWorld.TransformVector(rayDirection);
			rayDirection.Normalize();

			const Ray viewRay{ camera.origin, rayDirection };

			ColorRGB finalColor{};

			// hitinfo
			HitRecord closestHit{};
			pScene->GetClosestHit(viewRay, closestHit);

			if (closestHit.didHit)
			{
				switch (m_LightingMode)
				{
				case LightingMode::ObservedArea:
					finalColor = { 1,1,1 };
					break;
				case LightingMode::Radiance: 
					finalColor = materials[closestHit.materialIndex]->Shade();
					break;
				case LightingMode::BRDF: break;
				case LightingMode::Combined: break;
				}

				for (const Light& light : lights)
				{
					// get light to closesthit
					const Vector3 lightDirection{ LightUtils::GetDirectionToLight(light, closestHit.origin) };
					const float length{ lightDirection.Magnitude() - FLT_EPSILON };
					Ray lightRay{closestHit.origin + closestHit.normal * FLT_EPSILON, lightDirection.Normalized(), FLT_EPSILON, length};

					// if it hits, the object is being blocked => darken

					const float observedArea{ Vector3::Dot(lightRay.direction, closestHit.normal) };

					switch (m_LightingMode)
					{
					case LightingMode::ObservedArea:
						if (observedArea < 0)
							continue;
						finalColor += colors::White * observedArea;
						break;
					case LightingMode::Radiance:
						finalColor += LightUtils::GetRadiance(light, closestHit.origin);
						break;
					case LightingMode::BRDF: break;
					case LightingMode::Combined:;
						if (observedArea < 0)
							continue;
						finalColor += LightUtils::GetRadiance(light, closestHit.origin) * observedArea;
						break;
					}


					if (pScene->DoesHit(lightRay) && m_EnableShadows)
						finalColor *= 0.5f;
				}
			}

			// Update Color in Buffer
			finalColor.MaxToOne();

			m_pBufferPixels[int(px) + (int(py) * m_Width)] = SDL_MapRGB(m_pBuffer->format,
				static_cast<uint8_t>(finalColor.r * 255),
				static_cast<uint8_t>(finalColor.g * 255),
				static_cast<uint8_t>(finalColor.b * 255));
		}
	}

	//@END
	//Update SDL Surface
	SDL_UpdateWindowSurface(m_pWindow);
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBuffer, "RayTracing_Buffer.bmp");
}

void Renderer::CycleLightMode()
{
	int modeIndex{ int(m_LightingMode) };
	++modeIndex;
	modeIndex %= int(LightingMode::Combined);

	m_LightingMode = LightingMode(modeIndex);
}
