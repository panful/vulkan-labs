/**
 * 1. 单独一个窗口
 * 2. 多个窗口一个线程
 * 3. Viewer
 * 4. 多个窗口多个线程
 *
 */

#define TEST1

#ifdef TEST1

#include "Actor.h"
#include "Device.h"
#include "View.h"
#include "Viewer.h"
#include "Window.h"
#include <iostream>
#include <memory>

int main()
{
    auto window = std::make_unique<Window>("test", vk::Extent2D {800, 600});
    auto actor  = std::make_shared<Actor>();
    auto view   = std::make_shared<View>();
    view->SetViewport({.05, .05, .9, .9});
    view->SetBackground({.3f, .2f, .1f, 1.f});
    view->AddActor(actor);

    window->AddView(view);
    window->Run();

    std::cout << "Success\n";
    return 0;
}

#endif // TEST1

#ifdef TEST2

#include "Actor.h"
#include "Device.h"
#include "View.h"
#include "Viewer.h"
#include "Window.h"
#include <iostream>
#include <memory>

int main()
{
    auto window1 = std::make_unique<Window>("test", vk::Extent2D {800, 600});
    auto window2 = std::make_unique<Window>(window1->GetDevice(), "test2", vk::Extent2D {800, 600});

    auto actor = std::make_shared<Actor>();
    auto view  = std::make_shared<View>();
    view->SetViewport({.1, .1, .3, .3});
    view->SetBackground({.3f, .2f, .1f, 1.f});
    view->AddActor(actor);

    window1->AddView(view);
    window2->AddView(view);

    window1->Run();
    window2->Run();

    std::cout << "Success\n";
    return 0;
}

#endif // TEST2

#ifdef TEST3

#include "Actor.h"
#include "Device.h"
#include "View.h"
#include "Viewer.h"
#include "Window.h"
#include <iostream>
#include <memory>

int main()
{
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

    std::cout << "Success\n";
    return 0;
}

#endif // TEST3

#ifdef TEST4

#include "Actor.h"
#include "Device.h"
#include "View.h"
#include "Viewer.h"
#include "Window.h"
#include <iostream>
#include <memory>
#include <thread>

int main()
{
    std::thread t1([]() {
        auto window = std::make_unique<Window>("test", vk::Extent2D {800, 600});
        auto actor  = std::make_shared<Actor>();
        auto view   = std::make_shared<View>();

        view->SetViewport({.1, .1, .3, .3});
        view->SetBackground({.3f, .2f, .1f, 1.f});
        view->AddActor(actor);

        window->AddView(view);
        window->Run();
    });

    // 不能在不同的线程中同时使用 vkQueueSubmit // TODO: 暂不支持多线程共享 Device

    std::thread t2([]() {
        auto window = std::make_unique<Window>("test2", vk::Extent2D {800, 600});
        auto actor  = std::make_shared<Actor>();
        auto view   = std::make_shared<View>();

        view->SetViewport({.1, .1, .3, .3});
        view->SetBackground({.3f, .2f, .1f, 1.f});
        view->AddActor(actor);

        window->AddView(view);
        window->Run();
    });

    t1.join();
    t2.join();

    std::cout << "Success\n";
    return 0;
}

#endif // TEST4
