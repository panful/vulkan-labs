
#include "Actor.h"
#include "Device.h"
#include "View.h"
#include "Viewer.h"
#include "Window.h"
#include <iostream>
#include <memory>

#define USE_WINDOW 1

int main()
{
#if (USE_WINDOW)
    Window w1("test", vk::Extent2D {800, 600});
    // Window w2;

    auto actor = std::make_shared<Actor>();
    auto view  = std::make_shared<View>();
    view->SetViewport({.1, .1, .3, .3});
    view->SetBackground({.3f, .2f, .1f, 1.f});
    view->AddActor(actor);

    auto view2 = std::make_shared<View>();
    view2->SetViewport({.6, .6, .3, .3});
    view2->SetBackground({.3f, .3f, .3f, 1.f});
    view2->AddActor(actor);

    w1.viewer->AddView(view);
    w1.viewer->AddView(view2);
    w1.Run();
    // w2.Run();

    // XXX: w1已经将 glfw 卸载，w2才销毁窗口
#else
    auto viewer = std::make_unique<Viewer>();

    auto actor = std::make_shared<Actor>();
    auto view  = std::make_shared<View>();
    view->SetViewport({.1, .1, .3, .3});
    view->SetBackground({.3f, .2f, .1f, 1.f});
    view->AddActor(actor);

    auto view2 = std::make_shared<View>();
    view2->SetViewport({.6, .6, .3, .3});
    view2->AddActor(actor);

    viewer->AddView(view);
    viewer->AddView(view2);

    for (auto i = 0u; i < 10; ++i)
    {
        viewer->Render();
    }
    viewer->ResizeFramebuffer({399, 431});
    for (auto i = 0u; i < 10; ++i)
    {
        viewer->Render();
    }
#endif

    std::cout << "Success\n";
    return 0;
}
