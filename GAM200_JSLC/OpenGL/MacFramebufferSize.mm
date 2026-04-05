#import <Cocoa/Cocoa.h>

#include <cmath>

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "MacFramebufferSize.h"

int MacQueryFramebufferBackingSize(GLFWwindow* window, int* outWidth, int* outHeight)
{
    if (!window || !outWidth || !outHeight)
        return 0;

    NSWindow* nsWindow = (NSWindow*)glfwGetCocoaWindow(window);
    if (!nsWindow)
        return 0;

    NSView* view = [nsWindow contentView];
    if (!view)
        return 0;

    NSRect backing = [view convertRectToBacking:[view bounds]];
    const int w = static_cast<int>(std::lround(backing.size.width));
    const int h = static_cast<int>(std::lround(backing.size.height));
    if (w < 1 || h < 1)
        return 0;

    *outWidth = w;
    *outHeight = h;
    return 1;
}
