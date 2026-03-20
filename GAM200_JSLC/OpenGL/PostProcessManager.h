#pragma once
#include <memory>
#include "Shader.hpp"

struct PostProcessSettings { float exposure = 1.0f; };

class PostProcessManager {
public:
	void Initialize(int width, int height);
	void Shutdown(); 

	void BeginScene();
	void EndScene();
	void ApplyAndPresent();
	void Resize(int width, int height);

	// Set the actual screen framebuffer size (differs from virtual size on HiDPI/Retina displays)
	void SetDisplaySize(int w, int h) { m_displayWidth = w; m_displayHeight = h; }

	PostProcessSettings& Settings() { return m_settings; }
	const PostProcessSettings& Settings() const { return m_settings; }


private:
	void CreateSceneFBO();
	void CreateFullscreenQuad();

	PostProcessSettings m_settings{};

	unsigned int m_sceneFBO = 0;
	unsigned int m_sceneColorTex = 0;
	unsigned int m_sceneDepthRBO = 0;

	unsigned int m_quadVAO = 0;
	unsigned int m_quadVBO = 0;
	int m_width = 0;
	int m_height = 0;
	int m_displayWidth = 0;
	int m_displayHeight = 0;

	std::unique_ptr<Shader> m_postShader;
};