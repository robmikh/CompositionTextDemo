#include "pch.h"
#include "MainWindow.h"

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Foundation::Numerics;
    using namespace Windows::Graphics;
    using namespace Windows::Graphics::DirectX;
    using namespace Windows::UI;
    using namespace Windows::UI::Composition;
}

namespace util
{
    using namespace robmikh::common::uwp;
    using namespace robmikh::common::desktop;
}

template <typename T>
winrt::com_ptr<T> CreateDWriteFactory(DWRITE_FACTORY_TYPE factoryType);

int __stdcall WinMain(HINSTANCE, HINSTANCE, PSTR, int)
{
    // Initialize COM
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    // Create the DispatcherQueue that the compositor needs to run
    auto controller = util::CreateDispatcherQueueControllerForCurrentThread();

    // Create our window and visual tree
    auto window = MainWindow(L"CompositionTextDemo", 800, 600);
    auto compositor = winrt::Compositor();
    auto target = window.CreateWindowTarget(compositor);
    auto root = compositor.CreateSpriteVisual();
    root.RelativeSizeAdjustment({ 1.0f, 1.0f });
    root.Brush(compositor.CreateColorBrush(winrt::Colors::White()));
    target.Root(root);

    // Options
    bool dxDebug = false;
#ifdef _DEBUG
    dxDebug = true;
#endif

    // Init D3D and D2D
    uint32_t flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if (dxDebug)
    {
        flags |= D3D11_CREATE_DEVICE_DEBUG;
    }
    auto d3dDevice = util::CreateD3DDevice(flags);
    auto debugLevel = D2D1_DEBUG_LEVEL_NONE;
    if (dxDebug)
    {
        debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
    }
    auto d2dFactory = util::CreateD2DFactory(debugLevel);
    auto d2dDevice = util::CreateD2DDevice(d2dFactory, d3dDevice);
    auto compGraphics = util::CreateCompositionGraphicsDevice(compositor, d2dDevice.get());

    // Init DWrite
    auto dwriteFactory = CreateDWriteFactory<IDWriteFactory>(DWRITE_FACTORY_TYPE_SHARED);
    winrt::com_ptr<IDWriteFontCollection> fontCollection;
    winrt::check_hresult(dwriteFactory->GetSystemFontCollection(fontCollection.put()));
    winrt::com_ptr<IDWriteTextFormat> textFormat;
    winrt::check_hresult(dwriteFactory->CreateTextFormat(
        L"Comic Sans MS",
        fontCollection.get(), 
        DWRITE_FONT_WEIGHT_NORMAL, 
        DWRITE_FONT_STYLE_NORMAL, 
        DWRITE_FONT_STRETCH_NORMAL, 
        36.0f, 
        L"en-us", 
        textFormat.put()));

    std::wstring text(L"Hello, World!");
    winrt::com_ptr<IDWriteTextLayout> textLayout;
    winrt::check_hresult(dwriteFactory->CreateTextLayout(text.data(), text.size(), textFormat.get(), 400.0f, 0.0f, textLayout.put()));
    DWRITE_OVERHANG_METRICS metrics = {};
    winrt::check_hresult(textLayout->GetOverhangMetrics(&metrics));
    auto maxWidth = textLayout->GetMaxWidth();
    auto maxHeight = textLayout->GetMaxHeight();

    RECT textRect =
    {
        0,
        0,
        metrics.right + maxWidth + -metrics.left,
        metrics.bottom + maxHeight + -metrics.top,
    };
    winrt::SizeInt32 textSize = { textRect.right - textRect.left, textRect.bottom - textRect.top };

    // Create a visual for our text
    auto visual = compositor.CreateSpriteVisual();
    visual.AnchorPoint({ 0.5f, 0.5f });
    visual.RelativeOffsetAdjustment({ 0.5f, 0.5f, 0.0f });
    visual.Size({ static_cast<float>(textSize.Width), static_cast<float>(textSize.Height) });
    auto brush = compositor.CreateSurfaceBrush();
    root.Children().InsertAtTop(visual);

    // Setup our basic brush
    auto surface = compGraphics.CreateDrawingSurface2(textSize, winrt::DirectXPixelFormat::A8UIntNormalized, winrt::DirectXAlphaMode::Premultiplied);
    auto basicBrush = compositor.CreateSurfaceBrush(surface);
    {
        auto surfaceContext = util::SurfaceContext(surface);
        auto d2dContext = surfaceContext.GetDeviceContext();
    
        winrt::com_ptr<ID2D1SolidColorBrush> d2dBrush;
        winrt::check_hresult(d2dContext->CreateSolidColorBrush({ 0.0f, 0.0f, 0.0f, 1.0f }, d2dBrush.put()));
        
        d2dContext->Clear({ 0.0f, 0.0f, 0.0f, 0.0f });
        d2dContext->DrawTextLayout({ 0, 0 }, textLayout.get(), d2dBrush.get());
    }

    // Create our mask brush
    auto maskBrush = compositor.CreateMaskBrush();
    auto textColorBrush = compositor.CreateColorBrush(winrt::Color{ 255, 255, 0, 0 });
    maskBrush.Source(textColorBrush);
    maskBrush.Mask(basicBrush);
    visual.Brush(maskBrush);

    // Animate our text color
    auto animation = compositor.CreateColorKeyFrameAnimation();
    animation.InsertKeyFrame(0.0f, winrt::Color{ 255, 255, 0, 0 });
    animation.InsertKeyFrame(0.5f, winrt::Color{ 255, 0, 0, 255 });
    animation.InsertKeyFrame(0.25f, winrt::Color{ 255, 0, 255, 0 });
    animation.InsertKeyFrame(0.75f, winrt::Color{ 255, 255, 255, 0 });
    animation.InsertKeyFrame(1.0f, winrt::Color{ 255, 255, 0, 0 });
    animation.Duration(std::chrono::seconds(3));
    animation.IterationBehavior(winrt::AnimationIterationBehavior::Forever);
    textColorBrush.StartAnimation(L"Color", animation);

    // Add a border for debugging
    auto border = compositor.CreateSpriteVisual();
    float borderSize = 2.0f;
    border.Size({ borderSize * 2.0f, borderSize * 2.0f });
    border.RelativeSizeAdjustment({ 1.0f, 1.0f });
    border.Offset({ -borderSize, -borderSize, 0.0f });
    auto borderBrush = compositor.CreateNineGridBrush();
    border.Brush(borderBrush);
    borderBrush.SetInsets(borderSize);
    borderBrush.IsCenterHollow(true);
    borderBrush.Source(compositor.CreateColorBrush(winrt::Color{ 255, 255, 0, 0 }));
    visual.Children().InsertAtTop(border);

    // Message pump
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return util::ShutdownDispatcherQueueControllerAndWait(controller, static_cast<int>(msg.wParam));
}

template <typename T>
winrt::com_ptr<T> CreateDWriteFactory(DWRITE_FACTORY_TYPE factoryType)
{
    winrt::com_ptr<::IUnknown> unknown;
    winrt::check_hresult(DWriteCreateFactory(factoryType, winrt::guid_of<T>(), unknown.put()));
    return unknown.as<T>();
}
