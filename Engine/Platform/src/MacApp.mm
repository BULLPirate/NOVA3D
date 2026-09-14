#include <Nova/Platform/MacApp.h>

#import <AppKit/AppKit.h>

namespace Nova {

void EnsureMacOSApplicationReady() {
    @autoreleasepool {
        if (NSApp == nil) {
            [NSApplication sharedApplication];
        }
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        if (![NSApp isRunning]) {
            [NSApp finishLaunching];
        }
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
