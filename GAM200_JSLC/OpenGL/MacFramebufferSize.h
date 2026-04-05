#pragma once

struct GLFWwindow;

#if defined(__APPLE__)
/** macOS: backing-store pixels for the window content view (Retina-safe). Returns 1 on success. */
int MacQueryFramebufferBackingSize(GLFWwindow* window, int* outWidth, int* outHeight);
#else
inline int MacQueryFramebufferBackingSize(GLFWwindow*, int*, int*)
{
    return 0;
}
#endif
