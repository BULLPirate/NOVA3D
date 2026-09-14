#include <Nova/Platform/Window.h>

#import <AppKit/AppKit.h>

namespace Nova {

void ActivateCocoaApp() {
    @autoreleasepool {
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp activateIgnoringOtherApps:YES];
    }
}

void ForceOrderFrontCocoaWindow(void* nsWindow) {
    @autoreleasepool {
        NSWindow* window = (__bridge NSWindow*)nsWindow;
        if (!window) {
            return;
        }

        [window setCollectionBehavior:NSWindowCollectionBehaviorMoveToActiveSpace |
                                      NSWindowCollectionBehaviorFullScreenAuxiliary];
        [window setLevel:NSFloatingWindowLevel];
        [window setAlphaValue:1.0];
        [window setIsVisible:YES];
        [window center];
        [window orderFrontRegardless];
        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
    }
}

} // namespace Nova
